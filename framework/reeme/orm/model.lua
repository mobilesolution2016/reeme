local queryMeta = nil
local resultPub = require('reeme.orm.result')
local allConds = { ['AND'] = 2, ['OR'] = 3, ['XOR'] = 4, ['NOT'] = 5, ['and'] = 2, ['or'] = 3, ['xor'] = 4, ['not'] = 5 }

--query的原型类
queryMeta = {
	__index = {
		--全部清空
		reset = function(self)
			local ignores = { m = 1, op = 1, R = 1, builder = 1, keyvals = 1 }
			for k,_ in pairs(self) do
				if not ignores[k] then
					self[k] = nil
				end
			end
			
			self.limitStart, self.limitTotal = 0, 1
			return self
		end,
		
		--设置条件
		where = function(self, name, val)
			self.setWheres, self.setOns = true, false
			return self.builder.processWhere(self, 1, name, val)
		end,
		andWhere = function(self, name, val)
			self.setWheres, self.setOns = true, false
			return self.builder.processWhere(self, 2, name, val)
		end,
		orWhere = function(self, name, val)
			self.setWheres, self.setOns = true, false
			return self.builder.processWhere(self, 3, name, val)
		end,
		xorWhere = function(self, name, val)
			self.setWheres, self.setOns = true, false
			return self.builder.processWhere(self, 4, name, val)
		end,
		notWhere = function(self, name, val)
			self.setWheres, self.setOns = true, false
			return self.builder.processWhere(self, 5, name, val)
		end,
		clearWheres = function(self)
			self.condValues, self.condString = nil, nil
			self.setWheres, self.setOns = true, false
			return self
		end,
		
		--设置join on条件
		on = function(self, name, val)
			self.setOns, self.setWheres = true, false
			return self.builder.processOn(self, 1, name, val)
		end,
		andOn = function(self, name, val)
			self.setOns, self.setWheres = true, false
			return self.builder.processOn(self, 2, name, val)
		end,
		orOn = function(self, name, val)
			self.setOns, self.setWheres = true, false
			return self.builder.processOn(self, 3, name, val)
		end,
		xorOn = function(self, name, val)
			self.setOns, self.setWheres = true, false
			return self.builder.processOn(self, 4, name, val)
		end,
		notOn = function(self, name, val)
			self.setOns, self.setWheres = true, false
			return self.builder.processOn(self, 5, name, val)
		end,
		clearOns = function(self)
			self.onValues = nil
			self.setOns, self.setWheres = true, false
			return self
		end,
		
		--where/on中增加一个左括号
		lb = function(self, cond)
			self.brackets = self.brackets and (self.brackets + 1) or 1

			if self.setOns then
				return self.builder.processOn(self, allConds[cond] or 2, '(')
			end
			return self.builder.processWhere(self, allConds[cond] or 2, '(')
		end,
		--where/on中增加一个右括号
		rb = function(self)
			if not self.brackets or self.brackets < 1 then
				error("error for add ')' without paired '('")
			end
			self.brackets = self.brackets - 1
			
			if self.setOns then
				return self.builder.processOn(self, 1, ')')
			end
			return self.builder.processWhere(self, 1, ')')
		end,
		
		--设置只操作哪些列，如果不设置，则会操作model里的所有列。同时会将只排除的列清空掉
		columns = function(self, names)
			if not self.colSelects then
				self.colSelects = table.new(0, 8)
			end
			self.colExcepts = nil
			self.colCache = nil

			local tp = type(names)
			local fields = self.m.__fields
			
			if tp == 'string' then
				if names == '*' then
					self.colSelects = nil
					
				elseif #names > 0 then
					local splits = string.split(names, ',', string.SPLIT_TRIM)
					for i = 1, #splits do
						--取出as重命名
						local n, nto = string.cut(splits[i], ' ')
						if nto and string.ncasecmp(nto, 'AS ', 3) then
							nto = nto:sub(4)
						end

						local f = fields[n]
						if f then
							self.colSelects[n] = f
							if nto then
								--添加重命名
								self:alias(n, nto)
							end
						else
							error('model columns function set a not exists field:' .. n)
						end
					end
				end
				
			elseif tp == 'table' then
				for i = 1, #names do
					--取出as重命名
					local n, nto = string.cut(names[i], ' ')
					if nto and string.ncasecmp(nto, 'AS ', 3) then
						nto = nto:sub(4)
					end
					
					local f = fields[n]
					if f then
						self.colSelects[n] = f
						if nto then
							--添加重命名
							self:alias(str, nto)
						end
					else
						error('model columns function set a not exists field:' .. n)
					end
				end
			end
			
			return self
		end,
		--设置只排除哪些列，同时会将只选择哪些列清空掉
		excepts = function(self, names)
			if not self.colExcepts then
				self.colExcepts = table.new(0, 8)
			end
			self.colSelects = nil
			self.colCache = nil
			
			local tp = type(names)
			local fields = self.m.__fields
			
			if tp == 'string' then
				for str in string.gmatch(names, '([^,]+)') do
					if not fields[str] then
						error('model excepts function set a not exists field:' .. str)
					end
					
					self.colExcepts[str] = true
				end
				
			elseif tp == 'table' then
				for i = 1, #names do
					local n = names[i]
					if not fields[n] then
						error('model excepts function set a not exists field:' .. n)
					end
					
					self.colExcepts[n] = true
				end
			end
			
			return self
		end,
		--使用列表达式。不带参数的调用就是清除表达式
		expr = function(self, expr)
			if expr then
				if not self.expressions then
					self.expressions = table.new(4, 0)
				end
				self.expressions[#self.expressions + 1] = expr
			else
				self.expressions = nil
			end		
			self.colCache = nil
			
			return self
		end,
		--清除掉所有的列和表达式
		clearColumns = function(self, withJoins)
			self.expressions, self.colSelects, self.colExcepts, self.colCache = nil, nil, nil, nil
			if withJoins and self.joins then
				for i = 1, #self.joins do
					self.joins[i]:clearColumns(self, withJoins)
				end
			end
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

			local tbname = query.m.__name
			local j = { q = query, type = jt, on = self.joinOn }
			
			if not self.joins then
				self.joins = { j }
			else
				table.insert(self.joins, j)
			end
			
			return self
		end,
		leftjoin = function(self, query) return self:join(query, 'left') end,
		rightjoin = function(self, query) return self:join(query, 'right') end,
		
		--设置别名，如果不设置，则将使用自动别名，自动别名的规则是_C[C>=A && C<=Z]，在设置别名的时候请不要与自动别名冲突
		--如果没有参数3，则设置的是表的别名，否则就是设置的字段的别名。不带任何参数的调用可以取消所有的别名
		alias = function(self, name, aliasTo)
			if name then
				if aliasTo then
					--字段别名
					local baseab, baseba = self.m.__fieldAlias
					
					if not self.aliasAB then
						self.aliasAB = table.new(0, 8)
						self.aliasBA = table.new(0, 8)
					elseif baseab then
						baseab, baseba = baseab.ab, baseab.ba
						if self.aliasAB == baseab then
							self.aliasAB = table.clone(baseab)
							self.aliasBA = table.clone(baseba)
						end
					end

					baseab, baseba = self.aliasAB, self.aliasBA
					local setone = function(n, to)
						local old = baseab[n]
						if old then
							baseba[old] = nil
						end

						baseab[n] = to
						baseba[to] = n
					end
					
					if type(name) == 'string' then
						setone(name, aliasTo)
					else
						assert(type(aliasTo) == 'table')
						
						for i = 1, #name do
							setone(name[i], aliasTo[i])
						end
					end
		
				elseif #name > 0 and name ~= self.m.__name then
					--表的别名
					local chk = name:match('_[A-Z]')
					if not chk or #chk ~= #name then
						self.userAlias = name
					end
				end
			else
				--全部取消
				self.userAlias = nil
				self.aliasAB, self.aliasBA = nil, nil
			end
			
			return self
		end,
		
		--可按照原名或别名获取字段
		getField = function(self, n)
			local fs = self.m.__fields
			local f = fs[n]
			if not f and self.aliasAB then
				f = fs[self.aliasAB[n]]
			end
			return f
		end,
		
		--获取表名（如果表被别名过，那么返回的是别名）
		name = function(self)
			return self.userAlias or self.m.__name
		end,
		--返回原始表名忽略别名（如果有的话）
		sourceName = function(self)
			return self.m.__name
		end,
		
		--设置排序
		order = function(self, field, asc)
			if field:find('[,]+') then
				self.orderBy = field
			else
				if not asc then
					field, asc = field:split(' ', string.SPLIT_TRIM, true)
					if asc == nil then asc = 'asc' end
				end
				
				if field and self.m.__fields[field] and asc then
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
					db = self.R(db)
				end
			else
				local m = self.m
				db = m.__db or self.R(m.__builder.dbTypeName .. 'db')
			end
			if not db then
				return nil
			end
			
			if self.brackets then
				--关闭括号
				repeat
					self:rb()
					self.brackets = self.brackets - 1
				until self.brackets == 0
			end
			
			local setvnil = false
			if not self.keyvals and result then
				self.keyvals = result(false)
				setvnil = true
			end
			
			local sqls = self.builder[self.op](self, db)
			if sqls then
				result = resultPub.init(result, self.m)
				res = resultPub.query(result, db, sqls, self.limitTotal or 1)

				self.lastSql = sqls
				if res then
					logger.d(sqls, ':insertid=', tostring(res.insert_id), ',affected=', res.affected_rows)
				else
					logger.d(sqls, ':error')
				end
			end
			
			if setvnil then
				self.keyvals = nil
			end
			
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
		
		--执行select查询并返回所有行，注意返回是所有行而非结果集实例，并且在没有结果集或不是查询指令的时候返回的是一个空的table而非nil
		fetchAll = function(self, db, result)
			if self.op == 'SELECT' then
				local res, r = self:execute(db, result)	
				if res then
					return r + 1 and r(true) or {}
				end
			end
			return {}
		end,
		--执行select查询并返回第一行，注意返回的是行而非结果集实例，如果不存在至少1行，则返回为nil
		fetchFirst = function(self, db, result)
			if self.op == 'SELECT' then
				local res, r = self:limit(1):execute(db, result)
				if res then
					return r + 1 and r(false) or nil
				end
			end
		end,
		--执行select查询并返回第一行的唯一一个值，注意返回的是行而非结果集实例，如果不存在至少1行，则返回为nil
		fetchSingle = function(self, db, result)
			if self.op == 'SELECT' then
				local res, r = self:limit(1):execute(db, result)
				if res and r + 1 then
					r = r(false)
					for _,v in pairs(r) do
						return v
					end
				end
			end
		end,
		--执行select查询并获取所有的行，然后将这些行的指定的一个字段形成为一个新的table返回，如果没有结果集或不是查询指令时返回一个空的table而非nil
		fetchAllFirst = function(self, colname, db, result)
			if self.op == 'SELECT' then
				local res, r = self:columns(colname):execute(db, result)	
				if res and r + 1 then
					r = r(true)

					local s = table.new(#r, 0)
					for i = 1, #r do
						s[i] = r[i][colname]
					end
					
					return s
				end
			end
			return {}
		end,
	}
}


--model的原型表，提供了所有的model功能函数
--[[
	以下函数名字类似的较多，下面是一些规则：
	1、所有findAll系列的函数，如findAll、findAllWithNames、findAllFirst返回的值都是一个table而不是结果集，并且在没有查找到任何结果的时候返回是一个空table而不是nil
	2、所有的findFirst系列的函数，如findFirst、findFirstWithNames返回的值都是一个table而不是结果集，并且在没有查找到任何结果的时候返回是一个nil
	3、只有find函数返回的是结果集
	
	一般建议使用query()函数构建一个查询器然后自己限定各种条件而尽量不要使用find函数，因为find函数会返回所有的列
]]
local modelMeta = {
	__index = {
		--从模板新建结果集实例，建立的同时可从现有的一个结果集copyfrom复制数据，复制的时候可以指定只复制哪些字段fieldnames，最后还可以使用newvals中的值优先覆盖copyfrom中的指定字段
		new = function(self, copyfrom, fieldnames, newvals)	
			local r = resultPub.init(nil, self)
			if not copyfrom then
				return r
			end
			
			--从copyfrom复制
			local keys = nil
			local names = self.__fieldsPlain
			if fieldnames then
				local tp = type(fieldnames)

				keys = table.new(0, #names / 2)
				if tp == 'string' then
					string.split(fieldnames, ',', string.SPLIT_ASKEY, keys)
				elseif tp == 'table' then
					for i = 1, #fieldnames do
						keys[fieldnames[i]] = true
					end
				else
					return
				end
				
				--default for add all unique key(s)
				for k,v in pairs(self.__fieldIndices) do
					if v.type == 1 or v.type == 2 then
						keys[k] = true
					end
				end
			end
						
			if newvals then
				--优先从newvals中取
				for i = 1, #names do
					local n = names[i]
					if not keys or keys[n] then
						r[n] = newvals[n] or copyfrom[n]
					end
				end
			else
				for i = 1, #names do
					local n = names[i]
					if not keys or keys[n] then
						r[n] = copyfrom[n]
					end
				end
			end
			
			r.__db = self.__db
			return r
		end,
		
		--将一个数组型的table包装成一个结果集
		wrap = function(self, result)
			if type(result) ~= 'table' or #result < 1 then
				return nil
			end
			
			local r = resultPub.init(nil, self)
			return resultPub.wrap(r, result)
		end,

		--建立一个查询器
		query = function(self, op)
			local q = { m = self, R = self.__reeme, op = op ~= nil and string.upper(op) or 'SELECT', builder = self.__builder }
			local alias = self.__fieldAlias
			if alias then
				q.aliasAB = alias.ab
				q.aliasBA = alias.ba
			end

			return setmetatable(q, queryMeta)
		end,		
		
		--按条件查找并返回所有的结果（其实不指定limit也是默认只是前50条而非真的全部），注意本函数返回为结果集实例而非结果行数组
		find = function(self, p1, p2, p3, p4)
			local q = self:query()			
			q.limitTotal = nil
			
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

		--建立一个执行表达式的查询器而忽略其它行或列
		expression = function(self, expr)
			return self:query():expr(expr):columns()
		end,
		
		--和find一样，只不过返回的是结果行而非结果集实例，并且在没有任何结果的时候也不会返回Nil而是返回一个空table
		findAll = function(self, p1, p2, p3, p4)
			local r = self:find(p1, p2, p3, p4)
			return r and r(true) or {}
		end,
		--和findAll一样，只不过可以限制只返回某些列而不是所有列
		findAllWithNames = function(self, colnames, p1, p2, p3, p4)
			local q = self:query()			
			q.limitTotal = nil
			
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
			
			return q:columns(colnames):fetchAll()
		end,
		--和findFirst一样，但是仅查找第1行的指定的某1列，返回的将是一个所有行的这一列组成的一个新table(注意不是结果集实例)，并在没有结果集的时候并不会返回nil
		findAllFirst = function(self, colname, name, val)
			local q = self:query()			
			q.limitTotal = nil

			if name then q:where(name, val) end
			return q:fetchAllFirst(colname)
		end,
		
		--和find一样但是仅查找第1行，其实就是find加上limit(1)
		findFirst = function(self, name, val)
			local q = self:query()
			if name then q:where(name, val) end
			return q:limit(1):exec()
		end,
		--和find一样但是仅查找第1行，并且限制查找时的列名（不会将所有列都取出），如果列名只有1个，那么返回的将是单值或nil
		findFirstWithNames = function(self, colnames, name, val)
			local q = self:query()

			if name then q:where(name, val) end
			q = q:columns(colnames):limit(1):exec()
			
			if string.find(colnames, ',', 1, true) then
				return q and q(false) or nil
			end
			return q and q[colnames] or nil
		end,
		
		--建立一个delete查询器。条件可以直接给出一个语句或不给，也可以用where/xxxWhere来给出
		delete = function(self, where)
			local q = self:query('DELETE')
			q.__where = where
			return q
		end,
		--建立一个delete查询器，只需要给出主键值。返回true表示操作成功，否则返回false。如果模型没有定义主键，那么直接报错
		deleteBy = function(self, val)
			assert(val ~= nil, 'call model.deleteBy with nil value')

			for k,v in pairs(self.__fieldIndices) do
				if v.type == 1 then
					local q = self:query('DELETE')
					local tp, field = type(val), self.__fields[k]
					
					if tp == 'table' then
						assert(#val >= 1)
						q.__where = table.concat({field.colname, val[1]}, ' ')
						
					elseif tp == 'cdata' then
						local newv 
						val, newv = string.checkinteger(val)
						if newv then
							q.__where = string.format('%s=%s', newv)
						end
					else
						q.__where = string.format('%s=%s', tostring(val))
					end
					
					q = q:limit(1):exec()
					if q and q.rows == 1 then
						return true
					end
					
					return false
				end
			end
			
			error('the must a primary key declared first when call model.deleteBy')
		end,
		--建立一个update查询器，必须给出要更新的值的集合。条件可以直接给出一个语句或不给，也可以用where/xxxWhere来给出
		update = function(self, vals, where)
			assert(type(vals) == 'table' or type(vals) == 'string')
			
			local q = self:query('UPDATE')
			q.keyvals = vals
			q.__where = where
			return q
		end,
		
		--按名称字段配置
		getField = function(self, name)
			return self.__fields[name]
		end,
		--获取字段类型的字符串表示
		getFieldType = function(self, name)
			local f = self.__fields[name]
			if f then
				local typeStrings = { 'string', 'integer', 'number', 'boolean' }
				if f.isDate then
					return f.isDatetime and 'datetime' or 'date'
				end
				return typeStrings[f.type]
			end
		end,
		
		--判断字段是否可以为null
		isFieldNullable = function(self, name)
			local f = self.__fields[name]
			return f and f.null or false
		end,
		--寻找自增字段如果找到返回其配置否则返回nil
		findAutoIncreasementField = function(self)
			for n,f in pairs(self.__fieldIndices) do
				if f.autoInc then
					return self.__fields[n]
				end
			end
		end,
		--寻找一个unique字段或primary key字段，找到返回其配置否则返回nil
		findUniqueKey = function(self)
			for k,v in pairs(self.__fieldIndices) do
				if v.type == 1 or v.type == 2 then
					return k
				end
			end
		end,
		
		--验证给定的长度是否超过了字段配置的限制
		validlen = function(self, name, value, minlength, utf8Enc)
			local f = self.__fields[name]
			if not f then
				error(string.format("valid value length by field config failed, field name '%s' not exists", name))
			end
			if type(value) ~= 'string' then
				if value == nil then
					error(string.format("valid value length by field config failed, field name '%s', value is nil", name))
				end
				value = tostring(value)
			end
			
			local l = #value
			if l > f.maxlen then
				return false
			end			
			if minlength then
				if utf8Enc then
					return utf8str.len(value) >= minlength
				end
				return l >= minlength
			end
			
			return true
		end,

		--验证给定的值是否是enum字段配置的许可值
		validenum = function(self, name, value)
			local f = self.__fields[name]
			if not f or not f.enums then
				error(string.format("valid enum value by field config failed, field name '%s' not exists or not a enum field", name))
			end
			if type(value) ~= 'string' then
				if value == nil then
					error(string.format("valid enum value by field config failed, field name '%s', value is nil", name))
				end
				value = tostring(value)
			end

			if f.enums[value] then
				return true
			end
			
			if value:byte(1) ~= 39 or value:byte(#value - 1) ~= 39 then
				value = string.format("'%s'", value)
				return f.enums[value] and true or false
			end
			
			return false
		end,
		
		__queryMetaTable = queryMeta
	}
}

--当mode:findByXXXX或者model:findFirstByXXXX被调用的时候，只要XXXX是一个合法的字段名称，就可以按照该字段进行索引。下面的两个meta table一个用于模拟
--函数调用，一个用于生成一个模拟器
local simFindFuncMeta = {
	__call = function(self, value, p1, p2)
		local func = modelMeta.__index[self.of and 'find' or 'findFirst']
		return func(self.m, self.f, value, p1, p2)
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
				local call = { m = self, f = field, of = of }
				return setmetatable(call, simFindFuncMeta)
			end
		end
	end
}

setmetatable(modelMeta.__index, modelMeta2)

return modelMeta