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

local _parseExpression = findmetatable('REEME_C_EXTLIB').sql_expression_parse


--处理where条件的值，与field字段的配置类型做比对，然后根据是否有左右引号来决定是否要做反斜杠处理
local booleanValids = { TRUE = '1', ['true'] = '1', FALSE = '0', ['false'] = '0' }
local removeColFromExpWhen = { distinct = 1 }

local processWhereValue = function(self, field, value)
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

--解析一个where条件，本函数被processWhere调用
local parseWhere = function(self, condType, name, value)
	self.condString = nil	--condString和condValues同一时间只会存在一个，只有一个是可以有效的
	if not self.condValues then
		self.condValues = {}
	end

	name = name:trim()
	
	if value == nil then
		--name就是整个表达式
		return { expr = name, c = condType }
	end
	
	puredkeyname = name:match('^[0-9A-Za-z-_]+')
	if puredkeyname and #puredkeyname == #name then
		--key没有多余的符号，只是一个纯粹的列名
		puredkeyname = true
	else
		puredkeyname = false
	end
	
	local tv = type(value)
	if tv == 'table' then
		if not getmetatable(value) then
			--{value}这种表达式
			assert(#value > 0)
			return { expr = puredkeyname and string.format('%s=%s', name, value[1]) or value[1], c = condType }
		end
		
		--子查询
		return { puredkeyname = puredkeyname, n = name, sub = value, c = condType }
	end

	if type(name) == 'string' then
		--key=value
		if tv == 'string' then
			value = ngx.quote_sql_str(value)
			
		elseif puredkeyname then
			local f = self.__m.__fields[name]
			if f and f.type == 1 then
				--如果是字段是字符串类型，则自动转字符串
				value = ngx.quote_sql_str(tostring(value))
			end
		end
		
		return { expr = puredkeyname and string.format('%s=%s', name, value) or (name .. value), c = condType }
	end
end

--处理where函数带来的条件
local processWhere = function(self, condType, k, v)
	local tp = type(k)
	if tp == 'table' then
		for name,val in pairs(k) do
			local where = parseWhere(self, condType, name, val)
			if where then
				self.condValues[#self.condValues + 1] = where
			else
				error(string.format("process where(%s) function with illegal value(s) call", name))
			end
		end
		return self
	end
	
	if tp ~= 'string' then
		k = tostring(k)
	end

	local where = parseWhere(self, condType, k, v)
	if where then
		self.condValues[#self.condValues + 1] = where
	else
		error(string.format("process where(%s) function with illegal value(s) call", name))
	end
	return self
end

--处理on函数带来的条件
local processOn = function(self, condType, k, v)
	local tp = type(k)
	if tp == 'string' then
		local where = parseWhere(self, condType, k, v)
		if where then
			if not self.onValues then
				self.onValues = { where }
			else
				self.onValues[#self.onValues + 1] = where
			end			
		else
			error(string.format("process on(%s) function call failed: illegal value or confilict with declaration of model fields", name))
		end

	elseif tp == 'table' then
		for name,val in pairs(k) do
			local where = parseWhere(self, condType, name, val)
			if where then
				if not self.onValues then
					self.onValues = { where }
				else
					self.onValues[#self.onValues + 1] = where
				end
			else
				error(string.format("process on(%s) function call failed: illegal value or confilict with declaration of model fields", name))
			end
		end
	end

	return self
end

--解析Where条件中的完整表达式，将表达式中用到的字段名字，按照表的alias名称来重新生成
local processTokenedString = function(self, alias, expr, joinFrom)
	if #alias == 0 then
		return expr
	end

	local fields = self.__m.__fields
	local sql, adjust = expr, 0
	local n1, n2 = self.__m.__name, joinFrom and joinFrom.__m.__name or nil
	local names = self.joinNames
	
	local tokens, poses = _parseExpression(sql)
	if not tokens or not poses then
		return sql
	end

	local drops = 0
	for i=1, #tokens do
		local one, newone = tokens[i], nil
		if one then
			if fields[one] then
				--这是一个字段的名称
				newone = drops > 0 and one or alias .. one
			elseif one == n1 then
				newone = ''
				one = one .. '.'
			elseif one == n2 then
				newone = joinFrom.alias
				drops = 2
			elseif names then
				local q = names[one]
				newone = q and self.joinNames[one].alias or nil
			end
		end
		
		if newone then
			--替换掉最终的表达式
			sql = sql:subreplace(newone, poses[i] + adjust, #one)
			adjust = adjust + #newone - #one
		end
		
		drops = drops - 1
	end
	
	return sql
end

--将query设置的条件合并为SQL语句
local queryexecuter = { conds = { '', 'AND ', 'OR ', 'XOR ', 'NOT ' }, validJoins = { inner = 'INNER JOIN', left = 'LEFT JOIN', right = 'RIGHT JOIN', full = 'FULL JOIN' } }

queryexecuter.SELECT = function(self, model, db)
	local sqls = {}
	sqls[#sqls + 1] = 'SELECT'
	
	--main
	local alias = ''
	self.db = db
	if self.joins and #self.joins > 0 then
		self.alias = self.userAlias or '_A'
		alias = self.alias .. '.'
	end	
	
	queryexecuter.buildColumns(self, model, sqls, alias)
	
	--joins fields
	queryexecuter.buildJoinsCols(self, sqls)
	
	--from
	sqls[#sqls + 1] = 'FROM'
	sqls[#sqls + 1] = model.__name
	if #alias > 0 then
		sqls[#sqls + 1] = self.userAlias or self.alias
	end

	--joins conditions	
	queryexecuter.buildJoinsConds(self, sqls)
	
	--where
	local haveWheres = queryexecuter.buildWheres(self, sqls, 'WHERE', alias)
	queryexecuter.buildWhereJoins(self, sqls, haveWheres)
	
	--order by
	queryexecuter.buildOrder(self, sqls, alias)
	--limit
	queryexecuter.buildLimits(self, sqls)
	
	--end
	self.db = nil
	return table.concat(sqls, ' ')
end
	
queryexecuter.UPDATE = function(self, model, db)
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
		local haveWheres = false
		local idx, vals = model.__fieldIndices, self.__vals
		if vals then
			for k,v in pairs(idx) do
				if (v.type == 1 or v.type == 2) and vals[k] then
					processWhere(self, 1, k, vals[k])
					haveWheres = queryexecuter.buildWheres(self, sqls, 'WHERE')
					break
				end
			end
		end

		if not haveWheres then
			error("Cannot save a model without any conditions")
			return false
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

queryexecuter.INSERT = function(self, model, db)
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
		local fields = self.__m.__fields
		local tbname = self.__m.__name
		
		for i=1, #self.expressions do
			local expr = self.expressions[i]
			
			if type(expr) == 'string' then
				local adjust = 0
				local tokens, poses = _parseExpression(expr)
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
							
						elseif one == tbname then
							newone = alias
							one = one .. '.'
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
	
	local cols
	if self.colSelects then
		--只获取某几列
		local plains = {}
		if excepts then			
			for k,v in pairs(self.colSelects) do
				if not excepts[k] then
					plains[#plains + 1] = k
				end
			end
		else
			for k,v in pairs(self.colSelects) do
				plains[#plains + 1] = k
			end
		end

		cols = table.concat(plains, ',' .. alias)		
		
	elseif excepts then
		--只排除掉某几列
		local fps = {}
		local fieldPlain = model.__fieldsPlain
		
		for i = 1, #fieldPlain do
			local n = fieldPlain[i]
			if not excepts[n] then
				fps[#fps + 1] = n
			end
		end
		
		cols = #fps > 0 and table.concat(fps, ',' .. alias) or '*'
	else
		--所有列
		cols = '*'
	end	

	if #alias > 0 then
		cols = #cols > 0 and (alias .. cols) or ''
	end
	if express then
		cols = #cols > 0 and string.format('%s,%s', express, cols) or express
	end

	if #cols > 0 then
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
			local tp = type(v)

			if cfg.ai then
				if not full or not string.checkinteger(v) then
					v = nil
				end
			elseif v == nil then
				if cfg.default then
					v = cfg.default
				elseif cfg.null then
					v = 'NULL'
				else
					v = cfg.type == 1 and "''" or '0'
				end
			elseif v == ngx.null then
				v = 'NULL'
			elseif tp == 'table' then
				v = v[1]
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
				if alias and #alias > 0 then
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


--如果condValues非nil，说明现在处理的不是self代表的query自己的条件
queryexecuter.buildWheres = function(self, sqls, condPre, alias, condValues)
	if not alias then alias = '' end
	
	if not condValues then
		if self.condString then
			if condPre then
				sqls[#sqls + 1] = condPre
			end
			sqls[#sqls + 1] = self.condString
			return true
		end
		
		condValues = self.condValues
	end
	
	if condValues and #condValues > 0 then
		local wheres, conds = {}, queryexecuter.conds
		local joinFrom = self.joinFrom
		
		for i = 1, #condValues do
			local one, rsql = condValues[i], nil
			
			if i > 1 and one.c == 1 then
				one.c = 2
			end			
			
			if one.sub then
				--子查询
				local subq = one.sub
				subq.limitStart, subq.limitTotal = nil, nil
				
				local expr = processTokenedString(self, alias, one.n, joinFrom)
				local subsql = queryexecuter.SELECT(subq, subq.__m, self.db)
				
				if subsql then
					if one.puredkeyname then
						rsql = string.format('%s IN(%s)', expr, subsql)
					else
						rsql = string.format('%s(%s)', expr, subsql)
					end
				end
				
			else
				rsql = conds[one.c] .. processTokenedString(self, alias, one.expr, joinFrom)
			end

			wheres[#wheres + 1] = rsql
		end
		
		if condPre then
			sqls[#sqls + 1] = condPre
		end
		sqls[#sqls + 1] = table.concat(wheres, ' ')
		
		return true
	end
	
	return false
end

queryexecuter.buildWhereJoins = function(self, sqls, haveWheres)
	local cc = self.joins == nil and 0 or #self.joins
	if cc < 1 then
		return
	end

	for i = 1, cc do
		local q = self.joins[i].q
		q.joinFrom = self
		queryexecuter.buildWheres(q, sqls, haveWheres and 'AND' or 'WHERE', q.alias .. '.')
		q.joinFrom = nil
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
		q.alias = q.userAlias or ('_' .. string.char(65 + indient))

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

queryexecuter.buildJoinsConds = function(self, sqls, haveOns)
	local cc = self.joins == nil and 0 or #self.joins
	if cc < 1 then
		return
	end
	
	local validJoins = queryexecuter.validJoins
	
	for i = 1, cc do
		local join = self.joins[i]
		local q = join.q
		
		q.joinFrom = self

		sqls[#sqls + 1] = validJoins[join.type]
		sqls[#sqls + 1] = q.__m.__name
		sqls[#sqls + 1] = q.alias
		sqls[#sqls + 1] = 'ON('
		if not queryexecuter.buildWheres(q, sqls, nil, q.alias .. '.', q.onValues) then		
			sqls[#sqls + 1] = '1'
		end
		sqls[#sqls + 1] = ')'
		
		q.joinFrom = nil
		
		queryexecuter.buildJoinsConds(q, sqls, haveOns)
	end
end

queryexecuter.buildOrder = function(self, sqls, alias)
	if self.orderBy then
		if type(self.orderBy) == 'string' then
			sqls[#sqls + 1] = 'ORDER BY'
			sqls[#sqls + 1] = self.orderBy
		else
			sqls[#sqls + 1] = string.format('ORDER BY %s%s %s', alias, 		self.orderBy.name, self.orderBy.order)
		end
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
		
		--设置join on条件
		on = function(self, name, val)
			return processOn(self, 1, name, val)
		end,
		andOn = function(self, name, val)
			return processOn(self, 2, name, val)
		end,
		orOn = function(self, name, val)
			return processOn(self, 3, name, val)
		end,
		xorOn = function(self, name, val)
			return processOn(self, 4, name, val)
		end,
		notOn = function(self, name, val)
			return processOn(self, 5, name, val)
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
		
		--两个Query进行联接
		join = function(self, query, joinType)
			local validJoins = { inner = 1, left = 1, right = 1 }
			local jt = joinType == nil and 'inner' or joinType:lower()
			
			if validJoins[jt] == nil then
				error("error join type '%s' for join", tostring(joinType))
				return self
			end

			local tbname = query.__m.__name
			local j = { q = query, type = jt, on = self.joinOn }
			
			if not self.joins then
				self.joins = { j }
				self.joinNames = { }
				self.joinNames[tbname] = query
				
			elseif not self.joinNames[tbname] then
				self.joins[#self.joins + 1] = j
				self.joinNames[tbname] = query
			end
			
			return self
		end,
		
		--设置表的表名，如果不设置，则将使用自动别名，自动别名的规则是_C[C>=A && C<=Z]，在设置别名的时候请不要与自动别名冲突
		alias = function(self, name)
			if type(name) == 'string' then
				name = name:trim()
				if #name > 0 then
					local chk = name:match('_[A-Z]')
					if not chk or #chk ~= #name then
						self.userAlias = name
					end
				end
			else
				self.userAlias = nil
			end
			return self
		end,
		
		--设置排序
		order = function(self, field, asc)
			if field:find('[,]+') then
				self.orderBy = field
			else
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
			end
			
			return self
		end,
		--限制数量
		limit = function(self, start, total)
			if (start or total) == nil then
				self.limitStart, self.limitTotal = nil, nil
				return self
			end	
			
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
		
		execute = function(self, db, result)
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
			
			if sqls then			
				--[[local f = io.open('d:/sqls.txt', 'w+')
				f:write(sqls .. '\n')
				f:close()]]
				
				result = ormr.init(result, model)
				res = ormr.query(result, db, sqls, self.limitTotal or 10)
			end
			
			self.__vals = nil
			return res, res and result or nil
		end,
		
		--执行并取第一行，如果都成功，则返回结果集本身
		exec = function(self, db, result)
			local res, r = self:execute(db, result)
			if res then
				if self.op == 'SELECT' then
					return r + 1 and r or nil
				end
				
				return { rows = res.affected_rows, insertid = res.insert_id }
			end
		end,
		
		--查询并返回所有行，并且在没有结果集或不是查询指令的时候返回的是一个空的table而非nil
		fetchAll = function(self, db, result)
			if self.op == 'SELECT' then
				local res, r = self:execute(db, result)	
				if res then
					return r + 1 and r(true) or {}
				end
			end
			return {}
		end,
		--查询并返回第一行，并且在没有结果集或不是查询指令的时候返回的是一个空的table而非nil
		fetchFirst = function(self, db, result)
			if self.op == 'SELECT' then
				local res, r = self:execute(db, result)
				if res then
					return r + 1 and r(false) or {}
				end
			end
			return {}
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
		findAll = function(self, p1, p2, p3, p4)
			local r = self:find(p1, p2, p3, p4)
			if r then
				return r(true)
			end			
		end,
		findFirst = function(self, name, val)
			local q = { __m = self, __reeme = self.__reeme, op = 'SELECT' }
			setmetatable(q, queryMeta)
			if name then q:where(name, val) end
			return q:limit(1):exec()
		end,
		
		query = function(self)
			local q = { __m = self, __reeme = self.__reeme, op = 'SELECT', limitStart = 0, limitTotal = 50 }
			return setmetatable(q, queryMeta)
		end,
		
		update = function(self)
			local q = { __m = self, __reeme = self.__reeme, op = 'UPDATE', limitStart = 0, limitTotal = 50 }
			return setmetatable(q, queryMeta)
		end,
		
		expression = function(self, expr)
			local q = { __m = self, __reeme = self.__reeme, op = 'SELECT', limitStart = 0, limitTotal = 50 }
			return setmetatable(q, queryMeta):expr(expr):columns()
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