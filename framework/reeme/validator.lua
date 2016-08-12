-- local filters = {
	-- number = function(self)
		-- return function(rawValue)
			-- return tonumber(rawValue), true
		-- end
	-- end,
	
	-- between = function(self, min, max)
		-- if not max then max = min end
		-- if max < min then min, max = max, min end
		-- return function(value)
			-- return value >= min and value <= max
		-- end
	-- end,
	
	-- equal = function(self, equal)
		-- return function(value)
			-- ngx.say("equl----------", equal)
			-- return value == equal
		-- end
	-- end,
-- }

-- local filterCall = {
	-- __call = function()
		-- local filter = self.filter
		
	-- end,
-- }

-- local Filter = {
	-- __index = function(self, key)
		-- local filter = filters[key]
		-- return setmetatable({ filter = filter }, filterCall)
		-- return self
	-- end,
	-- __call = function(self, value)
		-- local filters = rawget(self, "filters")
		
		-- filters[#filters + 1] = filter
		-- return self
	-- end,
-- }

-- local Validator = {
	-- __index = function(self, key)
		-- local filter = filters[key]
		-- if filter then
			-- return setmetatable({ filters = { }, filter = filter }, Filter)
		-- end
	-- end,
	-- __call = function(self, fields, bFailedStop)
		-- if type(fields) ~= "table" then return end
		-- local R = rawget(self, "R")
		-- local selfFilters = rawget(self, "filters")
		
		-- local rets = { }
		-- local field, name, value, validator, defValue, tp
		-- local bValidate, realValue = false
		-- local realValueOrValidateOk
		
		-- for i = 1, #fields do
			-- field = fields[i]
			-- name = field[1]
			-- value = field[2]
			-- validator = field[3]
			-- defValue = field[4]
			
			-- tp = type(validator)
			-- realValue = nil
			-- bValidate = false
			
			-- if tp == "function" then
				-- realValue = validator(value)
			-- elseif tp == "table" then
				-- bValidate = true
				-- local filters = rawget(validator, "filters")
				-- if filters then
					-- for ii = 1, #filters do
						-- local filter = filters[ii]
						-- local ret, bRealValue = filter(value)
						-- if bRealValue then
							-- realValue = ret
							-- value = ret
							-- if not realValue then
								-- break
							-- end
						-- elseif not ret then
							-- realValue = nil
							-- break
						-- end
					-- end
				-- end
			-- end
			
			-- if not realValue then
				-- realValue = defValue
				-- bValidate = realValue ~= nil
			-- end
			
			-- rets[name] = realValue
			
			-- if not bValidate and bFailedStop then
				-- return rets, false
			-- end
		-- end
		
		-- return rets, true
	-- end,
-- }

-- return function(reeme)
	-- local validator = { R = reeme }
	
	-- return setmetatable(validator, Validator)
-- end