local writeMembers = {
	status = function(self, value)
		ngx.status = value
	end,
}

local ResponseBase = {
	__index = {
		headers = require("reeme.response.headers")(),
	}
}

local Response = {
	__index = {
		begin = function(self)
			local body = rawget(self, "body")
			if #body > 0 then
				return
			end
			
			rawset(self, "begined", true)
		end,
		
		write = function(self, ...)
			local body = rawget(self, "body")
			local begined = rawget(self, "begined")
			local values = { ... }
			for i = 1, #values do
				local value = values[i]
				local tp = type(value)
				if tp == "string" then
					if begined then
						body[#body + 1] = value
					else
						ngx.say(value)
					end
				end
			end
		end,
		
		clear = function(self)
			rawset(self, "body", { })
		end,
		
		finish = function(self, split)
			local body = rawget(self, "body")
			local str = table.concat(body, split or "")
			ngx.say(str)
			
			rawset(self, "begined", false)
			rawset(self, "body", { })
		end,
		
		useView = function(self, tpl)
			local view = require("reeme.response.view")(rawget(self, "R"))
			view:init(tpl)
			
			return view
		end,
	},
	__newindex = function(self, key, value)
		local f = writeMembers[key]
		if f then
			f(self, value)
		end
	end,
}
setmetatable(Response.__index, ResponseBase)

return function(reeme)
	local response = { R = reeme, body = { } }
	
	return setmetatable(response, Response)
end