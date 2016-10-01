local s = nil
local ffi = require('ffi')

local kLogDebug = 0
local kLogVerbose = 1
local kLogInfo = 2
local kLogWarning = 3
local kLogError = 4
local kLogFault = 5
local kLogCommandStart = 6

local kCmdSetName = kLogCommandStart
local kCmdClear = kCmdSetName + 1

ffi.cdef[[
	typedef struct
	{
		uint16_t	logType;
		uint16_t	msgLeng;
	} PckHeader;
]]

local pckhd = ffi.new('PckHeader')

local connect = function(reeme)
	s = ngx.socket.tcp()
	local ok, err = s:connect(reeme('dslogger') or '127.0.0.1', 9880)
	if not ok then
		return false
	end
	
	s:settimeout(1000)

	local appname = ngx.var.APP_NAME
	if appname and #appname > 0 then
		pckhd.logType = kCmdSetName
		pckhd.msgLeng = #appname
		s:send(ffi.string(pckhd, 4))
		s:send(appname)
	end
	
	return true
end

local sendmsg = function(t, ...)
	local c = select('#', ...)	
	local msg = c == 0 and '' or string.merge(...)

	pckhd.logType = t
	pckhd.msgLeng = #msg

	s:send(ffi.string(pckhd, 4))
	if #msg > 0 then
		s:send(msg)
	end
end

local globalLogger = {
	i = function(...)
		sendmsg(kLogInfo, ...)
	end,
	e = function(...)
		sendmsg(kLogError, ...)
	end,
	w = function(...)
		sendmsg(kLogWarning, ...)
	end,
	d = function(...)
		sendmsg(kLogDebug, ...)
	end,
	v = function(...)
		sendmsg(kLogVerbose, ...)
	end,
	f = function(...)
		sendmsg(kLogFault, ...)
	end,
	clear = function()
		sendmsg(kCmdClear)
	end,
}
local globalLoggerDummy = {
	i = function()
	end,
	e = function()
	end,
	w = function()
	end,
	d = function()
			end,
	v = function()
	end,
	f = function()
	end,
	clear = function()
	end,
}

_G['logger'] = globalLoggerDummy

setmetatable(globalLogger, { __call = function(self, ...)
	if s then sendmsg(kLogInfo, ...) end
end})

setmetatable(globalLoggerDummy, { __call = function()
end})

return function(reeme, t)
	local tp = type(reeme)
	if tp == 'string' then
		if s then sendmsg(t or kLogInfo, reeme) end
	
	elseif tp == 'table' then
		if not s and connect(reeme) then
			_G['logger'] = globalLogger
		end
		
	elseif not reeme then
		if s then
			s:close()
			s = nil
		end

		_G['logger'] = globalLoggerDummy
	end
	
	return globalLogger
end