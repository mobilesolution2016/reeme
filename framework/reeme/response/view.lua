local viewDir = nil 
local viewMeta = { }
local globalTplCaches = nil
local globalSectionCaches = nil

--tpl为文件名，如果以/开头则表示绝对路径（绝对路径将被直接使用不会做任何修改），否则将会被加上views路径以及默认的view文件扩展名后再做为路径被使用
local function loadTemplateFile(reeme, tpl)
	if type(tpl) == 'string' then 
		if tpl:byte(1) ~= 47 then
			local cfgs = reeme:getConfigs()
			local dirs = cfgs.dirs
			
			tpl = string.format("%s/%s/%s/%s%s", ngx.var.APP_ROOT, dirs.appBaseDir, dirs.viewsDir, tpl:replace('.', '/'), cfgs.viewFileExt)
		else
			tpl = tpl:sub(2)
		end
		
		local t
		if globalTplCaches then
			t = globalTplCaches:get(tpl)
			return t
		end

		local f, err = io.open(tpl, "rb")
		if f then
			t = f:read("*all")
			f:close()
		end
		
		return t, tpl
	end
end

local function dumpFile(name, code)
	local f, err = io.open(name, "wb")
	if f then
		t = f:write(code, #code)
		f:close()
	end
end

local function cacheTemplate(name, code)
	--如果缓存支持二进制，那么就编译
	if globalTplCaches.isSupportFunction then
		globalTplCaches:writeFunction(name, loadstring(code, '__templ_tempr__'))
	else
		globalTplCaches:writeCode(name, code)
	end
end

local subNameCallMeta = {
	__call = function(self, params)
		if type(params) == 'table' then
			return string.replace(self[1], params)
		end
		
		return self[1]
	end,
}

local function loadSubtemplate(self, env, name)
	local t, cacheName = loadTemplateFile(self.__reeme, name)
	if t then
		if cacheName then
			--有cache名字说明这不是缓存中取出来的
			t = string.parseTemplate(self, t, env)
			if not t then
				return ''
			end
		elseif type(t) == 'function' then
			--缓存的模板代码已经编译为函数了，直接调用
			t = t(self, env)
		else
			--缓存的模板代码，已经解析过了，只需要加载
			local f = loadstring(source, '__templ_tempr__')
			t = f(self, env)
		end
		
		if type(t) == 'table' then
			--返回的是一个分段命名的子模板，于是要检测其中是否含有函数调用式的命名
			for n,v in pairs(t) do
				local pos = string.find(n, '(', 2, true)
				if pos then
					--将原名字对应的值去掉，换成模拟函数调用的值
					t[n] = nil
					n = n:sub(1, pos - 1)
					t[n] = setmetatable({ v }, subNameCallMeta)
				end
			end
		end
		
		if cacheName and globalTplCaches then
			cacheTemplate(cacheName, t)
		end

		return t
	end
	
	return string.format('subtemplate("%s") failed, name not exists', name)
end

local function setCachesection(isBegin, rets, caches, ...)	
	if not caches then
		--cache没有，于是永远返回true，表示模板缓存或读取缓存失败
		return true
	end
	
	local cc = #caches
	if isBegin then
		--创建
		local conds = { ... }
		if #conds == 0 then
			error('template cache beginned with no condition(s)')
		end
		
		conds = table.concat(conds, ',')
		local cached = caches[conds]
		if cached then
			--条件相同的缓存数据存在，于是返回false表示不需要继续解析接下来到cache end之间的代码
			rets[#rets + 1] = cached
			return false
		end

		caches[cc + 1] = { c = conds, s = #rets }

	elseif cc > 0 then
		--结束。将开始到结束之间的所有代码组合后缓存起来
		local last = table.remove(caches, cc)

		cc = #rets
		local k = 1
		local newtbl = table.new(cc - last.s, 0)
		for i = last.s + 1, cc do
			newtbl[k] = rets[i]
			k = k + 1
		end

		caches[last.c] = table.concat(newtbl, '')
		
	else
		error('error for closed template cache, not paired with begin')
	end
	
	return true
end

local sourceRenderMethods = {
	debug = function(self, olds, source, env)
		local r = string.parseTemplate(self, source)
		rawset(self, 'finalHTML', r)
		return self
	end,
	overwrite = function(self, olds, source, env)
		local r = string.parseTemplate(self, source, env)
		dumpFile('d:/dump.txt', r)
		rawset(self, 'finalHTML', r)
		return self
	end,
	append = function(self, olds, source, env)
		local r = string.parseTemplate(self, source, env)
		rawset(self, 'finalHTML', table.concat({ olds, r }, ''))
		return self
	end,
}
local parsedRenderMethods = {
	debug = function(self, olds, source, env)
		if type(source) == 'function' then
			return 'template function cached mode cannot support "debug" render'
		end
		return source
	end,
	overwrite = function(self, olds, source, env)
		local f = source
		if type(f) == 'string' then
			f = loadstring(source, '__templ_tempr__')
		end

		local r = f(self, env)
		rawset(self, 'finalHTML', r)
		return self
	end,
	append = function(self, olds, source, env)
		local f = source
		if type(f) == 'string' then
			f = loadstring(source, '__templ_tempr__')
		end

		local r = f(self, env)
		rawset(self, 'finalHTML', table.concat({ olds, r }, ''))
		return self
	end,
}

viewMeta.__index = {
	--设置/获取source
	source = function(self, source)
		if source then
			local tp = type(source)
			if tp == 'string' then
				rawset(self, 'sourceCode', source)
			elseif tp == 'table' then
				rawset(self, 'sourceCode', table.concat(source, ''))
			end
			return self
		end
		return rawget(self, 'sourceCode')
	end,
	
	--启用模板分段缓存
	enableCache = function(self, cachesTable)
		if type(cachesTable) ~= 'table' then
			error('enable template cache must pass a table for cache working')
		end
		
		rawset(self, 'caches', cachesTable)
		return self
	end,
	--关闭模板分段缓存
	disableCache = function(self)
		rawset(self, 'caches', nil)
		rawset(self, 'cacheiden', nil)
		return self
	end,
	
	--参数2为所有的模板参数，参数3为nil表示重新渲染并且清除掉上一次的结果，否则将会累加在上一次的结果之后（如果本参数不是true而是string，那么将会做为join字符串放在累加的字符串中间）
	render = function(self, env, method)
		--如果没有source那么就无法渲染
		local methods = sourceRenderMethods
		local src = rawget(self, 'sourceCode')
		if not src then
			methods = parsedRenderMethods
			src = rawget(self, 'parsedCode')
			if not src then
				error('render view with no source code')
			end
		end

		--切换meta
		local vals = {
			self = self,
			reeme = self.__reeme,
			subtemplate = loadSubtemplate,
			cachesection = setCachesection,
			__cachesecs__ = rawget(self, 'caches') or globalSectionCaches,
		}
		local meta = { __index = vals, __newindex = vals }
		setmetatable(vals, { __index = _G })

		local oldMeta = nil
		if env then
			oldMeta = getmetatable(env)
			env = setmetatable(env, meta)
		else
			env = setmetatable({}, meta)
		end

		--使用某个方法渲染模板
		local m = sourceRenderMethods[method or 'overwrite']
		if not m then
			m = sourceRenderMethods.overwrite
		end
		
		r = m(self, rawget(self, 'finalHTML'), src, env)
		
		if vals.__cachesecs__ and #vals.__cachesecs__ > 0 then
			error('render template with cache(s) not closed')
		end

		--换回meta
		if oldMeta then
			setmetatable(env, oldMeta)
		end
		meta, vals = nil, nil
		
		return r
	end,
	
	--获取render之后的内容
	content = function(self)
		return rawget(self, 'finalHTML') or ''
	end,
	--直接设置内容
	setContent = function(self, code)
		local tp = type(code)
		if tp == 'string' then
			rawset(self, 'finalHTML', code)
		elseif tp == 'table' then
			rawset(self, 'finalHTML', table.concat(code, ''))
		end
		return self
	end,
	
	--在渲染后的结果中追加字符串
	appendString = function(self, code)
		if type(code) == 'string' then
			local cur = rawget(self, 'finalHTML')
			rawset(self, 'finalHTML', cur and (cur .. code) or code)
		end
		return self
	end,
	--合并多个模板，joinPiece是两个模板之间的连接字符串，可以为空字符串但不能为非string类型的变量。合并后的结果直接返回而不会覆盖掉当前模板的内容
	merge = function(self, joinPiece, ...)
		local t = { rawget(self, 'finalHTML') }
		for i = 1, select('#', ...) do
			local another = select(i, ...)
			if getmetatable(another) then
				t[#t + 1] = rawget(another, 'finalHTML') or ''
			else
				for k = 1, #another do
					t[#t + 1] = rawget(another[k], 'finalHTML') or ''
				end
			end
		end
		return table.concat(t, joinPiece)
	end
}

viewMeta.__concat = function(self, another)
	local a, b = rawget(self, 'finalHTML'), rawget(another, 'finalHTML')
	if a and b then
		return a .. b
	end
	return a or b
end


--tpl是模板的名称，名称中的.符号将被替换为/，表示多级目录
return function(r, tpl)
	if type(r) == 'string' then
		if r == 'templatecache' then
			--设置全局模板缓存
			globalTplCaches = tbl
		elseif r == 'sectioncache' then
			--设置模板段缓存
			globalSectionCaches = tbl
		elseif r == 'getmeta' then
			return viewMeta
		end
		
		return
	end
	
	if tpl then
		local t, cacheName = loadTemplateFile(r, tpl)
		if t then
			if cacheName then
				--返回了缓存名字说明这个模板不是从缓存中取出来的，那么t就是源码，顺便缓存一下其代码
				if globalTplCaches then
					cacheTemplate(cacheName, t)
				end

				return setmetatable({ __reeme = r, sourceCode = t }, viewMeta)
			else
				--没有缓存名字，说明模板是从缓存中取出来的，就不要再设置sourceCode了
				return setmetatable({ __reeme = r, parsedCode = t }, viewMeta)
			end
		end
	end
	
	return setmetatable({ __reeme = r }, viewMeta)
end