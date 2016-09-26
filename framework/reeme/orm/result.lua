--[[
	结果集封装
	
	这样就可以得到一个结果集
	local r = reeme.orm.use('mytable'):query():exec()
	
	在r这个变量上可以执行下列操作：
	
	1、+ N 或 - N，可以控制结果集的游标向后或向前移动，如果移动成功，则返回true否则返回false。+1就相当于r:next()，-1就相当于r:prev()
		当然，实际上r类型并没有next和prev函数，所以只能使用+N和-N来完成
	while r + 1 do
	end
	
	2、r()，即将r当成一个函数一样的调用，可以返回当前行的原始记录的table。这种操作方式一般只在一种情况下需要使用：那就是当r中的函数名与
		结果集中的字段名冲突的时候，就可以采用这种方式越过r的metatable直接访问字段，就不用担心函数名称与字段名称冲突了。比如r中有一个函数
		名为saveTo，如果数据表中也有一个字段叫做saveTo(尽管这种可能性其实很小而且也不应该)，那么r().saveTo就可以操作这个字段了。
		
	   r(param) 这个调用还可以有参数，参数可以是true|false|table|string，当为false时，和没有参数是一样的，当为true时，将会一次性返回整个结
	   果集，那就是一个二维的table。当为table时，可以将该table中的所有的key=>value覆盖到当前的结果集中，相当于一次性设置结果集中当前行中列
	   的值。也可以是一个string，表示列名，函数会根据该列的数据类型自动的转换值类型之后再返回
		
	3、for k,v in pairs(r) do这样可以迭代出r当前行的所有字段，不会将r中的函数迭代出来
	   如果您的luajit在编译的时候没有加上过LUAJIT_ENABLE_LUA52COMPAT这个选项（参考：http://luajit.org/extensions.html拉到最后面基本上就可以看到）
	   那么要迭代出所有的选项就需要这样写：for k,v in pairs(r()) do。注意pairs(r)中的那个r打了括号，这就是上面一条说的()返回了结果集当前行
	
	4、#r可以得到结果集的总行数
	
	5、r = -r可以让结果集当前行归零，即游标回到结果集的第一行，然后又可以使用+N或-N来移动游标了。当然，归零的只限于游标，如果在迭代的过程中
		曾经修改过某一行的某个列的值，那么这个值是不会再被归零的了
	
	之所以对结果集类型使用了好几个算术运算符号，也没有采用函数来完成，目的是为了减少函数的数量。因为对结果集中字段的操作也是通过索引来完成的，
	如果结果集带的函数越多，那么名称冲突的可能性就越高。所以目前结果集这个类型中，一共也只有4个函数save、fullSave、create、fullCreate
]]

local execModelInstance = function(self, db, op, limit, full)
	local m = rawget(self, -10000)
	if m then
		local q = m:query()

		q.op = op
		q.fullop = full

		if limit then 
			q:limit(1) 
		end
		return q:exec(db, self)
	end
end

local resultMeta = {}

local getRowsLen = function(self)
	return rawget(self, -10001) or 0
end
local getRowPairs = function(self)
	local vals = getmetatable(self).__index	
	return function(t, k)
		return next(vals, k)
	end
end
--参数tbl可以是一个以key=val的table，也可以是一个布尔值，true的时候返回所有行，false返回当前行，也可以为Nil则效果同false
local callRowTable = function(self, tbl)
	if not tbl then
		return getmetatable(self).__index
	end	
	if tbl == true then
		return rawget(self, -10003)
	end
	
	local tp = type(tbl)
	if tp == 'string' then
		local m = rawget(self, -10000)
		local f = m.__fields[tbl]
		
		if f then
			local v = getmetatable(self).__index[tbl]

			if f.type == 1 then
				return f.isDate and require('reeme.orm.datetime')(v) or v

			elseif f.type == 2 then
				return (f.maxlen > 11 and type(v) == 'string') and string.int64(v) or tonumber(v)

			elseif f.type == 3 then
				return tonumber(v)
				
			elseif f.type == 4 then
				return toboolean(v)
			end
		end
		
	elseif tp == 'table' then
		local vals = getmetatable(self).__index
		for k,v in pairs(tbl) do
			vals[k] = v
		end
	end
	
	return self
