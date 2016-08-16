local appRootDir = ngx.var.APP_ROOT
if not appRootDir then
	ngx.say("APP_ROOT is not definde in app nginx conf")
	ngx.eof()
end

local currentAppPath = appRootDir .. '/?.lua'
if not string.find(package.path, currentAppPath) then
	if package.path and #package.path > 0 then
		package.path = package.path .. ';' .. currentAppPath
	else
		package.path = appRootDir .. currentAppPath
	end
end

local oldRequire = require

local ffi = require("ffi")
function require(name)
	local appName = appRootDir .. "/" .. name
	
    if package.loaded[appName] then 
		return package.loaded[appName]
	end

	local searchPrefix = ffi.abi('win') and "?.lua;?/init.lua" or "/?.lua;/?/init.lua"
	local moduleName = nil
	if package.searchpath(appName, searchPrefix) ~= nil then
        moduleName = appName
    else
        moduleName = name
    end
	
    return oldRequire(moduleName)
end

local ok, err = pcall(function()
	require("public.index")()
end)
if not ok then
	ngx.say(err)
end