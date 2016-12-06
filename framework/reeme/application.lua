--crt functions & ffi.load modify
local ffi, reemext = require('ffi'), nil
local ffiload = ffi.load

local newffiload = function(name)
	for path in string.gmatch(package.cpath, '([^;]+)') do
		local h = string.gsub(path, '?', name)
		if ffi.C.access(h, 0) == 0 then
			h = ffiload(h)
			if h then
				return h
			end
		end
	end
end

if ffi.os == 'Windows' then
	ffi.cdef [[
		int strcmp(const char*, const char*);
		int strcasecmp(const char*, const char*) __asm__("_stricmp");
		int strncmp(const char*, const char*, size_t);
		int strncasecmp(const char*, const char*, size_t) __asm__("_strnicmp");

		int access(const char *, int) __asm__("_access");
	]]

	ffi.load = function(name)
		local h
		local ok = pcall(function()
			h = ffiload(name)
		end)
		if not h and not (string.byte(name, 1) == 47 or (string.byte(name, 2) == 58 and string.byte(name, 3) == 47)) then
			h = newffiload(name)
		end
		return h
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
		local h
		local ok = pcall(function()
			h = ffiload(name)
		end)
		if not h and string.byte(name, 1) ~= 47 then
			return newffiload('lib' .. name)
		end
		return h
	end
end

--reemext c module
local reemext = ffi.load('reemext') or error('Load reemext c module failed')

require(ffi.os == 'Windows' and 'reemext' or 'libreemext')
local cExtLib = findmetatable('REEME_C_EXTLIB')

ffi.cdef[[
	int strcspn(const char*, const char*);
	int strspn(const char*, const char*);

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

	int deleteDirectory(const char* path);
	int deleteFile(const char* fname);
]]

_G.libreemext = reemext

