--表达式分解为token函数
local cExtLib = findmetatable('REEME_C_EXTLIB')
local _parseExpression, _findToken = cExtLib.sql_token_parse, cExtLib.find_token

local mysqlwords = require('reeme.orm.mysqlwords')

--date/datetime类型的原型
local datetimeMeta = getmetatable(require('reeme.orm.datetime')())
--原始表达式的原型
local rawsqlMeta = require('reeme.orm.rawsql')(mysqlwords)
--Json操作器原型
local myjsonMeta = require('reeme.orm.mysqljson')()

local queryMeta = require('reeme.orm.model').__index.__queryMetaTable
local specialExprFunctions = { distinct = 1, count = 2, as = 3 }
local reemext = require('ffi').load('reemext')
local allOps = { SELECT = 1, UPDATE = 2, INSERT = 3, DELETE = 4 }
local mysqlequals = { ['='] = 1, ['!='] = 1, ['<'] = 1, ['<='] = 1, ['>'] = 1, ['>='] = 1 }

--合并器
local builder = table.new(0, 24)

--数据库类型名字
builder.dbTypeName = 'mysql'
--允许的条件组合方式
builder.conds = { '', 'AND ', 'OR ', 'XOR ', 'NOT ' }
--允许的联表方式
builder.validJoins = { inner = 'INNER JOIN', left = 'LEFT JOIN', right = 'RIGHT JOIN', full = 'FULL JOIN', cross = 'CROSS JOIN' }

local function autoConvertJson(j)
	local jType = type(j)
	
	if jType == 'table' then
		j = string.json(j, string.JSON_UNICODES)
		assert(type(j) == 'string', 'Encode table to json string failed')
		return j
	end
	
	if jType == 'string' then
		return j
	end

	assert(false, 'JSON value type checked error')
end

