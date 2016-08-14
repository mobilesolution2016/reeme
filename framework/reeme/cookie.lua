local cookie = require "resty.cookie"

local Cookie = {
	__index = function(self, key)
		local c = rawget(self, "cookie")
		if c then
			return c:get(key)
		end
	end,
	__call = function(self, cookies)
		local c = rawget(self, "cookie")
		if c then
			if cookies == nil then
				return c:get_all()
			elseif type(cookies) == "table" then
				c:set(cookies)
			end
		end
	end,
}

return function(reeme)
	local cookie = { R = reeme, cookie = cookie:new() }
	
	return setmetatable(cookie, Cookie)
end