--ModelQuery建立的类型，self.__m是model定义

--[[
	where的condType可以取以下值：
	0: none
	1: and
	2: or
	3: xor
	4: not
	
	__m指向model原型
]]


--处理where条件的值，与field字段的配置类型做比对，然后根据是否有左右引号来决定是否要做反斜杠处理
local booleanValids = { TRUE = '1', ['true'] = '1', FALSE = '0', ['false'] = '0' }
local processWhereValue = function(field, value)
	local tp = type(value)
	value = tostring(value)

	local l, quoted = #value, false
	if value:byte(1) == 39 and value:byte(l) == 39 then
		quoted = true
	end
	
	if field.type == 1 then
		--字符串/Binary型的字段
		if l == 0 then
			return "''"
		end
		if not quoted then
			return ngx.quote_sql_str(value)
		end
		return value
	end
	
	if field.type == 2 then
		--整数型的字段
		if quoted then
			l = l - 2
		end
		if l <= field.maxlen then
			return quoted and value:sub(2, l + 1) or value
		end
		return nil
	end
	
	if field.type == 3 then
		--小数型的字段
		if quoted then
			l = l - 2
		end
		return quoted and value:sub(2, l + 1) or value
	end
	
	--布尔型的字段
	if quoted then
		value = value:sub(2, l - 1)
	end
	return booleanValids[value]
end