--处理一个SQL值（本值必须与字段相关，会根据字段的配置对值做出相应的处理）
--第二返回值表示建议在这个值的基础上使用的运算符，这个运算符是否需要用上由外部自行决定。若返回为nil表示值为原值表达式
local function buildSqlValue(self, cfg, v, limitByField, usedEq)
	local check
	local tp = type(v)
	local suggCon, removeEq = '=', false
	local quoteIt, multiVals, checkType = false, false, true

	check = function()
		if v == ngx.null then
			--NULL值直接设置
			v = 'NULL'
			if usedEq == nil then
				suggCon = ' IS '
			elseif usedEq == '=' then
				suggCon, removeEq = ' IS ', true
			elseif usedEq == '!=' then
				suggCon, removeEq = ' IS NOT ', true
			else
				error(string.format("Illegal equation symbol: '%s' used for NULL", usedEq))
			end
			
		elseif tp == 'table' then	
			--table类型则根据meta来进行判断是什么table
			local mt = getmetatable(v)

			if mt == datetimeMeta then
				--日期时间
				v = ngx.quote_sql_str(tostring(v))

			elseif mt == rawsqlMeta then
				--原值表达式，0号是表达式字符串
				assert(type(v[0]) == 'string')
				
				checkType = false
				suggCon = nil
				v = v[0]
				
			elseif mt == myjsonMeta then
				--Json表达式。0号是doc值
				local j = v[0] or v[1]
				local jType = type(j)
				
				if jType == 'table' then
					--doc是个Table，编码
					v = string.json(j, string.JSON_UNICODES)
					assert(type(v) == 'string', 'Encode table to json string failed')
				elseif jType == 'string' then
					--doc是个字符串，判断是否是个Json
				else
					assert(jType == 'nil', string.format('JSON value type "%s" is not allowed', jType))
				end
				
			elseif mt == queryMeta then	
				--子查询
				v.limitStart, v.limitTotal = nil, nil
				local subsql = builder.SELECT(v, v.m)
				if subsql then
					v = table.concat({ '(', subsql, ')' }, '')
				else
					v = '(error build sql for sub-query)'
				end

			elseif cfg.type == 8 then
				--Json类型+Table无Meta，直接转Json字符串
				v = string.json(v, string.JSON_UNICODES)

			elseif #v == 1 then		
				v = v[1]
				tp = type(v)
				check()

			elseif #v > 1 then
				--表达式或多值
				multiVals, suggCon = true, ''
			else
				local vEnc = ''
				pcall(function() vEnc = string.json(v, string.JSON_UNICODES) end)
				error('Illegal sql value for table type: ' .. vEnc)
			end
			
		elseif tp == 'cdata' then
			--cdata类型，检测是否是boxed int64
			local s = tostring(v)
			local i64type, newv = reemext.cdataisint64(s, #s), s
			if i64type > 0 then
				v = newv:sub(1, #newv - i64type)
			else
				v, quoteIt = newv, true
			end
			tp = 'string'
			
		elseif tp == 'string' then
			--字符串类型需要被引用
			quoteIt = true
			
		elseif tp ~= 'number' and tp ~= 'boolean' then
			--如果再不是数字或布尔型那么就要报错了
			error('Illegal sql value type : ' .. tp)
		end
	end
	
	check()

	if v ~= nil then
		local spec = 'IN'

		if usedEq == '=' then
			spec, removeEq = 'IN', true
		elseif usedEq == '!=' then
			spec, removeEq = 'NOT IN', true
		end

		if cfg.type == 1 then
			--字段要求为字符串
			if multiVals then
				multiVals = table.new(#v, 0)
				for i = 1, #v do
					multiVals[i] = buildSqlValue(self, cfg, v[i]) or "''"
				end

				if #multiVals > 0 then
					v = string.format(" %s(%s)", spec, table.concat(multiVals, ','))
				else
					v = string.format(" %s('')", spec)
				end
				multiVals = nil
			else
				v = tostring(v)
				if limitByField and #v > cfg.maxlen then
					v = string.sub(1, cfg.maxlen)
				end
				
				if quoteIt and (string.byte(v, 1) ~= 39 or string.byte(v, #v) ~= 39) then
					--没有引用，那么增加引用
					v = ngx.quote_sql_str(v)
				end
			end
			
		elseif cfg.type == 4 then
			--布尔型使用1或0来处理
			if multiVals then
				multiVals = table.new(#v, 0)
				for i = 1, #v do
					multiVals[i] = buildSqlValue(self, cfg, v[i]) or '0'
				end
				
				if #multiVals > 0 then
					v = string.format(' %s(%s)', spec, table.concat(multiVals, ','))
				else
					v = string.format(' %s(0)', spec)
				end
				multiVals = nil
			else
				v = toboolean(v) and '1' or '0'
			end
			
		elseif cfg.type == 3 then
			--数值/浮点数类型
			if multiVals then
				multiVals = table.new(#v, 0)
				for i = 1, #v do
					multiVals[i] = buildSqlValue(self, cfg, v[i]) or '0'
				end
				
				v = string.format(' %s(%s)', spec, table.concat(multiVals, ','))
				multiVals = nil
				
			elseif checkType and not string.checknumeric(v) then
				logger.e(string.format("model '%s': a field named '%s' its type is number but the value is '%s' not a number", self.m.__name, cfg.colname, tostring(v)))
				v = nil
			end
			
		elseif cfg.type == 8 then
			--Json类型
			if type(v) == 'string' then
				v = ngx.quote_sql_str(v)
			else
				v = nil
			end
			
		elseif multiVals then
			--整数型，多值
			multiVals = table.new(#v, 0)
			for i = 1, #v do
				multiVals[i] = buildSqlValue(self, cfg, v[i]) or '0'
			end
			
			v = string.format(' %s(%s)', spec, table.concat(multiVals, ','))
			multiVals = nil

		elseif checkType then
			--整数型的值
			local reti, newv = string.checkinteger(v)
			if not reti then
				--否则就都是整数类型，检测值必须为整数
				logger.e(string.format("model '%s': a field named '%s' its type is integer but the value is '%s' not a integer", self.m.__name, cfg.colname, tostring(v)))
				v = nil
			end
			
			if newv then
				v = newv
			end
		end
	end
	
	return v, suggCon, removeEq
end

builder.wrapValue = buildSqlValue

-----------------------------------------------------------------------------------------------------------------------
--解析一个where条件，本函数被processWhere调用
builder.parseWhere = function(self, condType, name, value)
	if not self.condValues then
		self.condValues = {}
	end	
	
	if value == nil then
		--name就是整个表达式
		return { expr = name, c = condType }
	end
	if name == nil then
		--value是整个表达式
		return { expr = value, c = condType }
	end

	name = name:trim()	

	local purekn = false
	local r = ngx.re.match(name, '^[\\w-]+$|^[\\w-]+([.]{1}[\\w-]+)$', 'o')

	if r and (r[1] or not mysqlwords[r[0]]) then
		purekn = true
	end

	if type(value) == 'table' then
		local mt = getmetatable(value)
		if mt == queryMeta then
			--子查询
			return { purekn = purekn, n = name, sub = value, c = condType }
		end
		if mt == myjsonMeta then
			--Json查询
			local r, range, tp = { purekn = false, c = condType }, 'one', nil
			
			if value[0] or value[1] then
				--使用Json查找
				local v = autoConvertJson(value[0] or value[1])
				r.expr = string.format("JSON_CONTAINS(%s, %s)", name, ngx.quote_sql_str(v))
			else
				--模糊查找
				tp = type(value[1])
				assert(tp == 'string', 'Error use JSON type for where condition')

				if value._all then
					range = 'all'
				end

				r.expr = string.format("JSON_SEARCH(%s, '%s', %s) %s", name, range, value[1], value._not and 'IS NULL' or 'IS NOT NULL')
			end
			
			return r
		end
	end

	return { purekn = purekn, expr = name, value = value, c = condType }
end

--处理where函数带来的条件
builder.processWhere = function(self, condType, k, v)
	local tp = type(k)
	if tp == 'table' then
		if #k > 0 then
			for i = 1, #k do
				local where = builder.parseWhere(self, condType, nil, k[i])
				if where then
					self.condValues[#self.condValues + 1] = where
				else
					error(string.format("call where(%s) function with illegal value(s) call", k[i]))
				end
			end
		else
			for name,val in pairs(k) do
				local where = builder.parseWhere(self, condType, name, val)
				if where then
					self.condValues[#self.condValues + 1] = where
				else
					error(string.format("call where(%s) function with illegal value(s) call", name))
				end
			end
		end
		
		return self
	end
	
	if tp ~= 'string' then
		k = tostring(k)
	end
	if #k > 0 then
		local where = builder.parseWhere(self, condType, k, v)
		if where then
			self.condValues[#self.condValues + 1] = where
		else
			error(string.format("call where(%s) function with illegal value(s) call", k))
		end
	end
	
	return self
end

--处理on函数带来的条件
builder.processOn = function(self, condType, k, v)
	local tp = type(k)
	if tp == 'string' and #k > 0 then
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
		if #k > 0 then
			for i = 1, #k do
				local where = builder.parseWhere(self, condType, nil, k[i])
				if where then
					if not self.onValues then
						self.onValues = { where }
					else
						self.onValues[#self.onValues + 1] = where
					end
				else
					error(string.format("process on(%s) function call failed: illegal value or confilict with declaration of model fields", k[i]))
				end
			end
		else
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
	end

	return self
end

--解析Where条件中的完整表达式，将表达式中用到的字段名字，按照表的alias名称来重新生成
builder.processTokenedString = function(self, alias, expr, purekn, allJoins)
	local tokens, poses, lastField
	local fields, aliasab = self.m.__fields, self.aliasAB
	
	if purekn then
		--纯字段名就不需要再解析一次了
		tokens = { expr }
		poses = { 1 }
		
	elseif mysqlwords[expr] then
		--这种情况下还需要考虑是不是可能是表的字段名
		lastField = fields[expr] or fields[aliasab[expr]]
		if lastField then
			return expr, lastField, nil, '='
		end
		
		return expr
		
	else
		tokens, poses = _parseExpression(expr)
		assert(tokens ~= nil, expr)
	end

	--逐个token的循环进行处理
	local sql, adjust = expr, 0	
	local selfname = self.userAlias or self.m.__name
	local lastToken, lastEq

	for i = 1, #tokens do
		local newone
		local token = tokens[i]

		if string.byte(token, 1) == 63 then
			--?替换位
			local bindpos

			if #token == 1 then
				--没有指定位置的替换位
				bindpos = self.lastBindpos + 1
				self.lastBindpos = bindpos
			else
				--直接指定了位置的替换位
				bindpos = tonumber(string.sub(token, 1))
				self.lastBindpos = math.max(bindpos, self.lastBindpos)
			end

			for i = 1, #allJoins do
				local vals = allJoins[i].bindvals
				if vals then
					newone = tostring(vals[bindpos])
					break
				end
			end

			assert(newone ~= nil)
			
		else
			--非替换位
			local a, b = string.cut(token, '.')

			if b then
				--可能是表名.字段名
				if a == selfname then
					--出现自己的原表名，那么b就一定是字段名了
					lastField = fields[b] or fields[aliasab[b]]
					newone = alias .. b
				else
					--判断是否其它被join进来的表名
					newone = allJoins[a]
					if newone then
						lastField = newone:getField(b)
						newone = newone._alias .. '.' .. b
					end
				end
				
				lastToken = a
				
			elseif a == '*' then
				--所有字段
				newone = alias .. '*'

			elseif mysqlwords[a] then
				lastToken = a
				
			elseif mysqlequals[a] then
				lastEq = a
			
			else
				--查找是否字段名。先在自己身上查找，没有找到再去所联的表中查找
				local field = self:getField(a)
				if field then
					lastField = field
					if self._alias then
						newone = self._alias .. '.' .. a
					end
				else
					local m
					for i = 1, #allJoins do
						m = allJoins[i]
						field = m ~= self and m:getField(a) or nil
						if field then
							lastField = field
							if m._alias then
								newone = m._alias .. '.' .. a
							end
							break
						end
					end
				end
				
				lastToken = a
			end
		end

		if newone then
			--替换掉最终的表达式
			sql = sql:subreplace(newone, poses[i] + adjust, #token)
			adjust = adjust + #newone - #token
		end
	end
	
	return sql, lastField, lastToken, lastEq
end


-----------------------------------------------------------------------------------------------------------------------
--将query设置的条件合并为SQL语句
builder.SELECT = function(self)
	local sqls = {}
	sqls[#sqls + 1] = 'SELECT'
	
	--main
	local model = self.m
	local alias, allJoins = '', table.new(4, 4)
	
	if self.joins and #self.joins > 0 then
		if self.userAlias then
			self._alias = self.userAlias
			allJoins[self._alias] = self
		else
			self._alias = '_A'			
		end

		alias = self._alias .. '.'
	end

	allJoins[self.m.__name] = self
	allJoins[#allJoins + 1] = self

	local cols = builder.buildColumns(self, sqls, alias)
	
	--joins fields
	if #alias > 0 then
		builder.buildJoinsCols(self, sqls, 1, #cols > 0 and true or false, allJoins)
	end

	--from
	sqls[#sqls + 1] = 'FROM'
	sqls[#sqls + 1] = model.__name
	if #alias > 0 then
		sqls[#sqls + 1] = self._alias
	end

	--joins conditions	
	if #alias > 0 then
		builder.buildJoinsConds(self, sqls, false, allJoins)
	
		--where
		local haveWheres = builder.buildWheres(self, sqls, 'WHERE (', alias, nil, allJoins)
		if haveWheres then
			sqls[#sqls + 1] = ')'
		end
		builder.buildWhereJoins(self, sqls, haveWheres, allJoins)
	else
		--where
		local haveWheres = builder.buildWheres(self, sqls, 'WHERE', alias, nil, allJoins)
		builder.buildWhereJoins(self, sqls, haveWheres, allJoins)
	end
	
	--order by
	builder.buildOrder(self, sqls, alias)
	--limit
	builder.buildLimits(self, sqls)
	
	--end
	allJoins = nil

	return table.concat(sqls, ' ')
end
	
builder.UPDATE = function(self)
	local sqls = {}
	local model = self.m
	
	sqls[#sqls + 1] = 'UPDATE'
	sqls[#sqls + 1] = model.__name
	
	--has join(s) then alias
	local alias, allJoins = '', table.new(4, 4)
	if self.joins and #self.joins > 0 then
		if self.userAlias then
			self._alias = self.userAlias
			allJoins[self._alias] = self
		else
			self._alias = '_A'			
		end
				
		alias = self._alias .. '.'
		sqls[#sqls + 1] = self._alias
	end	
	
	allJoins[self.m.__name] = self
	allJoins[#allJoins + 1] = self

	--joins fields
	if allJoins then
		builder.buildJoinsCols(self, nil, 1, false, allJoins)
		
		--joins conditions	
		builder.buildJoinsConds(self, sqls, false, allJoins)
	end
	
	--all values
	if builder.buildKeyValuesSet(self, sqls, alias, allJoins) > 0 then
		table.insert(sqls, #sqls, 'SET')
	else
		--空值UPDATE不执行
		return nil
	end
	
	--where
	local function _tryBuildWheres(that, keyType)
		local idx, vals = model.__fieldIndices, that.keyvals
		if type(vals) == 'table' then
			for k,v in pairs(idx) do
				if v.type == keyType then
					local v = vals[k]
					if v and v ~= ngx.null then
						--重新处理where条件
						builder.processWhere(that, 1, k, v)
						return builder.buildWheres(that, sqls, 'WHERE', alias, nil, allJoins)
					end
				end
			end
		end
	end
	
	if not builder.buildWheres(self, sqls, 'WHERE', alias, nil, allJoins) then
		if type(self.__where) == 'string' then
			sqls[#sqls + 1] = 'WHERE'
			sqls[#sqls + 1] = builder.processTokenedString(self, alias, self.__where, false, allJoins)
			
		elseif not _tryBuildWheres(self, 1) and not _tryBuildWheres(self, 2) then
			--find primary key
			error(string.format("Cannot do model(%s) update without condition(s)", model.__name))
			return false
		end
	end
	
	--order by
	if self.orderBy then
		sqls[#sqls + 1] = string.format('ORDER BY %s %s', self.orderBy.name, self.orderBy.order)
	end
	--limit
	builder.buildLimits(self, sqls, true)
	
	--end
	allJoins = nil
	
	return table.concat(sqls, ' ')
end

builder.INSERT = function(self)
	local sqls = {}
	local model = self.m
	
	sqls[#sqls + 1] = 'INSERT INTO'
	sqls[#sqls + 1] = model.__name

	local allJoins = {}
	allJoins[self.m.__name] = self
	allJoins[1] = self
	
	--all values
	if builder.buildKeyValuesSet(self, sqls, '', allJoins) > 0 then
		table.insert(sqls, #sqls, 'SET')
	else
		sqls[#sqls + 1] = '() VALUES()'
	end
	
	--end
	return table.concat(sqls, ' ')
end

builder.REPLACE = function(self)
	local sqls = {}
	local model = self.m
	
	sqls[#sqls + 1] = 'REPLACE INTO'
	sqls[#sqls + 1] = model.__name

	local allJoins = {}
	allJoins[self.m.__name] = self
	allJoins[1] = self
	
	--all values
	if builder.buildKeyValuesSet(self, sqls, '', allJoins) > 0 then
		table.insert(sqls, #sqls, 'SET')
	else
		sqls[#sqls + 1] = '() VALUES()'
	end
	
	--end
	return table.concat(sqls, ' ')
end
	
builder.DELETE = function(self)
	local sqls = {}
	local model = self.m
	
	sqls[#sqls + 1] = 'DELETE'
	sqls[#sqls + 1] = 'FROM'
	sqls[#sqls + 1] = model.__name
	
	local allJoins = table.new(1, 1)

	allJoins[self.m.__name] = self
	allJoins[#allJoins + 1] = self
	
	--where
	if not builder.buildWheres(self, sqls, 'WHERE', '', nil, allJoins) then
		if type(self.__where) == 'string' then
			sqls[#sqls + 1] = 'WHERE'
			sqls[#sqls + 1] = builder.processTokenedString(self, '', self.__where, false, allJoins)
		else
			--find primary or unique
			local haveWheres = false
			local idx, vals = model.__fieldIndices, self.keyvals
			if vals then
				for k,v in pairs(idx) do
					if (v.type == 1 or v.type == 2) and vals[k] then
						builder.processWhere(self, 1, k, vals[k])
						haveWheres = builder.buildWheres(self, sqls, 'WHERE', '', nil, allJoins)
						break
					end
				end
			end

			if not haveWheres then
				error("Cannot do model delete without condition(s)")
				return false
			end
		end
	end
	
	--limit
	builder.buildLimits(self, sqls, true)
	
	--end
	allJoins = nil
	
	return table.concat(sqls, ' ')
end

-----------------------------------------------------------------------------------------------------------------------
builder.buildColumns = function(self, sqls, alias, returnCols)
	local model = self.m

	--列名有缓存，则直接使用
	if self.colCache then
		if returnCols ~= true and #self.colCache > 0 then
			sqls[#sqls + 1] = self.colCache
		end
		return self.colCache
	end
	
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
	
	local cols, colAlias, n2 = nil, self.aliasAB or {}, nil
	if self.colSelects then
		--只获取某几列
		local plains = {}
		if excepts then
			for n,_ in pairs(self.colSelects) do
				if not excepts[n] then
					n2 = colAlias[n]					
					plains[#plains + 1] = n2 and (n .. ' AS ' .. n2) or n
				end
			end
		else
			for n,_ in pairs(self.colSelects) do
				n2 = colAlias[n]
				plains[#plains + 1] = n2 and (n .. ' AS ' .. n2) or n
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
				n2 = colAlias[n]
				fps[#fps + 1] = n2 and (n .. ' AS ' .. n2) or n
			end
		end

		cols = #fps > 0 and table.concat(fps, ',' .. alias) or '*'
		
	else
		--所有列
		cols = express and '' or '*'
	end	

	if express then
		cols = #cols > 0 and string.format('%s,%s%s', express, alias, cols) or express
	elseif #cols > 0 then
		cols = alias .. cols
	end

	self.colCache = cols
	if returnCols ~= true and #cols > 0 then
		sqls[#sqls + 1] = cols
	end
	return cols
end

builder.buildKeyValuesSet = function(self, sqls, alias, allJoins)
	local model = self.m
	local fieldCfgs = model.__fields
	local vals, keyvals = self.keyvals, table.new(0, 8)

	if not vals then
		error(string.format('mysqlbuilder operator "%s" failed, no key=>value(s) set', self.op))
	end
	if type(vals) == 'string' then
		sqls[#sqls + 1] = vals
		return 1
	end

	local op = allOps[self.op]
	local colAlias = self.aliasBA or {}	
	local fieldCfgs = self.colSelects == nil and model.__fields or self.colSelects

	for name,v in pairs(vals) do
		if type(name) == 'number' or (type(name) == 'string' and string.countchars(name, '0123456789') == #name) then
			keyvals[#keyvals + 1] = v
		else
			name = colAlias[name] or name

			local cfg = fieldCfgs[name]
			if cfg then				
				if cfg.ai and self.op == 3 and (not self.fullop or not string.checkinteger(v)) then
					--自增长型字段的值，如果是在insert下的话，就必须是fullCreate，否则被忽略

				elseif v == nil then
					--值为nil又不是UPDATE操作，且字段没有默认值又不可以为NULL，则根据字段类型给一个默认值，防止SQL出错
					if op ~= 2 and cfg.default == nil and not cfg.null then
						keyvals[#keyvals + 1] = cfg.type == 1 and "''" or '0'
					end

				else
					local strval, suggConn = buildSqlValue(self, cfg, v)				
					if cfg.type == 8 then
						--特殊的Json字段
						if v._func then
							local sql = { name, '=', v._func, '(', name, ',' }
							
							if v._path then
								sql[#sql + 1] = ngx.quote_sql_str(v._path)
								if strval then
									sql[#sql + 1] = ','
									sql[#sql + 1] = strval
								end
							else
								sql[#sql + 1] = strval
							end
						
							sql[#sql + 1] = ')'
							strval = table.concat(sql, '')
							
						elseif strval then
							strval = table.concat({ name, '=', strval }, '')
						end
						
						if strval then
							if alias and #alias > 0 then
								keyvals[#keyvals + 1] = alias .. strval
							else
								keyvals[#keyvals + 1] = strval
							end
						end
						
					elseif strval then
						--普通字段
						if suggConn == nil then
							v = builder.processTokenedString(self, alias, strval, false, allJoins)
						end

						if alias and #alias > 0 then
							keyvals[#keyvals + 1] = table.concat({ alias, name, '=', strval }, '')
						else
							keyvals[#keyvals + 1] = table.concat({ name, '=', strval }, '')
						end
					end
				end
			end
		end
	end

	sqls[#sqls + 1] = table.concat(keyvals, ',')
	return #keyvals
end


--如果condValues非nil，说明现在处理的不是self代表的query自己的条件
builder.buildWheres = function(self, sqls, condPre, alias, condValues, allJoins)
	local model = self.m
	if not alias then alias = '' end
	
	if self.__where then
		--如果有__where的话直接返回
		if condPre then
			sqls[#sqls + 1] = condPre
		end
		sqls[#sqls + 1] = self.__where
		
		return true
	end
	
	if not condValues then
		condValues = self.condValues
	end

	if condValues and #condValues > 0 then
		local ignoreNextCond = true
		local wheres, conds = {}, builder.conds
		local fieldCfg, merges = nil, table.new(4, 0)
		
		for i = 1, #condValues do
			local one, rsql = condValues[i], nil
			local onecond = one.c
			
			if ignoreNextCond then
				onecond = 1
				ignoreNextCond = false
			elseif i > 1 and onecond == 1 then
				--如果没有指定条件连接方式，那么当不是第1个条件的时候，就会自动修改为and
				onecond = 2
			end

			if one.sub then
				--子查询
				local subq = one.sub
				subq.limitStart, subq.limitTotal = nil, nil

				local expr = builder.processTokenedString(self, alias, one.n, false, allJoins)
				local subsql = builder.SELECT(subq, subq.m)
				
				if subsql then
					if one.purekn then
						rsql = string.format('%s%s IN(%s)', conds[onecond], expr, subsql)
					else
						rsql = string.format('%s%s(%s)', conds[onecond], expr, subsql)
					end
				end
				
			elseif one.expr == '(' then
				--左括号左边的条件保留，后面的条件扔掉
				rsql = conds[onecond] .. one.expr
				ignoreNextCond = true
			elseif one.expr == ')' then
				--右括号左边的条件扔掉
				rsql = one.expr
			else
				--按照表达式进行解析
				local lastToken, lastEq, suggCon, removeEq

				merges[1] = conds[onecond]
				merges[2], fieldCfg, lastToken, lastEq = builder.processTokenedString(self, alias, one.expr, one.purekn, allJoins)

				if one.value then
					assert(fieldCfg ~= nil, string.format("Field not exists! table name=%s, op=%s, expr=%s", model.__name, self.op, one.expr))

					merges[4], suggCon, removeEq = buildSqlValue(self, fieldCfg, one.value, false, lastEq)

					if merges[4] then
						merges[3] = ''
						if suggCon == nil then
							--解析表达式
							merges[4] = builder.processTokenedString(self, alias, merges[4], false, allJoins)				
							suggCon = '='
						end

						if mysqlwords[lastToken] == 1 --[[and string.byte(one.expr, #one.expr) ~= 40 ]]then
							--最后一个符号为函数调用，那么就自动的为其加上括号。这种情况只有name和val分开写的时候才会运行到这里来，正因为分开写了，所以括号只能在这里自动的加							
							merges[3], merges[4] = merges[3] .. '(', merges[4] .. ')'
						elseif removeEq then
							--对某些等式的特殊处理
							assert(suggCon)
							merges[2] = string.replace(merges[2], lastEq, '')
							merges[3] = suggCon
						else
							--如果expr是一个纯字段名，那就要加上建议的连接符号
							merges[3] = one.purekn and suggCon .. ' ' or ' '
						end

						rsql = table.concat(merges, '')
					else
						--值出错
						rsql = one.expr .. '#ERR#'
					end
				else
					merges[3], merges[4] = nil, nil
					rsql = table.concat(merges, '')
				end
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

builder.buildWhereJoins = function(self, sqls, haveWheres, allJoins)
	local cc = self.joins == nil and 0 or #self.joins
	if cc < 1 then
		return
	end

	for i = 1, cc do
		local j = self.joins[i]
		local q, prefix, postfix = j.q, 'WHERE', ''
		
		if haveWheres then
			prefix = j.cond .. ' ('
			postfix = ')'
		end

		if builder.buildWheres(q, sqls, prefix, q._alias .. '.', nil, allJoins) then
			if haveWheres then
				sqls[#sqls + 1] = postfix
			end
			haveWheres = true
		end
		
		builder.buildWhereJoins(q, sqls, haveWheres, allJoins)
	end
end

builder.buildJoinsCols = function(self, sqls, indient, haveCols, allJoins)
	local cc = self.joins == nil and 0 or #self.joins
	if cc < 1 then
		return
	end

	for i = 1, cc do
		local cols = nil
		local q = self.joins[i].q

		if q.userAlias then
			q._alias = q.userAlias
			allJoins[q._alias] = q
		else
			q._alias = ('_' .. string.char(65 + indient))
		end
		allJoins[q.m.__name] = q
		allJoins[#allJoins + 1] = q

		if sqls then
			cols = builder.buildColumns(q, sqls, q._alias .. '.', true)			
			if #cols > 0 then
				if haveCols then
					sqls[#sqls + 1] = ','
				end
				
				sqls[#sqls + 1] = cols
				cols, haveCols = true, true
			end
		end
		
		local newIndient = builder.buildJoinsCols(q, sqls, indient + 1, haveCols, allJoins)
		indient = newIndient or (indient + 1)
	end
	
	return indient
end

builder.buildJoinsConds = function(self, sqls, haveOns, allJoins)
	local cc = self.joins == nil and 0 or #self.joins
	if cc < 1 then
		return
	end
	
	for i = 1, cc do
		local join = self.joins[i]
		local q = join.q

		sqls[#sqls + 1] = builder.validJoins[join.type]
		sqls[#sqls + 1] = q.m.__name
		sqls[#sqls + 1] = q._alias

		local defon = true
		if join.type == 'cross' then
			defon = false
		else
			sqls[#sqls + 1] = 'ON('
		end		

		local pos = #sqls
		if q.onValues == nil or not builder.buildWheres(q, sqls, nil, q._alias .. '.', q.onValues, allJoins) then
			if defon then
				sqls[#sqls + 1] = '1)'
			end

		else
			if not defon then
				table.insert(sqls, #sqls, 'ON(')
			end
			sqls[#sqls + 1] = ')'
		end
		
		if builder.buildJoinsConds(q, sqls, haveOns, allJoins) then
			haveOns = true
		end
	end
	
	return haveOns
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
		if ignoreStart or self.limitStart <= 0 then
			sqls[#sqls + 1] = string.format('LIMIT %u', self.limitTotal)
		else
			sqls[#sqls + 1] = string.format('LIMIT %u,%u', self.limitStart, self.limitTotal)
		end
	end
end

return builder