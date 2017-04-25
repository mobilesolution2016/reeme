local validTypes = { s = 1, i = 2, n = 3, b = 4, d = 5, t = 6, e = 7, j = 8 }
local validIndex = { primary = 1, unique = 2, index = 3 }

return {
	parseFields = function(m, modelName)
		local aiExists = false
		local fields, plains, indices, alias = {}, {}, { }, { ab = {}, ba = {}, cc = 0 }
		
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

				--����ֵ
				local t = validTypes[v:sub(1, 1)]
				if not t then
					return string.format('use(%s) the data type [%s] of field(%s) is invalid', modelName, v:sub(1, 1), k) 
				end
				
				--Ĭ��ֵ��ʼλ��
				local defv = string.find(v, ')', 3, true)
				if not defv then
					error(string.format('use(%s) syntax error, expet a ) before default value when parse field "%s"', modelName, k))
				end
				
				--��������Ķ���
				local decl = v:sub(3, defv - 1)
				if not decl then
					error(string.format('use(%s) syntax error, expet declaration in s|i|n|b|d|t|e(...) when parse field "%s"', modelName, k))
				end

				--ȡ��Ĭ��ֵ
				if defv and #v > defv then
					defv = v:sub(defv + 1)
				else
					defv = nil
				end

				local newf = { ai = isai, null = allownull, type = t, default = defv, colname = k }
									
				if t == 5 then
					--date/datetimeǿ��תΪstring�ͣ�Ȼ���ٴ���
					newf.type = 1
					newf.maxlen = 10
					newf.isDate = true
				elseif t == 6 then
					--date/datetimeǿ��תΪstring�ͣ�Ȼ���ٴ���
					newf.type = 1
					newf.maxlen = 19
					newf.isDate, newf.isDateTime = true, true
				elseif t == 7 then
					--ö�����ͣ�ת�ַ���
					newf.type = 1
					newf.maxlen = 0
					newf.enums = string.split(decl, ',', string.SPLIT_TRIM)

					for i = 1, #newf.enums do
						local n = newf.enums[i]
						newf.enums[n] = i
						newf.maxlen = math.max(newf.maxlen, #newf.enums[i])
					end

					--���Ĭ��ֵ�Ƿ�Ϸ�
					if defv and not newf.enums[defv] then
						error(string.format('use(%s) error default enum value when parse field "%s", value "%s" not in declarations', modelName, k, defv))
					end
				end
				
				if newf.maxlen == nil then
					--ָ������Ч�ĳ���
					newf.maxlen = string.checkinteger(decl) or 0
				end
				
				--�������ֵ�Ͳ���maxl����11λ������Ϊ��64λ����
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
		
		if m.alias then
			local aliascc = 0
			local ab, ba = table.new(0, 8), table.new(0, 8)
			for k,v in pairs(m.alias) do
				ab[k] = v
				ba[v] = k
				aliascc = aliascc + 1
			end
			
			if aliascc then
				alias = { ab = ab, ba = ba, cc = aliascc }
			end
		end
		
		if #plains > 0 then
			m.__fields = fields
			m.__fieldsPlain = plains
			m.__fieldIndices = indices
			m.__fieldAlias = alias
			return true
		end
		
		return 'may be no valid field or field(s) declaration error'
	end
}