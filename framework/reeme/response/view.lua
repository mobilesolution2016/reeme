local template = require "resty.template"

local function renderWithParams(view, params)
    return view(params)
end

local viewDir = nil 
local View = {
	__index = {
		init = function(self, tpl, params)
			if not tpl then error("template is nil") end
			local view = template.compile(viewDirs .. '/' .. tpl)
			rawset(self, "view", view)
			rawset(self, "params", params or { })
		end,
		render = function(self)
			local view = rawget(self, "view")
			local params = rawget(self, "params")
			local ok, ret = pcall(renderWithParams, view, params)
			if ok then
				ngx.say(ret)
			else
				error(ret)
			end
		end
	},
	__newindex = function(self, key, value)
		local params = rawget(self, "params")
		params[key] = value
	end,
}

return function(reeme)
	if not viewDir then
		local dirs = reeme:getConfigs().dirs
		viewDirs = string.format("%s/%s/%s", dirs.appRootDir, dirs.appBaseDir, dirs.viewsDir)
	end
	
	local view = { R = reeme, params = { } }
	
	return setmetatable(view, View)
end