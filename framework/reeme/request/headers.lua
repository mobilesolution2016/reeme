return function(reeme)
	local headers = { R = reeme }
	
	return setmetatable(headers, { 
		__index = ngx.req.get_headers(),
		__newindex = function(self, key, value)
			ngx.req.set_header(key, value)
		end,
	})
end