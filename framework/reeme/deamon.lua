local reemeCLib = findmetatable('REEME_C_EXTLIB')
local requestDeamon = reemeCLib.request_deamon

local deamonMeta = {
	__index = {
		start = function(path, args)
			return reemeCLib.start_deamon(path, args)
		end,
		connect = function(host, port)
			return reemeCLib.connect_deamon(host, port)
		end,
		
		request = function(posts)
			return requestDeamon(posts)
		end
	}
}

return function()
	return setmetatable({}, deamonMeta)
end