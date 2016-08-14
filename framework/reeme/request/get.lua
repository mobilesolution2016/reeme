return function(reeme)
	local get = { R = reeme }
	
	return setmetatable(get, { 
		__index = ngx.req.get_uri_args(),
	})
end