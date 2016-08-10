return function(reeme)
	local headers = { R = reeme }
	
	return setmetatable(headers, { 
		__index = ngx.req.get_headers(),
	})
end