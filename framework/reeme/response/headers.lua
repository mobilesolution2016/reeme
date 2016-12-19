return function(reeme)
	local headers = { R = reeme }
	
	return setmetatable(headers, { 
		__index = ngx.req.get_headers(),
		__newindex = function(self, key, value)
			ngx.header[key] = value
		end,
		__call = function(self, key, val)
			if key then
				if type(key) == 'table' then
					for k, v in pairs(key) do
						ngx.header[k] = v
					end
				else
					ngx.header[key] = val
				end
			end
		end
	})
end