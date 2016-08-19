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
local removeColFromExpWhen = { distinct = 1 }

local processWhereValue = function(self, field, value)
	local tp = type(value)
	
	if tp == 'table' then
		local tokens, poses = self.__reeme.orm.parseExpression(value[1])
		if not tokens then
			return nil
		end		
		return value, tokens, poses
	end
	
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

--添加一个where条件，本函数被processWhere调用
local addWhere = function(self, condType, name, value)
	local tokens, poses = nil, nil
	local fields, valok = self.__m.__fields, false

	self.condString = nil	--condString和condValues同一时间只会存在一个，只有一个是可以有效的
	if not self.condValues then
		self.condValues = {}
	end

	if value == nil then
		--name就是整个表达式
		if type(name) == 'string' then
			tokens, poses = self.__reeme.orm.parseExpression(name)
		end
		
		self.condValues[#self.condValues + 1] = { n = name, c = condType, tokens = tokens, poses = poses }
		
	elseif type(name) == 'string' then
		--key=value或key={value}这种表达式
		local f = fields[name]
		
		if f then
			value, tokens, poses = processWhereValue(self, f, value)
		else
			value = nil
		end
		
		if value then
			self.condValues[#self.condValues + 1] = { n = name, v = value, c = condType, tokens = tokens, poses = poses }
			return true
		end
		
	else
		--{value}这种表达式
		tokens, poses = self.__reeme.orm.parseExpression(value[1])
		self.condValues[#self.condValues + 1] = { n = name, v = value, c = condType, tokens = tokens, poses = poses }
		return true
	end
	
	return false
end

--处理where函数带来的条件
local processWhere = function(self, condType, k, v)
	local tp = type(k)
	if tp == 'string' then
		addWhere(self, condType, k, v)

	elseif tp == 'table' then
		for name,val in pairs(k) do
			if addWhere(self, condType, name, val) == false then
				error(string.format("process where(%s) function call failed: illegal value or confilict with declaration of model fields", name))
			end
		end
	end

	return self
end

--解析Where条件中的完整表达式，将表达式中用到的字段名字，按照表的alias名称来重新生成
local processWhereFullString = function(self, alias, src)
	local fields = self.__m.__fields
	local sql, tokens, poses = src.n, src.tokens, src.poses
	local adjust = 0

	if type(sql) == 'number' then
		sql = src.v[1]
	end
	if not tokens or not poses then
		return sql
	end
	
	for i=1, #tokens do
		local one, newone = tokens[i], nil

		if one:byte(1) == 39 then
			--这是一个字符串
			newone = ngx.quote_sql_str(one:sub(2, -2))
		elseif fields[one] then
			--这是一个字段的名称
			newone = alias .. one
		end

		if newone then
			--替换掉最终的表达式
			sql = sql:subreplace(newone, poses[i] + adjust, #one)
			adjust = adjust + #newone - #one
		end
	end
	
	return sql
end

--根据已经添加的where条件绑定值
local bindValue = function(self, fields, name, value)
	local cv = self.condValues
	local field = fields[name]
	local tokens, poses

	if field and cv and value then
		value, tokens, poses = processWhereValue(self, field, value)
		if value ~= nil then
			for i = 1, #cv do
				local cvi = cv[i]
				if cvi.n == name then
					cvi.v = value
					cvi.tokens = tokens
					cvi.poses = poses
				end
			end
			
			return true
		end
	end
	
	return false
end

--将query设置的条件合并为SQL语句
local queryexecuter = { conds = { '', 'AND ', 'OR ', 'XOR ', 'NOT ' }, validJoins = { inner = 'INNER JOIN', left = 'LEFT JOIN', right = 'RIGHT JOIN' } }

queryexecuter.SELECT = function(self, model)
	local sqls = {}
	sqls[#sqls + 1] = 'SELECT'
	
	--main
	self.alias = 'A'
	queryexecuter.buildColumns(self, model, sqls, self.alias .. '.')
	
	--joins fields
	queryexecuter.buildJoinsCols(self, sqls)
	
	--from
	sqls[#sqls + 1] = 'FROM'
	sqls[#sqls + 1] = model.__name
	sqls[#sqls + 1] = 'A'

	--joins conditions
	queryexecuter.buildJoinsConds(self, sqls)
	
	--where
	queryexecuter.buildWheres(self, sqls, 'WHERE', self.alias .. '.')
	queryexecuter.buildWhereJoins(self, sqls)
	
	--order by
	if self.orderBy then
		sqls[#sqls + 1] = string.format('ORDER BY %s.%s %s', self.alias, self.orderBy.name, self.orderBy.order)
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
	if not queryexecuter.buildWheres(self, sqls, 'WHERE') then
		--find primary or unique
		local idx, vals = model.__fieldIndices, self.__vals
		if vals then		
			for k,v in pairs(idx) do
				if (v.type == 1 or v.type == 2) and vals[k] then
					processWhere(self, 1, k, vals[k])				
					queryexecuter.buildWheres(self, sqls, 'WHERE')
					break
				end
			end
		end
	end
	
	--order by
	if self.orderBy then
		sqls[#sqls + 1] = string.format('ORDER BY %s %s', self.orderBy.name, self.orderBy.order)
	end
	--limit
	queryexecuter.buildLimits(self, sqls, true)
	
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
	--加入所有的表达式
	local excepts, express = nil, nil
	if self.expressions then
		local fields, func = self.__m.__fields, self.__reeme.orm.parseExpression
		
		for i=1, #self.expressions do
			local expr = self.expressions[i]
			
			if type(expr) == 'string' then
				local adjust = 0
				local tokens, poses = func(expr)
				if tokens then
					local removeCol = false
					for k=1,#tokens do
						local one, newone = tokens[k], nil

						if one:byte(1) == 39 then
							--这是一个字符串
							newone = ngx.quote_sql_str(one:sub(2, -2))				
						elseif fields[one] then
							--这是一个字段的名称
							if removeCol then
								if not excepts then
									excepts = {}
								end
								if self.colExcepts then
									for en,_ in pairs(self.colExcepts) do
										excepts[en] = true
									end
								end
								
								excepts[one] = true
							end
							
							newone = alias .. one
							
						elseif removeColFromExpWhen[one:lower()] then
							--遇到这些定义的表达式，这个表达式所关联的字段就不会再在字段列表中出现
							removeCol = true
						end

						if newone then
							expr = expr:subreplace(newone, poses[k] + adjust, #one)
							adjust = adjust + #newone - #one
						end
					end

					self.expressions[i] = expr
				end
			else
				self.expressions[i] = tostring(expr)
			end
		end
		
		express = table.concat(self.expressions, ',')
	end
	
	if not excepts then
		excepts = self.colExcepts
	end

	--如果imde指定只获取哪些列，那么就获取所有的列，当然，要去掉表达式中已经使用了的列
	local cols
	if self.colSelects then
		local plains = {}
		for k,v in pairs(self.colSelects) do
			if not excepts[k] then
				plains[#plains + 1] = k
			end
		end

		cols = table.concat(plains, ',' .. alias)		
	else
		local fieldPlain = model.__fieldsPlain
		if excepts then
			local fps = {}
			for i = 1, #fieldPlain do
				local n = fieldPlain[i]
				if not excepts[n] then
					fps[#fps + 1] = n
				end
			end
			fieldPlain = fps
		end
		
		cols = table.concat(fieldPlain, ',' .. alias)
	end
	
	cols = #cols > 0 and (alias .. cols) or ''
	if express then
		cols = #cols > 0 and string.format('%s,%s', express, cols) or express
	end
	
	if #cols > 2 then
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
			local v = vals[name]

			if cfg.ai then
				if not full or not string.checkinteger(v) then
					v = nil
				end
			elseif v == nil then
				if cfg.null then
					v = 'NULL'
				elseif cfg.default then
					v = cfg.type == 1 and "''" or '0'
				end
			elseif v == ngx.null then
				v = 'NULL'
			elseif cfg.type == 1 then
				v = ngx.quote_sql_str(v)
			elseif cfg.type == 3 then
				if not string.checknumeric(v) then
					v = nil
				end
			elseif not string.checkinteger(v) then
				v = nil
			end

			if v ~= nil then
				if alias then
					name = alias .. name
				end

				v = tostring(v)
				keyvals[#keyvals + 1] = string.format("%s=%s", name, v)
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
		return true
	end

	if self.condValues and #self.condValues > 0 then
		local wheres, conds = {}, queryexecuter.conds
		
		if not alias then
			alias = ''
		end
		
		for i = 1, #self.condValues do
			local one = self.condValues[i]
			local tp = type(one.v)
			
			if i > 1 and one.c == 1 then
				one.c = 2
			end

			if tp == 'table' then
				if type(one.n) == 'number' then
					wheres[#wheres + 1] = processWhereFullString(self, alias, one)
				else
					wheres[#wheres + 1] = string.format("%s%s%s %s", conds[one.c], alias, one.n, one.v[1])
				end
			elseif tp ~= 'nil' then
				wheres[#wheres + 1] = string.format("%s%s%s=%s", conds[one.c], alias, one.n, one.v)
			elseif one.tokens then
				wheres[#wheres + 1] = string.format("%s%s", conds[one.c], processWhereFullString(self, alias, one))
			else
				wheres[#wheres + 1] = string.format("%s%s", conds[one.c], one.n)
			end
		end
		
		if condPre then
			sqls[#sqls + 1] = condPre
		end
		sqls[#sqls + 1] = table.concat(wheres, ' ')
		
		return true
	end
	
	return false
end

queryexecuter.buildWhereJoins = function(self, sqls)
	local cc = self.joins == nil and 0 or #self.joins
	if cc < 1 then
		return
	end
	
	for i = 1, cc do
		local q = self.joins[i].q
		queryexecuter.buildWheres(q, sqls, 'AND', q.alias .. '.')
	end
end

queryexecuter.buildJoinsCols = function(self, sqls, indient)
	local cc = self.joins == nil and 0 or #self.joins
	if cc < 1 then
		return
	end
	
	if indient == nil then
		indient = 1
	end
	
	for i = 1, cc do
		local q = self.joins[i].q
		q.alias = string.char(65 + indient)

		local cols = queryexecuter.buildColumns(q, q.__m, sqls, q.alias .. '.', true)
		if cols then
			sqls[#sqls + 1] = ','
			sqls[#sqls + 1] = cols
		end
		
		local newIndient = queryexecuter.buildJoinsCols(q, sqls, indient + 1)
		indient = newIndient or (indient + 1)
	end
	
	return indient
end

queryexecuter.buildJoinsConds = function(self, sqls)
	local cc = self.joins == nil and 0 or #self.joins
	if cc < 1 then
		return
	end
	
	local validJoins = queryexecuter.validJoins
	
	for i = 1, cc do
		local join = self.joins[i]
		local q = join.q
		
		sqls[#sqls + 1] = validJoins[join.type]
		sqls[#sqls + 1] = q.__m.__name
		sqls[#sqls + 1] = q.alias
		sqls[#sqls + 1] = 'ON('
		if not queryexecuter.buildWheres(q, sqls, q.alias .. '.') then
			sqls[#sqls + 1] = '1'
		end
		sqls[#sqls + 1] = ')'
		
		queryexecuter.buildJoinsConds(q, sqls)
	end
end

queryexecuter.buildLimits = function(self, sqls, ignoreStart)
	if self.limitTotal and self.limitTotal > 0 then
		if ignoreStart then
			sqls[#sqls + 1] = string.format('LIMIT %u', self.limitTotal)
		else
			sqls[#sqls + 1] = string.format('LIMIT %u,%u', self.limitStart, self.limitTotal)
		end
	end
end


--query的原型类
local queryMeta = {
	__index = {
		--全部清空
		reset = function(self)
			local ignores = { __m = 1, op = 1, __reeme = 1 }
			for k,_ in pairs(self) do
				if not ignores[k] then
					self[k] = nil
				end
			end
			
			self.limitStart, self.limitTotal = 0, 50
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
				for str in names:gmatch("([^,]+)") do
					self.colSelects[str] = true
				end
			elseif tp == 'table' then
				for i = 1, #names do
					self.colSelects[names[i]] = true
				end
			end
			
			return self
		end,
		--设置只排除哪些列
		excepts = function(self, names)
			if not self.colExcepts then
				self.colExcepts = {}
			end
			
			local tp = type(names)
			if tp == 'string' then
				for str in names:gmatch("([^,]+)") do
					self.colExcepts[str:trim()] = true
				end
			elseif tp == 'table' then
				for i = 1, #names do
					self.colExcepts[names[i]] = true
				end
			end
			
			return self
		end,
		--设置列的别名
		alias = function(self, names, alias)
		end,
		--使用列表达式
		expr = function(self, expr)
			if not self.expressions then
				self.expressions = {}
			end
			
			self.expressions[#self.expressions + 1] = expr
			return self
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
				elseif bindValue(self, fields, name:split('=', string.SPLIT_TRIM + 2)) == false then
					failed = name
				end							
				
			elseif tp == 'table' then
				if #name > 0 then
					for k = 1, #name do
						local t1, t2 = name[k]:split('=', string.SPLIT_TRIM + 2)
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
			local validJoins = { inner = 1, left = 1, right = 1 }
			local jt = joinType == nil and 'inner' or joinType:lower()
			
			if validJoins[jt] == nil then
				error("error join type '%s' for join", tostring(joinType))
				return self
			end

			local j = { q = query, type = jt }
			if not self.joins then
				self.joins = { j }
			else
				self.joins[#self.joins + 1] = j
			end
			return self
		end,
		
		--设置排序
		order = function(self, field, asc)
			if not asc then
				field, asc = field:split(' ', string.SPLIT_TRIM)
				if asc == nil then asc = 'asc' end
			end
			
			if field and self.__m.fields[field] and asc then
				asc = asc:lower()
				if asc == 'asc' or asc == 'desc' then
					self.orderBy = { name = field, order = asc:upper() }
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
				self.__vals = result()
			end
			
			local model = self.__m
			local ormr = require('reeme.orm.result')
			local sqls = queryexecuter[self.op](self, model, db)

			result = ormr.init(result, model)
			local res = ormr.query(result, db, sqls, self.limitTotal or 10)

			self.__vals = nil
			if res then
				if self.op == 'SELECT' then
					return result + 1 and result or nil
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
			local r = require('reeme.orm.result').init(nil, self)
			if vals then
				for k,v in pairs(vals) do
					r[k] = v
				end
			end
			return r
		end,
		
		find = function(self, p1, p2, p3, p4)
			local q = { __m = self, __reeme = self.__reeme, op = 'SELECT' }
			setmetatable(q, queryMeta)
			
			if p1 then
				local tp = type(p1)
				if tp == 'number' then 
					q:limit(p1, p2)
				elseif tp == 'table' then
					q:where(p1)
					
					if type(p2) == 'number' then
						q:limit(p2, p3)
						p3, p4 = nil, nil
					end
				else
					q:where(p1, p2)
				end
			end
			
			if type(p3) == 'number' then
				q:limit(p3, p4)
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
			local q = { __m = self, __reeme = self.__reeme, op = 'SELECT', limitStart = 0, limitTotal = 50 }
			return setmetatable(q, queryMeta)
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
		findUniqueKey = function(self)
			local idx = self.__fieldIndices
			for k,v in pairs(idx) do
				if v.type == 1 or v.type == 2 then
					return k
				end
			end
		end,
	}
}

--当mode:findByXXXX或者model:findFirstByXXXX被调用的时候，只要XXXX是一个合法的字段名称，就可以按照该字段进行索引。下面的两个meta table一个用于模拟
--函数调用，一个用于生成一个模拟器
local simFindFuncMeta = {
	__call = function(self, value, p1, p2)
		local func = modelMeta.__index[self.onlyFirst and 'find' or 'findFirst']		
		return func(self.model, self.field, value, p1, p2)
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