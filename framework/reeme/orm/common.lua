local validTypes = { s = 1, i = 2, n = 3, b = 4, d = 5, t = 6 }
local validIndex = { primary = 1, unique = 2, index = 3 }

return {
	parseFields = function(m)
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
						return 'find the second auto-increasement field'
					end
					
					aiExists = true
					isai = true
				end
				
				local maxl = v:match('((%d+))')
				if maxl then
					local t = validTypes[v:sub(1, 1)]
					if not t then
						return string.format('the data type [%s] of field(%s) is invalid', v:sub(1, 1), k) 
					end
					
					local defv = v:find(')')
					if defv and #v > defv then
						defv = v:sub(defv + 1)
					else
						defv = nil
					end

					local newf = { maxlen = tonumber(maxl), ai = isai, null = allownull, type = t, default = defv, colname = k }
					
					--date/datetime强制转为string型，然后再打标记
					if t == 5 then
						newf.type = 1
						newf.maxlen = 10
						newf.isDate = true
					elseif t == 6 then
						newf.type = 1
						newf.maxlen = 19
						newf.isDate, newf.isDateTime = true, true
					end
					
					--如果是数值型并且maxl超过11位，就认为是64位整数
					if t == 2 and newf.maxlen > 11 then
						newf.isint64 = true
					end

					fields[k] = newf
					plains[#plains + 1] = k
				else
					return string.format('no max-length of field(%s)', k)
				end
			end
		end
		
		if m.indices then
			for k,v in pairs(m.indices) do
				local tp = validIndex[v]
				if tp ~= nil then
					local cfg = fields[k]				
					local idx = { type = tp }
					
					if cfg and cfg.ai then
						idx.autoInc = true
					end
					
					indices[k] = idx
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