local template = require "resty.template"

local function renderWithParams(view, params)
    return view(params)
end

local viewDir = nil 
local View = {
	__index = {
		init = function(self, tpl)
			local view = template.new(viewDirs .. '/' .. tpl)
			rawset(self, "view", view)
		end,
		render = function(self, tpl, params)
			if not tpl then
				local view = rawget(self, "view")
				if not view then error("tpl has not inited") end
				return view:render()
				--tostring(view)
			end
			
			local view = template.compile(viewDirs .. '/' .. tpl)
			local ok, ret = pcall(renderWithParams, view, params)
			if ok then
				ngx.say(ret)
				return true
			else
				error(ret)
			end
		end
	},
	__newindex = function(self, key, value)
		local view = rawget(self, "view")
		if not view then error("tpl has not inited") end
		view[key] = value
	end,
}

return function(reeme)
	if not viewDir then
		local dirs = reeme:getConfigs().dirs
		viewDirs = string.format("%s/%s/%s", dirs.appRootDir, dirs.appBaseDir, dirs.viewsDir)
	end
	
	local view = { R = reeme }
	
	return setmetatable(view, View)
end