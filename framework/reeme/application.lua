local configs = nil

local function doRequire(path)
	return pcall(function(path)
		return require(path)
	end, path)
end

local error_old = error
function error(e)
	error_old({ msg = e })
end

local baseMeta = {
	__index = {
		log = function(self, value)
			ngx.log(ngx.WARN, value)
		end,
		getConfigs = function()
			return configs
		end,
		jsonEncode = require("cjson.safe").encode,
		jsonDecode = require("cjson.safe").decode,
	},
	__call = function(self, key)
		local f = configs[key]
		if type(f) == 'function' then
			return f(self)
		end
	end
}

local application = {
	__index = function(self, key)
		local dirs = configs.dirs		
		local r, f = doRequire(string.format('reeme.%s', string.gsub(key, '_', '.')))
		
		if type(f) == 'function' then 
			local r = f(self)
			rawset(self, key, r)
			return r
		end
		
		return nil
	end,
	
	__newindex = function()
	end,
}

setmetatable(baseMeta.__index, application)

return function(cfgs)
	local ok, err = pcall(function()
		--require('mobdebug').start('192.168.3.13')
		if not configs then
			--first in this LuaState
			local appRootDir = ngx.var.APP_ROOT
			if not appRootDir then
				error("APP_ROOT is not definde in app nginx conf")
			end
			
			if package.path and #package.path > 0 then
				package.path = package.path .. ';' .. appRootDir .. '/?.lua'
			else
				package.path = appRootDir .. '/?.lua'
			end

			if not cfgs then cfgs = { } end
			if not cfgs.dirs then cfgs.dirs = { } end
			cfgs.dirs.appRootDir = appRootDir
			if cfgs.devMode == nil then cfgs.devMode = true end
			if not cfgs.dirs.appBaseDir then cfgs.dirs.appBaseDir = 'app' end
			if not cfgs.dirs.modelsDir then cfgs.dirs.modelsDir = 'models' end
			if not cfgs.dirs.viewsDir then cfgs.dirs.viewsDir = "views" end
			if not cfgs.dirs.controllersDir then cfgs.dirs.controllersDir = 'controllers' end
			
			configs = cfgs
		end
		
		local router = cfgs.router or require("reeme.router")
		local uri = ngx.var.uri
		local path, act = router(uri)
		local dirs = configs.dirs
		local ok, controlNew = doRequire(string.format('%s.%s.%s', dirs.appBaseDir, dirs.controllersDir, string.gsub(path, '_', '.')))

		if not ok then
			if configs.devMode then
				error(controlNew)
			else
				error(string.format("controller %s not find", path))
			end
		end
		
		if type(controlNew) ~= 'function' then
			error(string.format('controller %s must return a function that has action functions', path))
		end
		
		local c = controlNew(act)
		local cm = getmetatable(c)
		if cm then
			if type(cm.__index) == 'function' then
				error(string.format("the __index of controller[%s]'s metatable must be a table", path))
			end
			
			cm.__call = baseMeta.__call
			setmetatable(cm.__index, baseMeta)
		else
			setmetatable(c, baseMeta)
		end
		
		local r = c[act](c)
		if r then
			local tp = type(r)
			
			if tp == 'table' then
				ngx.say(c.jsonEncode(r))
			elseif tp == 'string' then
				ngx.say(r)
			end
		end
		--require('mobdebug').done()
	end)
	
	if not ok then
		ngx.say(err.msg or err)
		ngx.eof()
	end
end