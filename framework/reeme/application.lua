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
		
		log = function(self, value, level)
			ngx.log(level or ngx.WARN, value)
		end,
		
		--print()
		--writes argument values into the nginx error.log file with the ngx.NOTICE log level
		
		getConfigs = function()
			return configs
		end,
		
		--internal redirect to uri with args
		exec = function(self, uri, args)
			ngx.exec(uri, args)
		end,
		
		--issue an HTTP 301 or 302 redirection to uri
		redirect = function(self, uri, status)
			ngx.redirect(uri, status)
		end,
		
		--issues a synchronous but still non-blocking Nginx Subrequest using uri.
		capture = function(self, uri, options)
			return ngx.location.capture(uri)
		end,
		
		--issues several parallel subrequests specified by the input table and returns their results in the same order and will not return until all the subrequests terminate.
		captureMulti = function(self, captures)
			return ngx.location.capture_multi(captures)
		end,
		
		--sleeps for the specified seconds without blocking
		sleep = function(self, seconds)
			ngx.sleep(seconds)
		end,
		
		--escape str as a URI component.
		escapeUri = function(str)
			return ngx.escape_uri(str)
		end,
		
		--unescape str as an escaped URI component
		unescapeUri = function(str)
			return ngx.unescape_uri(str)
		end,
		
		--encode the Lua table to a query args string according to the URI encoded rules.
		encodeArgs = function(argsTable)
			return ngx.encode_args(argsTable)
		end,
		
		----decodes a URI encoded query-string into a Lua table. This is the inverse function of ngx.encode_args.
		decodeArgs = function(str, maxArgs)
			return ngx.decode_args(str, maxArgs)
		end,
		
		--encodes str to a base64 digest
		encodeBase64 = function(str, noPadding)
			return ngx.encode_base64(str, noPadding)
		end,
		
		--decodes the str argument as a base64 digest to the raw form. Returns nil if str is not well formed
		decodeBase64 = function(str)
			return decode_base64(str)
		end,
		
		--calculates the CRC-32 (Cyclic Redundancy Code) digest for the str argument, This method performs better on relatively short str inputs (i.e., less than 30 ~ 60 bytes)
		crc32Short = function(str)
			return ngx.crc32_short(str)
		end,
		
		--calculates the CRC-32 (Cyclic Redundancy Code) digest for the str argument. This method performs better on relatively long str inputs (i.e., longer than 30 ~ 60 bytes)
		crc32Long = function(str)
			return ngx.crc32_short(str)
		end,
		
		--computes the HMAC-SHA1 digest of the argument str and turns the result using the secret key <secretKey>.
		hmacSha1 = function(secretKey, str)
			return ngx.hmac_sha1(secretKey, str)
		end,
		
		--returns the hexadecimal representation of the MD5 digest of the str argumen
		md5 = function(self, str)
			return ngx.md5(str)
		end,
		
		--returns the binary form of the MD5 digest of the str argument.
		md5bin = function(self, str)
			return ngx.md5_bin(str)
		end,
		
		--returns the binary form of the SHA-1 digest of the str argument.
		sha1bin = function(self, str)
			return ngx.sha1_bin(str)
		end,
		
		--returns a quoted SQL string literal according to the MySQL quoting rules.
		quoteSqlStr = function(self, rawValue)
			return ngx.quote_sql_str(rawValue)
		end,
		
		--returns current date (in the format yyyy-mm-dd) from the nginx cached time (no syscall involved unlike Lua's date library).
		today = function(self)
			return ngx.today()
		end,
		
		--returns the elapsed seconds from the epoch for the current time stamp from the nginx cached time (no syscall involved unlike Lua's date library). Updates of the Nginx time cache an be forced by calling ngx.update_time first.
		time = function()
			return ngx.time()
		end,
		
		--returns a floating-point number for the elapsed time in seconds (including milliseconds as the decimal part) from the epoch for the current time stamp from the nginx cached time (no syscall involved unlike Lua's date library).You can forcibly update the Nginx time cache by calling ngx.update_time first.
		now = function()
			return ngx.now()
		end,
		
		--forcibly updates the Nginx current time cache. This call involves a syscall and thus has some overhead, so do not abuse it.
		updateTime = function()
			ngx.update_time()
		end,
		
		--returns the current time stamp (in the format yyyy-mm-dd hh:mm:ss) of the nginx cached time (no syscall involved unlike Lua's os.date function). This is the local time.
		localtime = function()
			return ngx.localtime()
		end,
		
		--returns the current time stamp (in the format yyyy-mm-dd hh:mm:ss) of the nginx cached time (no syscall involved unlike Lua's os.date function).This is the UTC time.
		utctime = function()
			return ngx.utctime()
		end,
		
		--returns a formatted string can be used as the cookie expiration time. The parameter sec is the time stamp in seconds (like those returned from ngx.time).
		cookieTime = function(sec)
			return ngx.cookie_time(sec)
		end,
		
		--returns a formated string can be used as the http header time (for example, being used in Last-Modified header). The parameter sec is the time stamp in seconds (like those returned from ngx.time).
		httpTime = function(sec)
			return ngx.http_time(sec)
		end,
		
		--parse the http time string (as returned by ngx.http_time) into seconds. Returns the seconds or nil if the input string is in bad forms.
		parseHttpTime = function(str)
			return ngx.parse_http_time(str)
		end,
		
		--matches the subject string using the Perl compatible regular expression regex with the optional options.Only the first occurrence of the match is returned, or nil if no match is found. In case of errors, like seeing a bad regular expression or exceeding the PCRE stack limit, nil and a string describing the error will be returned.When a match is found, a Lua table captures is returned, where captures[0] holds the whole substring being matched, and captures[1] holds the first parenthesized sub-pattern's capturing, captures[2] the second, and so on.
		regexMatch = function(self, subject, regex, options, ctx, resTable)
			return ngx.re.match(subject, regex, options?, ctx, resTable)
		end,
		
		--similar to ngx.re.match but only returns the beginning index (from) and end index (to) of the matched substring. The returned indexes are 1-based and can be fed directly into the string.sub API function to obtain the matched substring.In case of errors (like bad regexes or any PCRE runtime errors), this API function returns two nil values followed by a string describing the error.If no match is found, this function just returns a nil value.
		regexFind = function(self, subject, regex, options, ctx, nth)
			return ngx.re.find(subject, regex, options, ctx, nth)
		end,
		
		--similar to ngx.re.match, but returns a Lua iterator instead, so as to let the user programmer iterate all the matches over the <subject> string argument with the PCRE regex.In case of errors, like seeing an ill-formed regular expression, nil and a string describing the error will be returned.
		regexGmatch = function(subject, regex, options)
			return ngx.re.gmatch(subject, regex, options)
		end,
		
		--substitutes the first match of the Perl compatible regular expression regex on the subject argument string with the string or function argument replace. The optional options argument has exactly the same meaning as in ngx.re.match.
		regexSub = function(subject, regex, replace, options)
			return ngx.re.sub(subject, regex, replace, options)
		end,
		
		--just like ngx.re.sub, but does global substitution.
		regexGsub = function(subject, regex, replace, options)
			return ngx.re.gsub(subject, regex, replace, options)
		end,
		
		--creates an Nginx timer with a user callback function as well as optional user arguments.
		timerAt = function(delay, callback, ...)
			return ngx.timer.at(delay, callback, ...)
		end,
		
		--returns the number of timers currently running.
		timerRunningCount = function()
			return ngx.timer.running_count()
		end,
		
		--returns the number of pending timers.
		timerPendingCount = function()
			return ngx.timer.pending_count()
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