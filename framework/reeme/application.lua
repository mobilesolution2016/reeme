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

--print is use writes argument values into the nginx error.log file with the ngx.NOTICE log level
		
local baseMeta = {
	__index = {
		statusCode = {
			HTTP_CONTINUE = ngx.HTTP_CONTINUE, --100
			HTTP_SWITCHING_PROTOCOLS = ngx.HTTP_SWITCHING_PROTOCOLS, --101
			HTTP_OK = ngx.HTTP_OK, --200
			HTTP_CREATED = ngx.HTTP_CREATED, --201
			HTTP_ACCEPTED = ngx.HTTP_ACCEPTED, --202
			HTTP_NO_CONTENT = ngx.HTTP_NO_CONTENT, --204
			HTTP_PARTIAL_CONTENT = ngx.HTTP_PARTIAL_CONTENT, --206
			HTTP_SPECIAL_RESPONSE = ngx.HTTP_SPECIAL_RESPONSE, --300
			HTTP_MOVED_PERMANENTLY = ngx.HTTP_MOVED_PERMANENTLY, --301
			HTTP_MOVED_TEMPORARILY = ngx.HTTP_MOVED_TEMPORARILY, --302
			HTTP_SEE_OTHER = ngx.HTTP_SEE_OTHER, --303
			HTTP_NOT_MODIFIED = ngx.HTTP_NOT_MODIFIED, --304
			HTTP_TEMPORARY_REDIRECT = ngx.HTTP_TEMPORARY_REDIRECT, --307
			HTTP_BAD_REQUEST = ngx.HTTP_BAD_REQUEST, --400
			HTTP_UNAUTHORIZED = ngx.HTTP_UNAUTHORIZED, --401
			HTTP_PAYMENT_REQUIRED = ngx.HTTP_PAYMENT_REQUIRED, --402
			HTTP_FORBIDDEN = ngx.HTTP_FORBIDDEN, --403
			HTTP_NOT_FOUND = ngx.HTTP_NOT_FOUND, --404
			HTTP_NOT_ALLOWED = ngx.HTTP_NOT_ALLOWED, --405
			HTTP_NOT_ACCEPTABLE = ngx.HTTP_NOT_ACCEPTABLE, --406
			HTTP_REQUEST_TIMEOUT = ngx.HTTP_REQUEST_TIMEOUT, --408
			HTTP_CONFLICT = ngx.HTTP_CONFLICT, --409
			HTTP_GONE = ngx.HTTP_GONE, --410
			HTTP_UPGRADE_REQUIRED = ngx.HTTP_UPGRADE_REQUIRED, --426
			HTTP_TOO_MANY_REQUESTS = ngx.HTTP_TOO_MANY_REQUESTS, --429
			HTTP_CLOSE = ngx.HTTP_CLOSE, --444
			HTTP_ILLEGAL = ngx.HTTP_ILLEGAL, --451
			HTTP_INTERNAL_SERVER_ERROR = ngx.HTTP_INTERNAL_SERVER_ERROR, --500
			HTTP_METHOD_NOT_IMPLEMENTED = ngx.HTTP_METHOD_NOT_IMPLEMENTED, --501
			HTTP_BAD_GATEWAY = ngx.HTTP_BAD_GATEWAY, --502
			HTTP_GATEWAY_TIMEOUT = ngx.HTTP_GATEWAY_TIMEOUT, --504
			HTTP_VERSION_NOT_SUPPORTED = ngx.HTTP_SERVICE_UNAVAILABLE, --505
			HTTP_INSUFFICIENT_STORAGE = ngx.HTTP_INSUFFICIENT_STORAGE, --507
		},
		
		method = {
			HTTP_GET = ngx.HTTP_GET,
			HTTP_HEAD = ngx.HTTP_HEAD,
			HTTP_PUT = ngx.HTTP_PUT,
			HTTP_POST = ngx.HTTP_POST,
			HTTP_DELETE = ngx.HTTP_DELETE,
			HTTP_OPTIONS = ngx.HTTP_OPTIONS,
			HTTP_MKCOL = ngx.HTTP_MKCOL,
			HTTP_COPY = ngx.HTTP_COPY,
			HTTP_MOVE = ngx.HTTP_MOVE,
			HTTP_PROPFIND = ngx.HTTP_PROPFIND,
			HTTP_PROPPATCH = ngx.HTTP_PROPPATCH,
			HTTP_LOCK = ngx.HTTP_LOCK,
			HTTP_UNLOCK = ngx.HTTP_UNLOCK,
			HTTP_PATCH = ngx.HTTP_PATCH,
			HTTP_TRACE = ngx.HTTP_TRACE,
		},
		
		logLevel = {
			STDERR = ngx.STDERR,
			EMERG = ngx.EMERG,
			ALERT = ngx.ALERT,
			CRIT = ngx.CRIT,
			ERR = ngx.ERR,
			WARN = ngx.WARN,
			NOTICE = ngx.NOTICE,
			INFO = ngx.INFO,
			DEBUG = ngx.DEBUG,
		},
		
		log = function(level, ...)
			ngx.log(level or ngx.WARN, ...)
		end,

		getConfigs = function()
			return configs
		end,
		
		exec = function(uri, args)
			ngx.exec(uri, args)
		end,
		
		redirect = function(uri, status)
			ngx.redirect(uri, status)
		end,
		
		capture = function(uri, options)
			return ngx.location.capture(uri)
		end,
		
		captureMulti = function(captures)
			return ngx.location.capture_multi(captures)
		end,
		
		sleep = function(seconds)
			ngx.sleep(seconds)
		end,
		
		urlEncode = function(str)
			return ngx.escape_uri(str)
		end,
		
		urlDecode = function(str)
			return ngx.unescape_uri(str)
		end,
		
		argsEncode = function(argsTable)
			return ngx.encode_args(argsTable)
		end,
		
		argsDecode = function(str, maxArgs)
			return ngx.decode_args(str, maxArgs)
		end,
		
		base64Encode = function(str, noPadding)
			return ngx.encode_base64(str, noPadding)
		end,
		
		base64Decode = function(str)
			return decode_base64(str)
		end,
		
		jsonEncode = require("cjson.safe").encode,

		jsonDecode = require("cjson.safe").decode,
		
		crc32 = function(str)
			if not str or type(str) ~= "string" then return nil end
			
			if #str <= 60 then
				return ngx.crc32_short(str)
			else
				return ngx.crc32_long(str)
			end
		end,
		
		hmacSha1 = function(secretKey, str, bReturnBin)
			if not secretKey or type(secretKey) ~= "string" or not str or type(str) ~= "string" then return nil end
			
			if not bReturnBin then
				return ngx.hmac_sha1(secretKey, str)
			else
				return ngx.sha1_bin(str)
			end
		end,
		
		md5 = function(str, bReturnBin)
			if not str or type(str) ~= "string" then return nil end
			
			if not bReturnBin then
				return ngx.md5(str)
			else
				return ngx.md5_bin(str)
			end
		end,
		
		quoteSql = function(rawValue)
			return ngx.quote_sql_str(rawValue)
		end,
		
		regex = {
			match = function(self, subject, regex, options, ctx, resTable)
				return ngx.re.match(subject, regex, options, ctx, resTable)
			end,
			find = function(self, subject, regex, options, ctx, nth)
				return ngx.re.find(subject, regex, options, ctx, nth)
			end,
			gmatch = function(subject, regex, options)
				return ngx.re.gmatch(subject, regex, options)
			end,
			sub = function(subject, regex, replace, options)
				return ngx.re.sub(subject, regex, replace, options)
			end,
			gsub = function(subject, regex, replace, options)
				return ngx.re.gsub(subject, regex, replace, options)
			end,
		},
		
		time = {
			today = function()
				return ngx.today()
			end,
			
			time = function()
				return ngx.time()
			end,
			
			now = function()
				return ngx.now()
			end,
			
			update = function()
				ngx.update_time()
			end,
			
			localtime = function()
				return ngx.localtime()
			end,
			
			utctime = function()
				return ngx.utctime()
			end,
			
			cookieTime = function(sec)
				return ngx.cookie_time(sec)
			end,
			
			httpTime = function(sec)
				return ngx.http_time(sec)
			end,
			
			parseHttpTime = function(str)
				return ngx.parse_http_time(str)
			end,
		},
		
		timer = {
			set = function(delaySeconds, callback, ...)
				return ngx.timer.at(delaySeconds, callback, ...)
			end,
			
			getRunningCount = function()
				return ngx.timer.running_count()
			end,
			
			getPendingCount = function()
				return ngx.timer.pending_count()
			end,
		},
		
		file = {
			move = function(source, dest)
				return os.rename(source, dest)
			end,
			
			exists = function(filename)
				local f, err = io.open(filename)
				if f then
					f:close()
					return true
				end
				return false
			end,
			
			copy = function(source, destination)
				sourcefile = io.open(source, "rb")
				if not sourcefile then return false end
				destinationfile = io.open(destination, "wb+")
				if not destinationfile then
					sourcefile:close()
					return false
				end

				destinationfile:write(sourcefile:read("*a"))

				sourcefile:close()
				destinationfile:close()
			end,

			remove = function(filename)
				return os.remove(filename)
			end,
			
			readAll = function(filename)
				local file, err = io_open(filename, "rb")
				local data = file and file:read("*a") or nil
				if file then
					file:close()
				end
				return data
			end
		},
		
		crypt = {
			rsaEncrypt = function(publicKey, str)
				local rsa = require("resty.rsa")
				local base64 = require("resty.base64")
				
				local pub, err = rsa:new(publicKey, true)
				if not pub then
					return nil, err
				end
				
				local encrypted, err = pub:encrypt(str)
				if not encrypted then
					return nil, err
				end
				
				return base64.base64_encode(encrypted)
			end,
			
			rsaDecrypt = function(privateKey, str)
				local rsa = require("resty.rsa")
				local base64 = require("resty.base64")
				
				local priv, err = rsa:new(privateKey)
				if not priv then
					return nil, err
				end
				
				local decrypted = priv:decrypt(base64.base64_decode(str))
				if not decrypted then
					return nil, err
				end
				return decrypted
			end,
		}
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
				if rawget(c.response, "view") == r then
					r:render()
				else
					ngx.say(c.jsonEncode(r))
				end
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