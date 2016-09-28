--一些扩展函数
local ffi = require('ffi')
local reemext = ffi.load('reemext')
local int64Buf = ffi.new('char[?]', 32)
local cExtLib = findmetatable('REEME_C_EXTLIB')

local ffiload = ffi.load
local newffiload = function(name)
	local paths = string.split(package.cpath, ';')
	for i = 1, #paths do
		local h = string.replace(paths[i], '?', name)
		if ffi.C.access(h, 0) == 0 then
			h = ffiload(h)
			if h then
				return h
			end
		end
	end
end

if ffi.abi('win') then
	ffi.cdef [[
		int __cdecl strcmp(const char*, const char*);
		int __cdecl strcasecmp(const char*, const char*) __asm__("_stricmp");
		int __cdecl strncmp(const char*, const char*, size_t);
		int __cdecl strncasecmp(const char*, const char*, size_t) __asm__("_strnicmp");
		
		int access(const char *, int) __asm__("_access");
	]]
	
	ffi.load = function(name)
		if string.byte(name, 1) == 47 or (string.byte(name, 2) == 58 and string.byte(name, 3) == 47) then
			return ffiload(name)
		end
		return newffiload(name)
	end
else
	ffi.cdef [[
		int strcmp(const char*, const char*);
		int strcasecmp(const char*, const char*);
		int strncmp(const char*, const char*, size_t);
		int strncasecmp(const char*, const char*, size_t);
		
		int access(const char *, int);
	]]
	
	ffi.load = function(name)
		return string.byte(name, 1) == 47 and ffiload(name) or newffiload(name)
	end
end

ffi.cdef[[
	int64_t str2int64(const char* str);
	uint64_t str2uint64(const char* str);
	uint32_t cdataisint64(const char* str, size_t len);
	int64_t double2int64(double dbl);
	uint64_t double2uint64(double dbl);
	int64_t ltud2int64(void* p);
	uint64_t ltud2uint64(void* p);
	size_t opt_i64toa(int64_t value, char* buffer);
	size_t opt_u64toa(uint64_t value, char* buffer);
	size_t opt_u64toa_hex(uint64_t value, char* dst, bool useUpperCase);
]]

_G.table.unique = function(tbl)
	local cc = #tbl
	local s, r = table.new(0, cc), table.new(cc, 0)
	for i = 1, cc do
		s[tbl[i]] = true
	end
	local i = 1
	for k,_ in pairs(s) do
		r[i] = k
		i = i + 1
	end
	return r
end

_G.io.exists = function(name)
	return ffi.C.access(name, 0) == 0
end

local strlib = _G.string
strlib.cut = function(str, p)
	if str then
		local pos = string.find(str, p, 1, true)
		if pos then
			return string.sub(str, 1, pos - 1), string.sub(str, pos + 1)
		end
	end
	return str
end
strlib.casecmp = function(a, b)
	return ffi.C.strcasecmp(a, b) == 0
end
strlib.ncasecmp = function(a, b, n)
	return ffi.C.strncasecmp(a, b, n) == 0
end
strlib.cmp = function(a, b, c, d)
	local func
	if type(c) == 'number' then
		func = d == true and ffi.C.strncasecmp or ffi.C.strncmp
		if func(a, b, c) == 0 then
			return true
		end
	else
		func = c == true and ffi.C.strcasecmp or ffi.C.strcmp
		if func(a, b) == 0 then
			return true
		end
	end
	
	return false
end

local int64construct = function(a, b)
	local t = type(a)
	if t == 'string' then a = reemext.str2int64(a)
	elseif t == 'number' then a = reemext.double2int64(a)
	elseif t == 'lightuserdata' then a = reemext.ltud2int64(a)
	elseif t ~= 'cdata' then return error('error construct value by int64') end
	if b then
		t = type(b)
		if t == 'string' then b = reemext.str2int64(b)
		elseif t == 'number' then a = reemext.double2int64(b)
		elseif t == 'lightuserdata' then b = reemext.ltud2int64(b)
		elseif t ~= 'cdata' then return error('error construct value by int64') end
		return bit.lshift(a, 32) + b
	end
	return a
end

