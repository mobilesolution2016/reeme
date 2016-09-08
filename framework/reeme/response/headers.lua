return function(reeme)
	local headers = { R = reeme }
	
	return setmetatable(headers, { 
		__index = ngx.req.get_headers(),
		__newindex = function(self, key, value)
			ngx.header[key] = value
		end,
		__call = function(self, hds)
			for k, v in pairs(hds) do
				ngx.header[k] = v
			end
		end
	})
end