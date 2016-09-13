--表达式分解为token函数
local _parseExpression = findmetatable('REEME_C_EXTLIB').sql_expression_parse
--date/datetime类型的原型
local datetimeMeta = getmetatable(require('reeme.orm.datetime')())
local queryMeta = require('reeme.orm.model').__index.__queryMetaTable
local specialExprFunctions = { distinct = 1, count = 2, as = 3 }
local mysqlwords = require('reeme.orm.mysqlwords')

--合并器
local builder = table.new(0, 24)

--允许的条件组合方式
builder.conds = { '', 'AND ', 'OR ', 'XOR ', 'NOT ' }
--允许的联表方式
builder.validJoins = { inner = 'INNER JOIN', left = 'LEFT JOIN', right = 'RIGHT JOIN', full = 'FULL JOIN' }


--解析一个where条件，本函数被processWhere调用
builder.parseWhere = function(self, condType, name, value)
	self.condString = nil	--condString和condValues同一时间只会存在一个，只有一个是可以有效的
	if not self.condValues then
		self.condValues = {}
	end

	name = name:trim()
	
	if value == nil then
		--name就是整个表达式
		return { expr = name, c = condType }
	end
	
	local keyname, puredkeyname, findpos = nil, false, 1
	while true do
		--找到第一个不是mysql函数的名字时停止
		keyname, findpos = name:findvarname(findpos)
		--ngx.say(keyname, ',', findpos, '<br/>')
		if not keyname then
			keyname = nil
			break
		end
		if not mysqlwords[keyname:upper()] then
			break
		end
	end

	if keyname and #keyname == #name then
		--key没有多余的符号，只是一个纯粹的列名
		puredkeyname = true
	end
	
	local tv = type(value)
	if tv == 'table' then
		local mt = getmetatable(value)
		if mt == queryMeta then
			--子查询
			return { puredkeyname = puredkeyname, n = name, sub = value, c = condType }
		end
		
		if mt == datetimeMeta then
			--日期类型
			value = tostring(value)
			return { expr = puredkeyname and string.format('%s=%s', name, value) or value, c = condType }
		end
		
		--{value}这种表达式
		value = value[1]
		tv = type(value)

	elseif value == ngx.null then
		--设置为null值
		return { expr = puredkeyname and string.format('%s IS NULL', name) or (name .. 'NULL'), c = condType }
	end

	if type(name) == 'string' then
		--key=value
		local f = keyname and self.m.__fields[keyname] or nil
		local newv, quoted = nil, false		

		if tv == 'string' and value:byte(1) == 39 and value:byte(vlen) == 39 then
			quoted = true
		end

		if f then
			--有明确的字段就可以按照字段的配置进行检测和转换			
			if quoted then
				newv = value:sub(2, #newv - 1)
			else
				newv = tostring(value)
			end
			if #newv > f.maxlen then
				return nil
			end
			
			--判断是否cdata
			if tv == 'cdata' then
				value, newv = string.checkinteger(newv)
				value = newv
			end
			
			--再根据字段的值类型做相应的转换
			if f.type == 1 then
				if not quoted then
					value = ngx.quote_sql_str(newv)
				end
			elseif f.type == 2 or f.type == 3 then
				value = tonumber(newv)
			elseif f.type == 4 then
				value = toboolean(newv)
			else
				value = nil
			end

			if value == nil then
				return nil
			end

		elseif not quote then
			--未引用的字符串进行转义
			value = ngx.quote_sql_str(value)
		end
		
		return { expr = puredkeyname and string.format('%s=%s', name, value) or (name .. value), c = condType }
	end
end

--处理where函数带来的条件
builder.processWhere = function(self, condType, k, v)
	local tp = type(k)
	if tp == 'table' then
		for name,val in pairs(k) do
			local where = builder.parseWhere(self, condType, name, val)
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

	local where = builder.parseWhere(self, condType, k, v)
	if where then
		self.condValues[#self.condValues + 1] = where
	else
		error(string.format("process where(%s) function with illegal value(s) call", name))
	end
	return self
end

--处理on函数带来的条件
builder.processOn = function(self, condType, k, v)
	local tp = type(k)
	if tp == 'string' then
		local where = builder.parseWhere(self, condType, k, v)
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
			local where = builder.parseWhere(self, condType, name, val)
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
builder.processTokenedString = function(self, alias, expr, joinFrom)
	if #alias == 0 then
		return expr
	end

	local fields = self.m.__fields
	local sql, adjust = expr, 0
	local n1, n2 = self.m.__name, joinFrom and joinFrom.m.__name or nil
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
				--出现自己的表名，那么后面是一个字段名，由于字段名称会被自动的加上alias，因此在这里只需要将表名移除即可
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
builder.SELECT = function(self, model, db)
	local sqls = {}
	sqls[#sqls + 1] = 'SELECT'
	
	--main
	local alias = ''
	self.db = db
	if self.joins and #self.joins > 0 then
		self.alias = self.userAlias or '_A'
		alias = self.alias .. '.'
	end	
	
	local cols = builder.buildColumns(self, model, sqls, alias)
	
	--joins fields
	builder.buildJoinsCols(self, sqls, 1, #cols > 0 and true or false)
	
	--from
	sqls[#sqls + 1] = 'FROM'
	sqls[#sqls + 1] = model.__name
	if #alias > 0 then
		sqls[#sqls + 1] = self.alias
	end

	--joins conditions	
	builder.buildJoinsConds(self, sqls)
	
	--where
	local haveWheres = builder.buildWheres(self, sqls, 'WHERE', alias)
	builder.buildWhereJoins(self, sqls, haveWheres)
	
	--order by
	builder.buildOrder(self, sqls, alias)
	--limit
	builder.buildLimits(self, sqls)
	
	--end
	self.db = nil
	return table.concat(sqls, ' ')
end
	
builder.UPDATE = function(self, model, db)
	local sqls = {}
	sqls[#sqls + 1] = 'UPDATE'
	sqls[#sqls + 1] = model.__name
	
	--has join(s) then alias
	local alias = ''
	self.db = db
	if self.joins and #self.joins > 0 then
		self.alias = self.userAlias or '_A'
		alias = self.alias .. '.'
		
		sqls[#sqls + 1] = self.alias
	end	

	--joins fields
	if #alias > 0 then
		builder.buildJoinsCols(self, nil, 1)
		
		--joins conditions	
		builder.buildJoinsConds(self, sqls)
	end
	
	--all values
	if builder.buildKeyValuesSet(self, model, sqls, alias) > 0 then
		table.insert(sqls, #sqls, 'SET')
	end
	
	--where
	if not builder.buildWheres(self, sqls, 'WHERE', alias) then
		if type(self.__where) == 'string' then
			sqls[#sqls + 1] = 'WHERE'
			sqls[#sqls + 1] = builder.processTokenedString(self, alias, self.__where)
		else
			--find primary key
			local haveWheres = false
			local idx, vals = model.__fieldIndices, self.keyvals

			if vals then
				for k,v in pairs(idx) do
					if v.type == 1 then
						local v = vals[k]
						if v and v ~= ngx.null then
							builder.processWhere(self, 1, k, v)
							haveWheres = builder.buildWheres(self, sqls, 'WHERE', alias)
							break
						end
					end
				end
			end

			if not haveWheres then
				error("Cannot do model update without any conditions")
				return false
			end
		end
	end
	
	--order by
	if self.orderBy then
		sqls[#sqls + 1] = string.format('ORDER BY %s %s', self.orderBy.name, self.orderBy.order)
	end
	--limit
	builder.buildLimits(self, sqls, true)
	
	--end
	return table.concat(sqls, ' ')
end

builder.INSERT = function(self, model, db)
	local sqls = {}
	sqls[#sqls + 1] = 'INSERT INTO'
	sqls[#sqls + 1] = model.__name
	
	--all values
	if builder.buildKeyValuesSet(self, model, sqls, '') > 0 then
		table.insert(sqls, #sqls, 'SET')
	else
		sqls[#sqls + 1] = '() VALUES()'
	end
	
	--end
	return table.concat(sqls, ' ')
end
	
builder.DELETE = function(self, model)
	local sqls = {}
	sqls[#sqls + 1] = 'DELETE'
	sqls[#sqls + 1] = 'FROM'
	sqls[#sqls + 1] = model.__name
	
	--where
	if not builder.buildWheres(self, sqls, 'WHERE') then
		if type(self.__where) == 'string' then
			sqls[#sqls + 1] = 'WHERE'
			sqls[#sqls + 1] = builder.processTokenedString(self, '', self.__where)
		else
			--find primary or unique
			local haveWheres = false
			local idx, vals = model.__fieldIndices, self.keyvals
			if vals then
				for k,v in pairs(idx) do
					if (v.type == 1 or v.type == 2) and vals[k] then
						builder.processWhere(self, 1, k, vals[k])
						haveWheres = builder.buildWheres(self, sqls, 'WHERE', '')
						break
					end
				end
			end

			if not haveWheres then
				error("Cannot do model delete without any conditions")
				return false
			end
		end
	end
	
	--limit
	builder.buildLimits(self, sqls, true)
	
	--end
	return table.concat(sqls, ' ')
end


builder.buildColumns = function(self, model, sqls, alias, returnCols)
	--加入所有的表达式
	local excepts, express = nil, nil
	if self.expressions then
		local fields = self.m.__fields
		local tbname = self.m.__name
		local skips = 0
		
		for i = 1, #self.expressions do
			local expr = self.expressions[i]

			if skips <= 0 and type(expr) == 'string' then
				local adjust = 0
				local tokens, poses = _parseExpression(expr)
				if tokens then
					local removeCol = false
					for k = 1, #tokens do
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

						elseif one == tbname then
							newone = alias
							one = one .. '.'

						else
							--特殊处理
							local spec = specialExprFunctions[one:lower()]
							if spec == 1 then
								--遇到这些定义的表达式，这个表达式所关联的字段就不会再在字段列表中出现
								removeCol = true
							elseif spec == 2 then
								--构造出excepts数据，哪怕是为空，因为只要excepts数组存在，字段就不会以*形式出现，这样就不会组合出count(*),*的语句
								if not excepts then excepts = {} end
							elseif spec == 3 then
								--AS命名之后需要德育下一个token直接复制即可
								skips = 2
							end

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

			skips = skips - 1
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

	if express then
		cols = #cols > 0 and string.format('%s,%s%s', express, alias, cols) or express
	elseif #cols > 0 then
		cols = alias .. cols
	end

	if returnCols ~= true and #cols > 0 then
		sqls[#sqls + 1] = cols
	end
	return cols
end

builder.buildKeyValuesSet = function(self, model, sqls, alias)
	local fieldCfgs = model.__fields
	local vals, full = self.keyvals, self.fullop
	local isUpdate = self.op == 'UPDATE' and true or false
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
				--自增长值要么是fullCreate/fullSave要么被忽略
				if not full or not string.checkinteger(v) then
					v = nil
				end
			elseif v == nil then
				--值为nil，那么判断是否使用字段的默认值
				if not isUpdate then
					if cfg.default then
						v = cfg.default
					elseif cfg.null then
						v = 'NULL'
					else
						v = cfg.type == 1 and "''" or '0'
					end
				end
			elseif v == ngx.null then
				--NULL值直接设置
				v = 'NULL'
			elseif tp == 'table' then
				--table类型则根据meta来进行判断是什么table
				local mt = getmetatable(v)
				
				if mt == datetimeMeta then
					--日期时间
					v = ngx.quote_sql_str(tostring(v))
				elseif mt == queryMeta then	
					--子查询
				else
					--字符串原值
					v = v[1]
				end
			elseif tp == 'cdata' then
				--cdata类型，检测是否是boxed int64
				local i64type, newv = string.cdataIsInt64(v)
				v = i64type > 0 and newv:sub(1, #newv - i64type) or newv
			end
			
			if v ~= nil then
				if cfg.type == 1 then
					--字段要求为字符串，所以引用
					v = ngx.quote_sql_str(v)
				elseif cfg.type == 4 then
					--布尔型使用1或0来处理
					v = toboolean(v) and '1' or '0'
				elseif cfg.type == 3 then
					--数值/浮点数类型，检测值必须为浮点数
					if not string.checknumeric(v) then
						print(string.format("model '%s': a field named '%s' its type is number but the value is not a number", model.__name, name))
						v = nil
					end
				else
					local reti, newv = string.checkinteger(v)
					if not reti then
						--否则就都是整数类型，检测值必须为整数
						print(string.format("model '%s': a field named '%s' its type is integer but the value is not a integer", model.__name, name))
						v = nil
					end
					
					if newv then
						v = newv
					end
				end
				
				if v ~= nil then
					--到了这里v还不是nil，就可以记录了
					if alias and #alias > 0 then
						keyvals[#keyvals + 1] = string.format("%s%s=%s", alias, name, v)
					else
						keyvals[#keyvals + 1] = string.format("%s=%s", name, v)
					end
				end
			end
		end
	end

	sqls[#sqls + 1] = table.concat(keyvals, ',')
	return #keyvals
end


--如果condValues非nil，说明现在处理的不是self代表的query自己的条件
builder.buildWheres = function(self, sqls, condPre, alias, condValues)
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
		local wheres, conds = {}, builder.conds
		local joinFrom = self.joinFrom
		
		for i = 1, #condValues do
			local one, rsql = condValues[i], nil
			
			if i > 1 and one.c == 1 then
				--如果没有指定条件连接方式，那么当不是第1个条件的时候，就会自动修改为and
				one.c = 2
			end			
			
			if one.sub then
				--子查询
				local subq = one.sub
				subq.limitStart, subq.limitTotal = nil, nil
				
				local expr = builder.processTokenedString(self, alias, one.n, joinFrom)
				local subsql = builder.SELECT(subq, subq.m, self.db)
				
				if subsql then
					if one.puredkeyname then
						rsql = string.format('%s IN(%s)', expr, subsql)
					else
						rsql = string.format('%s(%s)', expr, subsql)
					end
				end
				
			else
				rsql = conds[one.c] .. builder.processTokenedString(self, alias, one.expr, joinFrom)
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

builder.buildWhereJoins = function(self, sqls, haveWheres)
	local cc = self.joins == nil and 0 or #self.joins
	if cc < 1 then
		return
	end

	for i = 1, cc do
		local q = self.joins[i].q
		q.joinFrom = self
		builder.buildWheres(q, sqls, haveWheres and 'AND' or 'WHERE', q.alias .. '.')
		q.joinFrom = nil
	end
end

builder.buildJoinsCols = function(self, sqls, indient, haveCols)
	local cc = self.joins == nil and 0 or #self.joins
	if cc < 1 then
		return
	end

	for i = 1, cc do
		local cols = nil
		local q = self.joins[i].q
		q.alias = q.userAlias or ('_' .. string.char(65 + indient))

		if sqls then
			cols = builder.buildColumns(q, q.m, sqls, q.alias .. '.', true)
			if #cols > 0 then
				if haveCols then
					sqls[#sqls + 1] = ','
				end
				sqls[#sqls + 1] = cols
			else
				cols = false
			end
		end
		
		local newIndient = builder.buildJoinsCols(q, sqls, indient + 1, cols or haveCols)
		indient = newIndient or (indient + 1)
	end
	
	return indient
end

builder.buildJoinsConds = function(self, sqls, haveOns)
	local cc = self.joins == nil and 0 or #self.joins
	if cc < 1 then
		return
	end
	
	local validJoins = builder.validJoins
	
	for i = 1, cc do
		local join = self.joins[i]
		local q = join.q
		
		q.joinFrom = self

		sqls[#sqls + 1] = validJoins[join.type]
		sqls[#sqls + 1] = q.m.__name
		sqls[#sqls + 1] = q.alias
		sqls[#sqls + 1] = 'ON('
		if not builder.buildWheres(q, sqls, nil, q.alias .. '.', q.onValues) then		
			sqls[#sqls + 1] = '1'
		end
		sqls[#sqls + 1] = ')'
		
		q.joinFrom = nil
		
		builder.buildJoinsConds(q, sqls, haveOns)
	end
end

builder.buildOrder = function(self, sqls, alias)
	if self.orderBy then
		if type(self.orderBy) == 'string' then
			sqls[#sqls + 1] = 'ORDER BY'
			sqls[#sqls + 1] = self.orderBy
		else
			sqls[#sqls + 1] = string.format('ORDER BY %s%s %s', alias, 		self.orderBy.name, self.orderBy.order)
		end
	end
end

builder.buildLimits = function(self, sqls, ignoreStart)
	if self.limitTotal and self.limitTotal > 0 then
		if ignoreStart then
			sqls[#sqls + 1] = string.format('LIMIT %u', self.limitTotal)
		else
			sqls[#sqls + 1] = string.format('LIMIT %u,%u', self.limitStart, self.limitTotal)
		end
	end
end

return builder