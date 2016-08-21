local filters = {
	string = function(self)
		return function(value)
			return tostring(value), true
		end
	end,
	
	number = function(self)
		return function(value)
			return tonumber(value), true
		end
	end,
	
	between = function(self, min, max)
		if not max then max = min end
		if max < min then min, max = max, min end
		return function(value)
			return value >= min and value <= max
		end
	end,
	
	outside = function(self, min, max)
		if not max then max = min end
		if max < min then min, max = max, min end
		return function(value)
			return value < min and value > max
		end
	end,
	
	divisible = function(self, number)
		return function(value)
			if type(value) ~= "number" or type(number) ~= "number" then return false end 
			return value % number == 0
		end
	end,
	
	indivisible = function(self, number)
		return function(value)
			if type(value) ~= "number" or type(number) ~= "number" then return false end 
			return value % number ~= 0
		end
	end,
	
	equals = function(self, equal)
		return function(value)
			return value == equal
		end
	end,
	
	unequals = function(self, unequal)
		return function(value)
			return value == unequal
		end
	end,
	
	oneof = function(self, ...)
		local n = select("#", ...)
		local args = { ... }
		return function(value)
			for i = 1, n do
				if value == args[i] then
					return true
				end
			end
			return false
		end
	end,
	
	noneof = function(self, ...)
		local n = select("#", ...)
		local args = { ... }
		return function(value)
			for i = 1, n do
				if value == args[i] then
					return false
				end
			end
			return true
		end
	end,
	
	match = function(self, pattern, init)
		return function(value)
			return string.match(value, pattern, init) ~= nil
		end
	end,
	
	unmatch = function(self, pattern, init)
		return function(value)
			return string.match(value, pattern, init) == nil
		end
	end,
	
	starts = function(self, starts)
		return function(value)
			return string.sub(value, 1, string.len(starts)) == starts
		end
	end,	
	
	ends = function(self, ends)
		return function(value)
			return ends == '' or string.sub(value, -string.len(ends)) == ends
		end
	end,	
	
	email = function(self)
		return function(value)
			if value == nil or type(value) ~= "string" then return false end
			local i, at = string.find(value, "@", 1, true), nil
			while i do
				at = i
				i = string.find(value, "@", i + 1)
			end

			if not at or at > 65 or at == 1 then return false end
			local lp = string.sub(value, 1, at - 1)
			if not lp then return false end
			local dp = string.sub(value, at + 1)
			if not dp or #dp > 254 then return false end
			local qp = string.find(lp, '"', 1, true)
			if qp and qp > 1 then return false end
			local q, p
			for i = 1, #lp do
				local c = string.sub(lp, i, i)
				if c == "@" then
					if not q then return false end
				elseif c == '"' then
					if p ~= [[\]] then
						q = not q
					end
				elseif c == " " or c == '"' or c == [[\]] then
					if not q then
						return false
					end
				end
				p = c
			end
			if q or string.find(lp, "..", 1, true) or string.find(dp, "..", 1, true) then return false end
			if string.match(lp, "^%s+") or string.match(dp, "%s+$") then return false end
			return string.match(value, "%w*%p*@+%w*%.?%w*") ~= nil
		end
	end,
	
	min = function(self, min)
		return function(value)
			if type(value) ~= "number" then return false end 
			return value >= min
		end
	end,
	
	max = function(self, max)
		return function(value)
			if type(value) ~= "number" then return false end 
			return value <= max
		end
	end,	
	
	len = function(min, max)
		if max then
			if max < min then min, max = max, min end
		end
		return function(value)
			local t = type(value)
			if t ~= "string" and t ~= "table" then return false end
			if type(min) ~= "number" or type(max) ~= "number" or type(value) == "nil" then return false end
			local l
			if t == "string" then l = string.len(value) else l = #value end
			if type(l) ~= "number" then return false end
			return l >= min and l <= max
		end
	end,
	
	minlen = function(min)
		return function(value)
			local t = type(value)
			if t ~= "string" and t ~= "table" then return false end
			if type(min) ~= "number" or type(value) == "nil" then return false end
			local l
			if t == "string" then l = string.len(value) else l = #value end
			if type(l) ~= "number" then return false end
			return l >= min
		end
	end,
	
	maxlen = function(max)
		return function(value)
			local t = type(value)
			if t ~= "string" and t ~= "table" then return false end
			if type(max) ~= "number" then return false end
			local l
			if t == "string" then l = len(value) else l = #value end
			if type(l) ~= "number" then return false end
			return l <= max
		end
	end,
	
	positive = function(self)
		return function(value)
			if type(value) ~= "number" then return false end 
			return value > 0
		end
	end,
	
	negative = function(self)
		return function(value)
			if type(value) ~= "number" then return false end 
			return value < 0
		end
	end,
	
	reverse = function(self)
		return function(value)
			if type(value) ~= "string" then return nil, true end
			return string.reverse(string.gsub(value, "[%z-\x7F\xC2-\xF4][\x80-\xBF]*", function(c) 
				return #c > 1 and string.reverse(c) 
			end)), true
		end
	end,
	
	trim = function(self, pattern)
		pattern = pattern or "%s+"
		local l = "^" .. pattern
		local r = pattern .. "$"
		return function(value)
			local t = type(value)
			if t == "string" or t == "number" then
				return (string.gsub(value, r, ""):gsub(l, "")), true
			end
			return nil, true
		end
	end,
	
	ltrim = function(self, pattern)
		pattern = "^" .. (pattern or "%s+")
		return function(value)
			local t = type(value)
			if t == "string" or t == "number" then
				return (string.gsub(value, pattern, "")), true
			end
			return nil, true
		end
	end,
	
	rtrim = function(self, pattern)
		pattern = (pattern or "%s+") .. "$"
		return function(value)
			local t = type(value)
			if t == "string" or t == "number" then
				return (string.gsub(value, pattern, "")), true
			end
			return nil, true
		end
	end,

	abs = function(self)
		return function(value)
			if type(value) ~= "number" then return nil, true end 
			return math.abs(value), true
		end
	end,
	
	lower = function(self)
		return function(value)
			local t = type(value)
			if t == "string" or t == "number" then
				return string.lower(value), true
			end
			return nil, true
		end
	end,
	
	upper = function(self)
		return function(value)
			local t = type(value)
			if t == "string" or t == "number" then
				return string.upper(value), true
			end
			return nil, true
		end
	end,
}

