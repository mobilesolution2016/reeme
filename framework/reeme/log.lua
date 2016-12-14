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

local lastAddr = nil
local cannotConnect = false
local pckhd = ffi.new('PckHeader')
local globalLogger, globalLoggerDummy = 0, 0

local initConnect = function(reeme)
	local addr = '127.0.0.1'
	if reeme and getmetatable(reeme) then
		addr = reeme('dslogger') or addr		
	end
	lastAddr = addr

	local s = ngx.socket.tcp()	
	s:settimeout(30)
	
	local ok, err = s:connect(addr, 9880)
	if not ok then
		grawset('logger', globalLoggerDummy)
		cannotConnect = true
		return false
	end		

	local appname = ngx.var.APP_NAME
	if appname and #appname > 0 then
		pckhd.logType = kCmdSetName
		pckhd.msgLeng = #appname
		
		s:send(ffi.string(pckhd, 4))
		s:send(appname)
		s:setkeepalive(7200000)
	end
	
	return true
end

local sendmsg = function(t, ...)
	local c = select('#', ...)	
	local msg = c == 0 and '' or string.merge(...)

	pckhd.logType = t
	pckhd.msgLeng = #msg

	local s = ngx.socket.tcp()	
	s:settimeout(200)
	
	local ok, err = s:connect(lastAddr, 9880)
	if err then
		grawset('logger', globalLoggerDummy)
		cannotConnect = true
		return
	end

	ok, err = s:send(ffi.string(pckhd, 4))
	if err then
		grawset('logger', globalLoggerDummy)
		cannotConnect = true
		return
	end
	
	if #msg > 0 then
		s:send(msg)
	end
	
	s:setkeepalive(7200000)
end

globalLogger = {
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
	close = function()
		grawset('logger', globalLoggerDummy)
	end
}
globalLoggerDummy = {
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
	close = function()
	end,
}

grawset('logger', globalLoggerDummy)

setmetatable(globalLogger, { __call = function(self, ...)
	sendmsg(kLogInfo, ...)
end})

setmetatable(globalLoggerDummy, { __call = function()
end})

return function(reeme, t)
	local tp = type(reeme)
	if tp == 'string' then
		if not cannotConnect then
			sendmsg(t or kLogInfo, reeme)
		end
	
	elseif tp == 'table' then
		if initConnect then
			if initConnect(reeme) then
				grawset('logger', globalLogger)
			end
			initConnect = nil
		end
		
	elseif not reeme then
		grawset('logger', globalLoggerDummy)
	end
	
	return globalLogger
end