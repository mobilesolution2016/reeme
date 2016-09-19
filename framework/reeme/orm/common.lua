local validTypes = { s = 1, i = 2, n = 3, b = 4, d = 5, t = 6, e = 7 }
local validIndex = { primary = 1, unique = 2, index = 3 }

return {
	parseFields = function(m, modelName)
		local aiExists = false
		local fields, plains, indices = {}, {}, {}
		
		for k,v in pairs(m.fields) do
			if type(k) == 'string' then
				local first = v:byte()
				local isai, allownull = false, false
				
				if first == 35 then --#
					v = v:sub(2)
					allownull = true
				elseif first == 42 then --*
					v = v:sub(2)
					if aiExists then
						error(string.format('use(%s) find the second auto-increasement field', modelName))
					end
					
					aiExists = true
					isai = true
				end
				
				first = v:byte(2)
				if first ~= 40 then
					error(string.format('use(%s) syntax error, expet a ( after type when parse field "%s"', modelName, k))
				end

				--类型值
				local t = validTypes[v:sub(1, 1)]
				if not t then
					return string.format('use(%s) the data type [%s] of field(%s) is invalid', modelName, v:sub(1, 1), k) 
				end
				
				--默认值开始位置
				local defv = string.plainfind(v, ')', 3)
				if not defv then
					error(string.format('use(%s) syntax error, expet a ) before default value when parse field "%s"', modelName, k))
				end
				
				--括号里面的定义
				local decl = v:sub(3, defv - 1)
				if not decl or #decl < 1 then
					error(string.format('use(%s) syntax error, expet declaration in t(...) when parse field "%s"', modelName, k))
				end

				--取出默认值
				if defv and #v > defv then
					defv = v:sub(defv + 1)
				else
					defv = nil
				end

				local newf = { ai = isai, null = allownull, type = t, default = defv, colname = k }
									
				if t == 5 then
					--date/datetime强制转为string型，然后再打标记
					newf.type = 1
					newf.maxlen = 10
					newf.isDate = true
				elseif t == 6 then
					--date/datetime强制转为string型，然后再打标记
					newf.type = 1
					newf.maxlen = 19
					newf.isDate, newf.isDateTime = true, true
				elseif t == 7 then
					--枚举类型，转字符串
					newf.type = 1
					newf.maxlen = 0
					newf.enums = string.split(decl, ',', string.SPLIT_TRIM + string.SPLIT_ASKEY)

					for i = 1, #newf.enums do
						newf.maxlen = math.max(newf.maxlen, #newf.enums[i])
					end

					--检测默认值是否合法
					if defv and not newf.enums[defv] then
						error(string.format('use(%s) error default enum value when parse field "%s", value "%s" not in declarations', modelName, k, defv))
					end
				end
				
				if newf.maxlen == nil and string.countchars(decl, '0123456789') == #decl then
					--指定了有效的长度
					newf.maxlen = tonumber(decl)
				end
				
				--如果是数值型并且maxl超过11位，就认为是64位整数
				if t == 2 and newf.maxlen > 11 then
					newf.isint64 = true
				end

				fields[k] = newf
				plains[#plains + 1] = k
			end
		end
		
		if m.indices then
			for k,v in pairs(m.indices) do
				local tp = validIndex[v]
				if tp ~= nil then
					local cfg = fields[k]
					if not cfg then
						error(string.format('use(%s) error index configuration because field name "%s" not exists', modelName, k))
					end
					
					indices[k] = { type = tp, autoInc = cfg.ai }
				else
					error(string.format('use(%s) error index type "%s"', modelName, v))
				end
			end
		end
		
		if #plains > 0 then
			m.__fields = fields
			m.__fieldsPlain = plains
			m.__fieldIndices = indices
			return true
		end
		
		return 'may be no valid field or field(s) declaration error'
	end
}