local FilterMeta = {
	__index = function(self, key)
		local filter = filters[key]
		if filter then
			rawset(self, "filter", filter)
			return self
		end
	end,
	__call = function(self, ...)
		local filter = rawget(self, "filter")
		if filter then
			local filters = rawget(self, "filters")
			filters[#filters + 1] = filter(...)
			return self
		end
	end,
}

local Validator = {
	__index = function(self, key)
		local filter = filters[key]
		if filter then
			return setmetatable({ filters = { }, filter = filter }, FilterMeta)
		end
	end,
	__call = function(self, fields, bFailedStop)
		if type(fields) ~= "table" then error("first parame must be a table") end
		local R = rawget(self, "R")
		local rets = { }
		local field, name, value, validator, defValue, tp
		local bValidateOk, realValue = false
		local realValueOrValidateOk
		
		for _, field in ipairs(fields) do
			name = field[1]
			value = field[2]
			validator = field[3]
			defValue = field[4]
			realValue = nil
			
			if not name then break end
			
			if value ~= nil then
				tp = type(validator)
				if tp == "function" then
					realValue = validator(value)
				elseif tp == "table" then
					local filters = rawget(validator, "filters")
					if filters then
						for _, filter in ipairs(filters) do
							local ret, bRealValue = filter(value)
							if bRealValue then
								if not ret then break end
								realValue = ret
								value = ret
							elseif not ret then
								realValue = nil
								break
							else
								realValue = value
							end
						end
					end
				end
			end
			
			if not realValue then
				realValue = defValue
			end
			--ngx.say(name, realValue, type(realValue))
			rets[name] = realValue
			
			if realValue == nil then
				bValidateOk = false
				if bFailedStop then
					break
				end
			end
		end
		
		return rets, bValidateOk
	end,
}

return function(reeme)
	local validator = { R = reeme }
	
	return setmetatable(validator, Validator)
end