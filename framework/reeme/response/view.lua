local template = require "reeme.response.template"

local function renderWithParams(view, params)
    return view(params)
end

local viewDir = nil 
local View = {
	__index = {
		--[[init = function(self, tpl, params)
			if not tpl then error("template is nil") end
			local view = template.compile(viewDirs .. '/' .. tpl)
			rawset(self, "view", view)
			rawset(self, "params", params or { })
		end,
		render = function(self, tpl, params)
			local view = rawget(self, "view")
			local params = rawget(self, "params")
			local ok, ret = pcall(renderWithParams, view, params)
			if ok then
				ngx.say(ret)
			else
				error(ret)
			end
		end]]
		render = function(self, tpl, env)
			if not tpl then return end
			local s = template.compile_file(string.format("%s/%s", viewDir, tpl), env)
			if s then
				ngx.say(s)
			end
		end,
	},
	--[[__newindex = function(self, key, value)
		local params = rawget(self, "params")
		params[key] = value
	end,]]
}

return function(reeme)
	if not viewDir then
		local dirs = reeme:getConfigs().dirs
		viewDir = string.format("%s/%s/%s", dirs.appRootDir, dirs.appBaseDir, dirs.viewsDir)
	end
	
	local view = { R = reeme, params = { } }
	
	return setmetatable(view, View)
end