--lua standard library extends
--将tbl中的元素构造成一个全新的唯一值的数组返回
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
--将A数组中的元素如果在B数组存在有的就移除，剩下的不存在于B数组中的构建出一个新的数组返回（A必须是顺序数组，B可以是顺序数组也可以不是顺序的）
_G.table.exclude = function(A, B)
	if type(A) == 'table' and type(B) == 'table' then
		local r, uniques = table.new(#A / 2, 0), B
		
		if #B > 0 then
			uniques = table.new(0, #B)
			for i = 1, #B do
				uniques[B[i]] = true
			end
		end
		
		for i = 1, #A do
			if not B[A[i]] then
				r[#r + 1] = A[i]
			end
		end
		
		return r
	end
end
_G.table.append = function(A, B)
	if type(A) == 'table' and type(B) == 'table' then
		if #B > 0 then
			for i = 1, #B do
				A[#A + 1] = B[i]
			end
		else
			for k,v in pairs(B) do
				A[k] = v
			end
		end
	end
	
	return A
end

_G.io.exists = function(name)
	return ffi.C.access(name, 0) == 0
end

_G.io.filesize = function(name)
	return cExtLib.filesize(name)
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
		return func(a, b, c) == 0
	else
		func = c == true and ffi.C.strcasecmp or ffi.C.strcmp
		return func(a, b) == 0
	end

	return false
end
strlib.spn = ffi.C.strspn
strlib.cspn = ffi.C.strcspn

local int64Buf = ffi.new('char[?]', 32)
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
--初始化全局变量logger，但不会真正产生连接，除非在外部第一次操作log
require('reeme.log')

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
	error_old(e, 0)
end

local function doLazyLoader(self, key, ...)
	local lazyLoader = self.thisApp.configs[key]
	local tp = type(lazyLoader)
	if tp == "table" then
		local fget = lazyLoader.get
		if type(fget) == "function" then
			local r = fget(self, ...)
			local ffree = lazyLoader.free

			if r and type(ffree) == "function" then
				local loader = {
					params = { ... },
					r = r
				}
				self.thisApp.lazyModules[ffree] = loader
			end
			return r
		end

	elseif tp == 'function' then
		return lazyLoader(self, ...)
	end

	rawset(self, key, lazyLoader)
	return lazyLoader
end

local loadables = { cookie = 1, mysql = 1, request = 1, response = 1, router = 1, utils = 1, validator = 1, deamon = 1, log = 1, fd = 1, ffi_reflect = require('reeme.ffi_reflect') }
local ctlMeta = {
	__index = function(self, key)
		local f = rawget(rawget(self, -10000), key) or
				  rawget(self, -10001)[key] or
				  rawget(self, -10002)[key]
		if f then
			rawset(self, key, f)
			return f
		end

		f = loadables[key]
		if f == 1 then
			f = require('reeme.' .. key)

			local ft = type(f)
			if ft == 'function' then
				local r = f(self)

				rawset(self, key, r)
				loadables[key] = f

				return r
			end
			if ft == 'table' then
				rawset(self, key, f)
				return f
			end

		elseif f then
			local r = type(f) == 'function' and f(self) or f
			rawset(self, key, r)
			return r
		end
	end,

	__call = doLazyLoader
}

local ctlMeta2 = {
	__index = function(self, key)
		local f = rawget(rawget(self, -10000), key)
		if f then
			rawset(self, key, f)
			return f
		end

		f = rawget(self, -10001)(self, key)
		if f then
			return f
		end

		f = rawget(self, -10002)[key]
		if f then
			rawset(self, key, f)
			return f
		end

		f = loadables[key]
		if f == 1 then
			f = require('reeme.' .. key)

			local ft = type(f)
			if ft == 'function' then
				local r = f(self)

				rawset(self, key, r)
				loadables[key] = f

				return r
			end
			if ft == 'table' then
				rawset(self, key, f)
				return f
			end

		elseif f then
			local r = type(f) == 'function' and f(self) or f
			rawset(self, key, r)
			return r
		end
	end,

	__call = doLazyLoader
}

-----------------------------------------------------------------------------------------------------------------------
local responseView = require('reeme.response.view')
local viewMeta = responseView('getmeta')

local outputRedirect = function(app, v)
	ngx.say(v)
end

local defTblfmt = function(app, tbl)
	return string.json(tbl, string.JSON_UNICODES)
end

local defRouter = function(uri)
    if uri == '/' then
        return 'index', 'index'
    end

	local segs = string.split(uri, './')
    if #segs == 1 then
		return segs[1], 'index'
    end

	return string.lower(table.concat(segs, '.', 1, #segs - 1)), string.lower(segs[#segs])
end

local appMeta = {
	__index = {
		init = function(self, cfgs)
			self.configs = table.extend(table.new(0, 8), defConfigs, cfgs)
			return self
		end,

		config = function(self, name)
			return name and self.configs[name] or self.configs
		end,

		--设置响应方法
		on = function(self, name, func)
			local valids = { pre = 1, route = 1, err = 1, denied = 1, missmatch = 1, tblfmt = 1, output = 1, ['end'] = 1 }
			if type(func) == 'function' and valids[name] then
				self[name .. 'Proc'] = func
			else
				error('Error for application.on function call with invalid name: ' .. name)
			end
			return self
		end,

		--添加控制器外部用户变量
		addUsers = function(self, n, v)
			local u = self.users
			if type(n) == 'table' then
				for k,v in pairs(n) do
					u[k] = v
				end
			else
				u[n] = v
			end
			return self
		end,

		--设置所有控制器公共的父级
		setControllerBase = function(self, tbl)
			if type(tbl) == 'table' then
				self.controllerBase = tbl
			end
			return self
		end,

		--加载控制器，返回控制器实例和要执行的动作函数
		loadController = function(self, path, act)
			local dirs = self.configs.dirs
			local controlNew = nil

			while true do
				--加载
				local _, errmsg = pcall(function()
					controlNew = require(string.format('%s.%s.%s', dirs.appBaseDir, dirs.controllersDir, path:gsub('/', '.')))
				end)

				if controlNew then
					assert(type(controlNew) == 'function', string.format('Controller "%s" must return a function that can be create controller instance', path))
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

			--初始化
			local c = controlNew(act)
			local cmeta = getmetatable(c)

			if rawget(c, '__index') or not cmeta then
				--直接使用c为meta table，然后再创建一个controller实例
				cmeta = c
				c = table.new(0, 40)
			end

			cmeta = cmeta and rawget(cmeta, '__index') or nil
			if type(cmeta) == 'function' then
				setmetatable(c, ctlMeta2)
			else
				assert(type(cmeta) == 'table', 'The returned value for creator function of controller only can be function|table')
				setmetatable(c, ctlMeta)
			end

			rawset(c, -10000, self.users)
			rawset(c, -10001, cmeta)
			rawset(c, -10002, self.controllerBase)

			rawset(c, 'thisApp', self)

			return c, c[act .. 'Action']
		end,

		run = function(self)
			local r, c, actionMethod
			local path, act = (self.routeProc or defRouter)(ngx.var.uri)

			--载入控制器
			c, actionMethod, r = self:loadController(path, act)
			if r == true then
				--halt it
				return
			end

			self.currentControllerName = path
			self.currentRequestAction = act

			local _, err = xpcall(function()
				if self.preProc then
					--执行动作前响应函数
					r = self.preProc(self, c, path, act, actionMethod)
					local tp = type(r)

					if tp == 'table' then
						if r.response then
							actionMethod = nil
							r = r.response
						elseif r.controller then
							c, actionMethod = r.controller, r.method
						elseif r.method then
							actionMethod = r.method
						end

					elseif tp == 'string' then
						actionMethod = nil
					end
				end

				if actionMethod and type(actionMethod) ~= 'function' then
					--如果没有动作前响应函数又没有动作，那么就报错
					error(string.format('the action "%s" of controller "%s" undefined or it is not a function', act, path))
				end

				--执行动作
				if c and actionMethod then
					r = actionMethod(c)

				elseif r == nil then
					if self.missmatchProc then
						r = self.missmatchProc(self, path, act)
					else
						error(string.format('the action "%s" of controller "%s" undefined', act, path))
					end
				end

				--结束动作
				if self.endProc then
					local newr = self.endProc(self, c, path, act, r)
					if newr ~= nil then
						r = newr
					end
				end

				--处理返回的结果
				local redirect = rawget(c.response, '__redirect')
				if redirect then
					--URL重定向的话就不需要输出任何其它内容了
					ngx.redirect(redirect)

				else
					--否则的话，以response里的内容为主，如果response里没内容，再使用返回值
					local resp = c.response:finish() or r
					if resp ~= nil then
						local tp = type(resp)
						local out = self.outputProc or outputRedirect

						if tp == 'table' then
							--可能是一个view也可能是json
							local o
							if getmetatable(resp) == viewMeta then
								o = resp:content()
							else
								o = (self.tblfmtProc or defTblfmt)(self, resp)
							end

							out(self, o)

						elseif tp == 'string' then
							--字符串直接输出
							out(self, resp)

						elseif resp == false then
							--动作函数返回false表示拒绝访问，此时如果定义有deniedProc的话就会被执行，否则统一返回ErrorAccess的报错
							if self.deniedProc then
								self.deniedProc(self, c, path, act)
							else
								out(self, 'Denied Error Access!')
							end
						end
					end
				end

			end, debug.traceback)

			--清理
			if c then
				local lazyLoaders = self.lazyModules
				for ffree, loader in pairs(lazyLoaders) do
					ffree(c, loader.r, loader.params)
				end
			end
			c = nil

			if err then
				local out = self.outputProc or outputRedirect
				local errtp = type(err)

				if errtp == "string" then
					out(self, err)
				elseif errtp == "table" then
					local tblfmt = self.tblfmtProc or defTblfmt
					out(self, tblfmt(self, err))
				end

				ngx.eof()
			end
		end,
	}
}

return function()
	return setmetatable({ users = { }, controllerBase = { }, lazyModules = table.new(0, 4), currentControllerName = '', currentRequestAction = '', configs = defConfigs }, appMeta)
end