local int64 = {
	fromstr = reemext.str2int64,
	is = function(str)
		if type(str) == 'cdata' then
			local s = tostring(str)
			return reemext.cdataisint64(s, #s) >= 2, s
		end
		return false
	end,
	tostr = function(v)
		local l = reemext.opt_i64toa(v, int64Buf)
		return ffi.string(int64Buf, l)
	end,
	value = reemext.ltud2int64,
	key = cExtLib.int64ltud,
}

local uint64 = {
	fromstr = reemext.str2uint64,
	is = function(str)
		if type(str) == 'cdata' then
			local s = tostring(str)
			return reemext.cdataisint64(s, #s) == 3, s
		end
		return false
	end,
	make = function(hi, lo)
		return bit.lshift(hi, 32) + lo
	end,
	tostr = function(v)
		local l = reemext.opt_u64toa(v, int64Buf)
		return ffi.string(int64Buf, l)
	end,
	tohex = function(v, upcase)
		local l = reemext.opt_u64toa_hex(v, int64Buf, upcase or true)
		return ffi.string(int64Buf, l)
	end,
	value = reemext.ltud2uint64,
	key = cExtLib.int64ltud,
}

_G.int64, _G.uint64, _G.ffi = setmetatable(int64, { __call = function(self, a, b) return int64construct(a, b) end }),
							  setmetatable(uint64, { __call = function(self, a, b) return int64construct(a, b) end }),
							  ffi

-----------------------------------------------------------------------------------------------------------------------

local configs = nil
local defConfigs = {
	dirs = {
		appBaseDir = 'app',
		modelsDir = 'models',
		viewsDir = 'views',
		controllersDir = 'controllers'
	},
	devMode = true,
	viewFileExt = '.html',
}

local error_old = error
function error(e)
	error_old({ msg = e })
end

local getAppConfigs = function()
	return configs
end
	
--print is use writes argument values into the nginx error.log file with the ngx.NOTICE log level
local copybasees = {
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
}

local loadables = { cookie = 1, mysql = 1, request = 1, response = 1, router = 1, utils = 1, validator = 1, deamon = 1, ffi_reflect = require('reeme.ffi_reflect') }
local application = {
	__index = function(self, key)
		local reeme = self.__reeme
		local f = self.__bases[key] or self.__users[key]
		if f then
			rawset(reeme, key, f)
			return f
		end

		f = loadables[key]
		if f == 1 then
			f = require('reeme.' .. key)
			
			local ft = type(f)
			if ft == 'function' then
				local r = f(reeme)

				rawset(reeme, key, r)
				loadables[key] = f
				
				return r
			end
			if ft == 'table' then
				rawset(reeme, key, f)
				return f
			end

		elseif f then
			local r = type(f) == 'function' and f(reeme) or f
			rawset(reeme, key, r)
			return r
		end		
	end,
}

local function lazyLoaderProc(self, key, ...)
	local lazyLoader = configs[key]
	local tp = type(lazyLoader)
	if tp == "table" then
		local fget = lazyLoader.get
		local ffree = lazyLoader.free
		if type(fget) == "function" then
			local r = fget(self, ...)
			if r and type(ffree) == "function" then
				local loader = {
					params = { ... },
					r = r
				}
				rawget(self, "_lazyLoaders")[ffree] = loader
			end
			return r
		end

	elseif tp == 'function' then
		return lazyLoader(self, ...)
	end
end

-----------------------------------------------------------------------------------------------------------------------
local responseView = require('reeme.response.view')
local viewMeta = responseView('getmeta')

local outputRedirect = function(app, v)
	ngx.say(v)
end

local defTblfmt = function(app, tbl)
	return string.json(tbl, string.JSON_UNICODES)
end

local appMeta = {
	__index = {
		init = function(self, cfgs)
			if not configs then
				configs = defConfigs
				table.extend(configs, cfgs)
			end
			return self
		end,
		
		--响应某个方法
		on = function(self, name, func)
			local valids = { pre = 1, err = 1, denied = 1, missmatch = 1, tblfmt = 1, output = 1, ['end'] = 1 }
			if type(func) == 'function' and valids[name] then
				self[name .. 'Proc'] = func
			else
				error('Error for application.on function call')
			end
			return self
		end,
		
		--添加用户级自定义变量
		addUsers = function(self, vals)
			local u = self.users
			for k,v in pairs(vals) do
				u[k] = v
			end
			return self
		end,
		
		--设置所有控制器公共的父级
		setControllerBase = function(self, tbl)
			self.controllerBase = tbl
			return self
		end,
		
		--启用全局模板缓存
		setTemplateCache = function(self, cacheObj)
			if cacheObj.get and (cacheObj.writeFunction or cacheObj.writeCode) then				
				responseView('templatecache', cacheObj)
			else
				error('Error for application.setTemplateCache call, cache object invalid')
			end
			return self
		end,
		
		--加载控制器，返回控制器实例和要执行的动作函数
		loadController = function(self, path, act)
			local dirs = configs.dirs
			local controlNew = nil

			while true do
				--加载
				local _, errmsg = pcall(function()
					controlNew = require(string.format('%s.%s.%s', dirs.appBaseDir, dirs.controllersDir, path:gsub('/', '.')))
				end)

				if controlNew then
					break
				end
				
				--失败，则判断是否有errProc，如果有就执行
				if self.errProc then
					--这个函数可以返回新的path和act用于去加载。如果没有返回，那么就停止加载了
					path, act = self.errProc(self, path, act, errmsg)
					if not path or not act then
						return nil, nil, true
					end
				else
					ngx.say(string.format("load controller '%s' failed:<br/>\n%s", path, errmsg))
					return nil, nil, true
				end
			end
			
			if type(controlNew) ~= 'function' then
				error(string.format('controller %s must return a function that has action functions', path))
			end
			
			local c = controlNew(act)
			local mth = c[act .. 'Action']
			local cm = getmetatable(c)

			local metacopy = { __index = { 
				__reeme = c, 
				__bases = self.controllerBase or {}, 
				__users = self.users or {},
				
				statusCode = copybasees.statusCode,
				method = copybasees.method,
				logLevel = copybasees.logLevel,
				getConfigs = getAppConfigs,
				
				currentControllerName = path,
				currentRequestAction = act
			} }
			setmetatable(metacopy.__index, application)

			if cm then
				if type(cm.__index) == 'function' then
					error(string.format("the __index of controller[%s]'s metatable must be a table", path))
				end
				
				cm.__call = lazyLoaderProc
				setmetatable(cm.__index, metacopy)
			else
				metacopy.__call = lazyLoaderProc
				setmetatable(c, metacopy)
			end

			rawset(c, "_lazyLoaders", { })
			
			return c, mth
		end,
		
		run = function(self)
			local router = configs.router or require("reeme.router")
			local path, act = router(ngx.var.uri)
			local c, mth, r

			--载入控制器
			c, mth, r = self:loadController(path, act)
			if r == true then
				--halt it
				return
			end
			
			local ok, err = xpcall(function()			
				if self.preProc then
					--执行动作前响应函数
					r = self.preProc(self, c, path, act, mth)
					local tp = type(r)

					if tp == 'table' then
						if r.response then
							mth = nil
							r = r.response
						elseif r.controller then
							c, mth = r.controller, r.method
						elseif r.method then
							mth = r.method
						end
					elseif tp == 'string' then
						mth = nil
						ngx.say(r)
					end
					
				elseif not mth then
					--如果没有动作前响应函数又没有动作，那么就报错
					error(string.format("the action %s of controller %s undefined", act, path))
				end

				--执行动作
				if c and mth then
					r = mth(c)
					if r == nil then
						r = c.actionReturned
					end
				elseif self.missmatchProc then
					self.missmatchProc(self, path, act)
					r = nil
				end

				--处理返回的结果
				if r ~= nil then
					local tp = type(r)
					local out = self.outputProc or outputRedirect

					if tp == 'table' then
						--可能是一个view也可能是json
						local o
						if getmetatable(r) == viewMeta then
							o = r:content()
						else
							o = (self.tblfmtProc or defTblfmt)(self, r)
						end
						
						out(self, o)
						
					elseif tp == 'string' then
						--字符串直接输出
						out(self, r)

					elseif r == false then
						--动作函数返回false表示拒绝访问，此时如果定义有deniedProc的话就会被执行，否则统一返回ErrorAccess的报错
						if self.deniedProc then
							self.deniedProc(self, c, path, act)
						else
							out(self, 'Denied Error Access!')
						end
					end
				end

				--结束动作
				if self.endProc then
					self.endProc(self, c, path, act)
				end
				
			end, debug.traceback)		
			
			--清理
			if c then
				local lazyLoaders = rawget(c, "_lazyLoaders")
				for ffree, loader in pairs(lazyLoaders) do
					ffree(c, loader.r, loader.params)
				end
			end
			c = nil

			if not ok then
				local msg = type(err) == 'table' and err.msg or err				
				local out = self.outputProc or outputRedirect
				local msgtp = type(msg)
				
				if msgtp == "string" then
					out(self, msg)
				elseif msgtp == "table" then
					local tblfmt = self.tblfmtProc or defTblfmt
					out(self, tblfmt(self, msg))
				elseif type(err) == "string" then
					out(self, err)
				end

				ngx.eof()
			end
		end,
	}
}

return function()
	return setmetatable({ users = { } }, appMeta)
end