--添加一个where条件
local addWhere = function(self, condType, name, value)
	local fields, valok = self.__m.__fields, false
	
	if value then
		local f = fields[name]
		if f then
			value = processWhereValue(f, value)
			if value ~= nil then valok = true end
		end
	else
		valok = true
	end
	
	if not valok then
		return false
	end
	
	self.condString = nil	--condString和condValues同一时间只会存在一个，只有一个是可以有效的
	if not self.condValues then
		self.condValues = {}
	end

	self.condValues[#self.condValues + 1] = { n = name, v = value, c = condType }
	return true
end

local processWhere = function(self, condType, k, v)
	local tp = type(k)
	if tp == 'string' then
		addWhere(self, condType, k, v)

	elseif tp == 'table' then
		if #k > 0 then
			for i = 1, #k do
				if addWhere(self, condType, k[i]) == false then
					error("process where(%s) function call failed: illegal value or confilict with declaration of model fields", k[i])
				end
			end	
		else
			for name,val in pairs(k) do
				if addWhere(self, condType, name, val) == false then
					error("process where(%s) function call failed: illegal value or confilict with declaration of model fields", name)
				end
			end
		end
	end

	return self
end

--根据已经添加的where条件绑定值
local bindValue = function(self, fields, name, value)
	local cv = self.condValues
	local field = fields[name]

	if field and cv and value then
		value = processWhereValue(field, value)
		if value ~= nil then
			for i = 1, #cv do
				if cv[i].n == name then
					cv[i].v = value
				end
			end
			
			return true
		end
	end
	
	return false
end

--将query设置的条件合并为SQL语句
local queryexecuter = { conds = { '', 'AND ', 'OR ', 'XOR ', 'NOT ' } }

queryexecuter.SELECT = function(self, model)
	local sqls = {}
	sqls[#sqls + 1] = 'SELECT'
	
	--main fields
	queryexecuter.buildColumns(self, model, sqls)
	
	--joins fields
	queryexecuter.buildJoinsCols(self, sqls)
	
	--from
	sqls[#sqls + 1] = 'FROM'
	sqls[#sqls + 1] = model.__name
	sqls[#sqls + 1] = 'A'

	--joins conditions
	queryexecuter.buildJoinsConds(self, sqls)
	
	--where
	queryexecuter.buildWheres(self, sqls, 'WHERE', 'A.')
	
	--order by
	if self.orderBy then
		sqls[#sqls + 1] = string.format('ORDER BY %s %s', self.orderBy.name, self.orderBy.order)
	end
	--limit
	queryexecuter.buildLimits(self, sqls)
	
	--end
	return table.concat(sqls, ' ')
end
	
queryexecuter.UPDATE = function(self, model)
	local sqls = {}
	sqls[#sqls + 1] = 'UPDATE'
	sqls[#sqls + 1] = model.__name
	
	--all values
	if queryexecuter.buildKeyValuesSet(self, model, sqls) > 0 then
		table.insert(sqls, #sqls, 'SET')
	end
	
	--where
	queryexecuter.buildWheres(self, sqls, 'WHERE')
	
	--order by
	if self.orderBy then
		sqls[#sqls + 1] = string.format('ORDER BY %s %s', self.orderBy.name, self.orderBy.order)
	end
	--limit
	queryexecuter.buildLimits(self, sqls)
	
	--end
	return table.concat(sqls, ' ')
end
	
queryexecuter.INSERT = function(self, model)
	local sqls = {}
	sqls[#sqls + 1] = 'INSERT INTO'
	sqls[#sqls + 1] = model.__name
	
	--all values
	if queryexecuter.buildKeyValuesSet(self, model, sqls) > 0 then
		table.insert(sqls, #sqls, 'SET')
	end
	
	--end
	return table.concat(sqls, ' ')
end
	
queryexecuter.DELETE = function(self, model)
end


queryexecuter.buildColumns = function(self, model, sqls, alias, returnCols)
	local cols
	if self.colSelects then
		local plains = {}
		for k,v in pairs(self.colSelects) do
			plains[#plains + 1] = k
		end

		local s = table.concat(plains, ',A.')
		cols = #s > 0 and ('A.' .. s) or ''
	else
		cols = model.__fieldPlain
	end

	if #cols > 2 then
		if alias then
			cols = cols:gsub('A.', alias)
		end
		
		if returnCols == true then
			return cols
		end
		
		sqls[#sqls + 1] = cols
	end
end

queryexecuter.buildKeyValuesSet = function(self, model, sqls, alias)
	local fieldCfgs = model.__fields
	local vals, full = self.__vals, self.__full	
	local keyvals = {}

	if not vals then
		vals = self
	end

	for name,v in pairs(self.colSelects == nil and model.fields or self.colSelects) do
		local cfg = fieldCfgs[name]
		if cfg then
			local v, skip, quoteIt = vals[name], false, false

			if cfg.ai then
				if not full then
					v = nil
				end
			elseif v == nil then
				if cfg.null then
					v = 'NULL'
				elseif cfg.default then
					v = cfg.type == 1 and '' or '0'
				end
			elseif cfg.type == 1 then
				quoteIt = true
			end

			if not skip then
				if v ~= nil then
					if alias then
						name = alias .. name
					end

					v = tostring(v)
					keyvals[#keyvals + 1] = quoteIt and string.format("%s=%s", name, ngx.quote_sql_str(v)) or string.format("%s='%s'", name, v)
				end
			end
		end
	end

	sqls[#sqls + 1] = table.concat(keyvals, ',')
	return #keyvals
end

queryexecuter.buildWheres = function(self, sqls, condPre, alias)
	if self.condString then
		if condPre then
			sqls[#sqls + 1] = condPre
		end
		sqls[#sqls + 1] = self.condString
		return
	end

	if self.condValues and #self.condValues > 0 then
		local wheres, conds = {}, queryexecuter.conds
		
		if not alias then
			alias = ''
		end
		
		for i = 1, #self.condValues do
			local one = self.condValues[i]
			if one.v then
				wheres[#wheres + 1] = string.format("%s%s%s=%s", conds[one.c], alias, one.n, one.v)
			else
				wheres[#wheres + 1] = string.format("%s%s", conds[one.c], one.n)
			end
		end
		
		if condPre then
			sqls[#sqls + 1] = condPre
		end
		sqls[#sqls + 1] = table.concat(wheres, ' ')
	end
end

queryexecuter.buildJoinsCols = function(self, sqls, indient)
	local cc = self.joins == nil and 0 or #self.joins
	if cc < 1 then
		return
	end
	
	local validJoins = { inner = 'INNER JOIN', left = 'LEFT JOIN' }
	
	if indient == nil then
		indient = 1
	end
	self.joinIndient = indient
	
	for i = 1, cc do
		local q = self.joins[i].q
		
		indient = indient + 1
		q.joinIndient = indient
		
		local cols = queryexecuter.buildColumns(q, q.__m, sqls, string.format('%s.', string.char(65 + indient)), true)
		if cols then
			sqls[#sqls + 1] = ','
			sqls[#sqls + 1] = cols
		end
	end
end

queryexecuter.buildJoinsConds = function(self, sqls)
	local cc = self.joins == nil and 0 or #self.joins
	if cc < 1 then
		return
	end
	
	local validJoins = { inner = 'INNER JOIN', left = 'LEFT JOIN', right = 'RIGHT JOIN' }
	
	for i = 1, cc do
		local join = self.joins[i]
		local q = join.q
		local alias = string.char(65 + q.joinIndient)
		
		sqls[#sqls + 1] = validJoins[join.type]
		sqls[#sqls + 1] = q.query.__m.__name
		sqls[#sqls + 1] = alias
		sqls[#sqls + 1] = 'ON('
		queryexecuter.buildWheres(self, sqls, alias .. '.')
		sqls[#sqls + 1] = ')'
	end
end

queryexecuter.buildLimits = function(self, sqls)
	if self.limitTotal and self.limitTotal > 0 then
		sqls[#sqls + 1] = string.format('LIMIT %u,%u', self.limitStart, self.limitTotal)
	end
end


--query的原型类
local queryMeta = {
	__index = {
		--全部清空
		reset = function(self)
			local ignores = { __m = 1, op = 1 }
			for k,_ in pairs(self) do
				if not ignores[k] then
					self[k] = nil
				end
			end
			return self
		end,
		
		--设置条件
		where = function(self, name, val)
			return processWhere(self, 1, name, val)
		end,		
		andWhere = function(self, name, val)
			return processWhere(self, 2, name, val)
		end,
		orWhere = function(self, name, val)
			return processWhere(self, 3, name, val)
		end,
		xorWhere = function(self, name, val)
			return processWhere(self, 4, name, val)
		end,
		notWhere = function(self, name, val)
			return processWhere(self, 5, name, val)
		end,
		
		--设置只操作哪些列，如果不设置，则会操作model里的所有列
		columns = function(self, names)
			if not self.colSelects then
				self.colSelects = {}
			end
			
			local tp = type(names)
			if tp == 'string' then
				for str in string.gmatch(names, "([^,]+)") do
					self.colSelects[name] = true
				end
			elseif tp == 'table' then
				for i = 1, #names do
					self.colSelects[names[i]] = true
				end
			end
		end,
		--设置列的别名
		alias = function(self, names, alias)
		end,
		--使用列函数
		proc = function(self, name, val)
		end,
		
		--直接设置where条件语句
		conditions = function(self, conds)
			if type(conds) == 'string' then
				self.condString = conds
				self.condValues = nil
			end
		end,
		
		--在已经添加了条件的列上设置其值
		bind = function(self, name, values)
			local tp, failed = type(name), nil
			local fields = self.__m.__fields

			if tp == 'string' then
				if values ~= nil then
					if bindValue(self, fields, name, values) == false then
						failed = name
					end
				elseif bindValue(self, fields, string.split(name, '=', string.SPLIT_TRIM + 2)) == false then
					failed = name
				end							
				
			elseif tp == 'table' then
				if #name > 0 then
					for k = 1, #name do
						local t1, t2 = string.split(name[k], '=', string.SPLIT_TRIM + 2)
						if t2 then
							if bindValue(self, fields, t1, t2) == false then
								failed = t1
								break
							end
							
						elseif bindValue(self, fields, name[k]) == false then
							failed = name[k]
						end	
					end	
				else
					for k,v in pairs(name) do
						if bindValue(self, fields, k, v) == false then
							failed = k
							break
						end
					end
				end
			end
			
			if failed then
				error("process bind(%s) function call failed: illegal value or not exists field or confilict with declaration of model fields", failed)
			end

			return self
		end,
		
		--两个Query进行联接
		join = function(self, query, joinType)			
			local validJoins, jt = { inner = 1, left = 1, right = 1 }, joinType
			if jt == nil then jt = 'inner' end
			jt = validJoins[jt]
			if jt == nil then
				error("error join type '%s' for query join", tostring(joinType))
				return self
			end
			
			local j = { q = query, type = jt }
			if not self.joins then
				self.joins = j
			else
				self.joins[#self.joins + 1] = j
			end
			return self
		end,
		
		--设置排序
		order = function(self, field, asc)
			if not asc then
				field, asc = string.split(field, ' ', string.SPLIT_TRIM)
				if asc == nil then asc = 'asc' end
			end
			
			if field and self.__m.fields[field] and asc then
				asc = asc:lower()
				if asc == 'asc' or asc == 'desc' then
					self.orderBy = { name = field, order = asc }
				end
			end
			
			return self
		end,
		--限制数量
		limit = function(self, start, total)
			local tp = type(total)
			if tp == 'string' then
				total = tonumber(total)
			elseif total and tp ~= 'number' then
				return self
			end
			
			tp = type(start)
			if tp == 'string' then
				start = tonumber(start)
			elseif start and tp ~= 'number' then
				return self
			end
			
			if total == nil then
				self.limitStart = 0
				self.limitTotal = start
			else
				self.limitStart = start
				self.limitTotal = total
			end
			
			return self
		end,
		
		--执行并返回结果集，
		exec = function(self, db, result)
			if db then
				if type(db) == 'string' then
					db = self.__reeme(db)
				end
			else
				db = self.__reeme('maindb')
				if not db then 
					db = self.__reeme('mysqldb')
				end
			end			
			if not db then
				return nil
			end
			
			if result then
				self.__vals = result:getValues()
			end
			
			local model = self.__m
			local sqls = queryexecuter[self.op](self, model, db)
			ngx.say(sqls, '<br/>')
			result = require('reeme.odm.result')(result, model)
			local res = result:query(db, sqls, self.limitTotal or 10)
			
			self.__vals = nil
			if res then
				if self.op == 'SELECT' then
					return result
				end
				
				return { rows = res.affected_rows, insertid = res.insert_id }
			end
		end,
	}
}


--model的原型表，提供了所有的model功能函数
local modelMeta = {
	__index = {
		new = function(self, vals)
			local r = require('reeme.odm.result')(nil, self)
			if vals then
				for k,v in pairs(vals) do
					r[k] = v
				end
			end
			return r
		end,
		
		find = function(self, p1, p2)
			local q = { __m = self, __reeme = self.__reeme, op = 'SELECT' }
			setmetatable(q, queryMeta)
			
			if p1 then
				if type(p1) == 'number' then 
					q:limit(p1, p2)
				else
					q:where(p1, p2)
				end
			end
			
			return q:exec()
		end,		
		findFirst = function(self, name, val)
			local q = { __m = self, __reeme = self.__reeme, op = 'SELECT' }
			setmetatable(q, queryMeta)
			if name then where(q, name, val) end
			return q:limit(1):exec()
		end,
		
		query = function(self)
			local q = { __m = self, __reeme = self.__reeme, op = 'SELECT' }
			return setmetatable(q, queryMeta)
		end,
		
		update = function(self)
			local q = { __m = self, __reeme = self.__reeme, op = 'UPDATE' }
			return setmetatable(q, queryMeta)
		end,
		
		delete = function(self)
			local q = { __m = self, __reeme = self.__reeme, op = 'DELETE' }
			return setmetatable(q, queryMeta)
		end,
		deleteFirst = function(self)
			local q = { __m = self, __reeme = self.__reeme, op = 'DELETE' }
			return setmetatable(q, queryMeta):limit(1)
		end,
		
		getField = function(self, name)
			return self.__fields[name]
		end,
		getFieldType = function(self, name)
			local f = self.__fields[name]
			if f then
				local typeStrings = { 'string', 'integer', 'number', 'boolean' }
				return typeStrings[f.type]
			end
		end,
		isFieldNull = function(self, name)
			local f = self.__fields[name]
			if f then return f.null end
			return false
		end,
		isFieldAutoIncreasement = function(self, name)
			local f = self.__fields[name]
			if f then return f.ai end
			return false
		end,	
	}
}

--当mode:findByXXXX或者model:findFirstByXXXX被调用的时候，只要XXXX是一个合法的字段名称，就可以按照该字段进行索引。下面的两个meta table一个用于模拟
--函数调用，一个用于生成一个模拟器
local simFindFuncMeta = {
	__call = function(self, value)
		local func = modelMeta.__index[self.onlyFirst and 'find' or 'findFirst']		
		return func(self.model, self.field, value)
	end
}

local modelMeta2 = {
	__index = function(self, key)
		if type(key) == 'string' then
			local l, of, field = #key, false, nil
			
			if l > 7 and key:sub(1, 7) == 'findBy_' then
				field = key:sub(8)
				
			elseif l > 12 and key:sub(1, 12) == 'findFirstBy_' then
				field, of = key:sub(13), true
			end
			
			if field and self[field] then
				--字段存在
				local call = { model = self, field = field, onlyFirst = of }
				return setmetatable(call, simFindFuncMeta)
			end
		end
	end
}

setmetatable(modelMeta.__index, modelMeta2)

return modelMeta