end
local setResultRow = function(self, rowId)
	local total = rawget(self, -10001)
	if not total or rowId < 1 or rowId > total then
		return false
	end
	
	local rows = rawget(self, -10003)
	local meta = getmetatable(self)
	
	meta.__index, meta.__newindex = rows[rowId], rows[rowId]

	setmetatable(meta.__index, resultMeta)
	
	return true
end
local addOffsetRow = function(self, cc)
	local curr = rawget(self, -10002)
	if setResultRow(self, curr + cc) then
		rawset(self, -10002, curr + cc)
		return true
	end
	return false
end
local subOffsetRow = function(self, cc)
	local curr = rawget(self, -10002)
	if setResultRow(self, curr - cc) then
		rawset(self, -10002, curr - cc)
		return true
	end
	return false
end
local resetRow = function(self)
	setResultRow(self, 1)
	rawset(self, -10002, 1)
	
	return self
end

local pub = {
	init = function(r, m)
		if r == nil then
			local rVals = {}
			local valsMeta = {
				__index = rVals, 
				__newindex = rVals, 
				__len = getRowsLen, 
				__pairs = getRowPairs, 
				__call = callRowTable, 
				__add = addOffsetRow, 
				__sub = subOffsetRow,
				__unm = resetRow
			}
			
			setmetatable(rVals, resultMeta)
			r = setmetatable({}, valsMeta)

		else
			local mt = getmetatable(r)
			if mt ~= resultMeta and (not mt or not mt.__index or getmetatable(mt.__index) ~= resultMeta) then
				error('Result.init function called by a table without result meta type')
			end
		end
		
		rawset(r, -10000, m)
		return r
	end,
	query = function(self, db, sql, maxrows)
		local res, err, code = db:query(sql, maxrows)
		if res then
			rawset(self, -10001, #res)
			rawset(self, -10002, 0)
			rawset(self, -10003, res)
			return res
		end

		if err then
			error(string.format("ORM Query execute failed:<br/>&nbsp;&nbsp;Sql = %s<br/>&nbsp;&nbsp;Error = %s", sql, err))
		end

		return false
	end,
	wrap = function(self, result)
		rawset(self, -10001, #result)
		rawset(self, -10002, 1)
		rawset(self, -10003, result)
		
		if setResultRow(self, 1) then
			return self
		end
	end
}

resultMeta.__index = {
	save = function(self, db)
		return execModelInstance(self, db or self.__db, 'UPDATE', true, false)
	end,
	fullSave = function(self, db)
		return execModelInstance(self, db or self.__db, 'UPDATE', true, true)
	end,
	create = function(self, db)
		local r = execModelInstance(self, db or self.__db, 'INSERT', false, false)
		if r then
			local ai = rawget(self, -10000):findAutoIncreasementField()
			if ai then
				self[ai.colname] = r.insertid
			end
		end
		return r
	end,
	fullCreate = function(self, db)
		return execModelInstance(self, db or self.__db, 'INSERT', false, true)
	end,
	delete = function(self, db)
		return execModelInstance(self, db or self.__db, 'DELETE', true, false)
	end,
	
	getModel = function(self)
		return rawget(self, -10000)
	end,
	
	clone = function(self, fieldnames, newvals)
		local m = rawget(self, -10000)
		if m then
			local r, vt = nil, type(newvals)
			if vt == 'table' then
				r = m:new(self, fieldnames, newvals)
			elseif vt == 'function' then
				r = newvals(m:new(self, fieldnames))
			end
			return r
		end
	end
}


return pub