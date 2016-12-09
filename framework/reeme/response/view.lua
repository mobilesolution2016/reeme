local viewDir = nil
local viewMeta = { }
local globalTplCaches = nil
local globalSectionCaches = nil

--tpl为文件名，如果以/开头则表示绝对路径（绝对路径将被直接使用不会做任何修改），否则将会被加上views路径以及默认的view文件扩展名后再做为路径被使用
local function getTemplatePathname(reeme, tpl)
	if tpl:byte(1) ~= 47 then
		local cfgs = reeme.thisApp.configs
		local dirs = cfgs.dirs

		return string.format("%s/%s/%s/%s%s", ngx.var.APP_ROOT, dirs.appBaseDir, dirs.viewsDir, tpl:replace('.', '/'), cfgs.viewFileExt)
	end
	return tpl:sub(2)
end

--从磁盘读入模板文件
local function loadTemplateFile(reeme, tpl, pathname)
	local t
	local f, err = io.open(pathname or getTemplatePathname(reeme, tpl), 'rb')
	if f then
		t = f:read("*all")
		f:close()

	elseif err then
		error(string.format('file io error when load template file: %s', tostring(err)))
	end

	return t
end

--将分段的模板名称模拟为函数调用
local subNameCallMeta = {
	__call = function(self, params)
		if type(params) == 'table' then
			return string.replace(self[1], params)
		end

		return self[1]
	end,
}
local function convertSubSegName(segs)
	for n,v in pairs(segs) do
		local pos = string.find(n, '(', 2, true)
		if pos then
			--将原名字对应的值去掉，换成模拟函数调用的值
			segs[n] = nil
			n = n:sub(1, pos - 1)
			segs[n] = setmetatable({ v }, subNameCallMeta)
		end
	end

	return segs
end

--载入子模板并解析后返回最终的HTML代码
local function loadSubtemplate(self, env, name, envForSub)
	local t, r, ok, err
	local restore = nil
	local reeme = self.__reeme

	if type(envForSub) == 'table' then
		--专用于这个模板的附加值，先保存原值再设置
		restore = table.new(0, 8)
		for k,v in pairs(envForSub) do
			restore[k] = env[k]
			env[k] = v
		end
	end

	local pathname = getTemplatePathname(reeme, name)

	if globalTplCaches then
		--从缓存中载入
		local tType
		r, tType = globalTplCaches:get(reeme, pathname)

		if r then
			if tType ~= 'loaded' then
				r = loadstring(r, '__templ_tempr__')
				assert(type(r) == 'function')
			end
			if tType == 'parsed' then
				r = r()
			end

			ok, err = pcall(function() r = setfenv(r, env)(self, env) end)
			if not ok then
				--运行模板函数时出错，报错
				t = loadTemplateFile(reeme, nil, pathname)

				rawset(env, '__sub_err_msg', err)
				rawset(env, '__sub_err_fullcode', t and string.parseTemplate(self, t) or string.format('subtemplate "%s" its source file losted'))
				error(err)
			end
		end
	end

	if not r then
		--加载模板文件，并解析和运行模板代码
		t = loadTemplateFile(reeme, nil, pathname)
		if not t then
			return string.format('subtemplate("%s") load failed', name)
		end

		r, err = string.parseTemplate(self, t, true)
		if type(err) == 'string' then
			error(string.format('Error when parsing subtemplate: %s <br/><br/>%s', name, err))
		end

		assert(type(r) == 'function')

		local forCachedFunc = r
		ok, err = pcall(function() r = setfenv(r, env)(self, env) end)
		if ok then
			--运行模板函数正常，于是可以保存到全局缓存
			if globalTplCaches then
				globalTplCaches:set(reeme, pathname, forCachedFunc, 'loaded')
			end

			if type(r) == 'table' then
				convertSubSegName(r)
			end
		else
			--运行模板函数出错，报错
			rawset(env, '__sub_err_msg', err)
			rawset(env, '__sub_err_fullcode', string.parseTemplate(self, t))
			error(err)
		end
	end

	if restore then
		--恢复被覆盖的值
		for k,v in pairs(restore) do
			env[k] = restore[k]
		end
	end

	return r
end

--使用模板内缓存
local function setCachesection(viewself, isBegin, rets, caches, ...)
	local cc = #caches
	if isBegin then
		--创建
		if select('#', ...) == 0 then
			caches[cc + 1] = { s = #rets }
			return true
		end

		local conds = table.concat({ viewself.tplName, ... }, '')
		if globalSectionCaches then
			local cached = globalSectionCaches[conds]
			if cached then
				--条件相同的缓存数据存在，于是返回false表示不需要继续解析接下来到cache end之间的代码
				rets[#rets + 1] = cached
				return false
			end
		end

		caches[cc + 1] = { c = conds, s = #rets }

	elseif cc > 0 then
		--结束。将开始到结束之间的所有代码组合后缓存起来
		local last = table.remove(caches)

		cc = #rets
		local k = 1
		local newtbl = table.new(cc - last.s, 0)
		for i = last.s + 1, cc do
			newtbl[k] = rets[i]
			k = k + 1
		end

		while k > 1 do
			table.remove(rets)
			k = k - 1
		end

		k = table.concat(newtbl, '')
		if globalSectionCaches and last.c then
			globalSectionCaches[last.c] = k
		end
		rets[#rets + 1] = k

	else
		error('error for closed template cache, not paired with begin')
	end

	return true
end


--------------------------------------------------------------------------------------------------------
--从源文件载入模板代码时所采用的渲染方式
local sourceRenderMethods = {
	debug = function(self, source, env)
		local r, err = string.parseTemplate(self, source)
		rawset(self, 'finalHTML', type(err) == 'string' and err or r)
	end,
	overwrite = function(self, source, env)
		local r, err = string.parseTemplate(self, source, env)
		rawset(self, 'finalHTML', type(err) == 'string' and err or r)
	end,
	append = function(self, source, env)
		local r, err = string.parseTemplate(self, source, env)
		rawset(self, 'finalHTML', table.concat({ rawget(self, 'finalHTML'), type(err) == 'string' and err or r }, ''))
	end,
}

--从Cache中载入模板时所采用的渲染方式
local function reportRunError(self, err)
	local t = loadTemplateFile(self.__reeme, nil, self.tplPathname)
	local matchs = ngx.re.match(err, '\\[string "([^"]+)"\\]:([\\d]+):')
	local r = {
		matchs and string.format('[string "%s"]:%d:%s', matchs[1], tonumber(matchs[2]) + 2, matchs[3]) or err,
		t and string.parseTemplate(self, t) or string.format('template "%s" its source file losted')
	}

	rawset(self, 'finalHTML', table.concat(r, '<br/><br/>\r\n\r\n'))
end

local parsedRenderMethods = {
	debug = function(self, source, env)
	end,
	overwrite = function(self, source, env)
		local ok, err = pcall(function()
			local r = setfenv(source, env)(self, env)
			rawset(self, 'finalHTML', r)
		end)
		if not ok then
			reportRunError(self, err)
		end
	end,
	append = function(self, source, env)
		local ok, err = pcall(function()
			local r = setfenv(source, env)(self, env)
			rawset(self, 'finalHTML', table.concat({ rawget(self, 'finalHTML'), r }, ''))
		end)
		if not ok then
			reportRunError(self, err)
		end
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

	safemode = function(self, is)
		if type(is) == 'boolean' then
			rawset(self, '__safemode', is)
		else
			return rawget(self, '__selfmode')
		end
		return self
	end,

	--参数2为所有的模板参数，参数3为nil表示重新渲染并且清除掉上一次的结果，否则将会累加在上一次的结果之后（如果本参数不是true而是string，那么将会做为join字符串放在累加的字符串中间）
	render = function(self, env, method)
		local gblCaches
		local methods = parsedRenderMethods
		local src = rawget(self, 'parsedTempl')

		if not src then
			gblCaches = globalTplCaches
			methods = sourceRenderMethods
			src = rawget(self, 'sourceCode')
		end

		--构造vals，切换meta
		local vals = {
			self = self,
			reeme = self.__reeme,
			subtemplate = loadSubtemplate,
			cachesection = setCachesection,
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

		if method ~= 'debug' and gblCaches then
			--若有缓存则写入缓存
			local r, segs = string.parseTemplate(self, src, true)
			if type(r) == 'function' then
				--入口模板不可以出现分段，分段只能用于子模板
				if segs == true then
					error(string.format("render view '%s' failed: segments only can be used with subtemplate", self.tplPathname))
				end

				src = r
				methods = parsedRenderMethods
				gblCaches:set(self.__reeme, self.tplPathname, src, 'loaded')
			else
				--解析时出错了，此时r是错误提示以及完整的模板解析后的代码
				src = r
			end
		end

		--按方法执行
		methods[method or 'overwrite'](self, src, env)

		--换回meta
		if oldMeta then
			setmetatable(env, oldMeta)
		end
		meta, vals = nil, nil

		return self
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


--------------------------------------------------------------------------------------------------------
--tpl是模板的名称，名称中的.符号将被替换为/，表示多级目录
return function(r, tpl)
	if type(r) == 'string' then
		if r == 'templatecache' then
			--设置全局模板缓存
			globalTplCaches = tpl
		elseif r == 'sectioncache' then
			--设置模板段缓存
			globalSectionCaches = tpl
		elseif r == 'getmeta' then
			return viewMeta
		elseif r == 'getcaches' then
			return globalTplCaches, globalSectionCaches
		end

		return
	end

	if type(tpl) == 'string' then
		local pathname = getTemplatePathname(r, tpl)
		if globalTplCaches then
			--从缓存中载入
			local t, tType = globalTplCaches:get(r, pathname)
			if t then
				if tType ~= 'loaded' then
					t = loadstring(t, '__templ_tempr__')
					assert(type(t) == 'function')
				end
				if tType == 'parsed' then
					t = t()
				end

				return setmetatable({ __reeme = r, parsedTempl = t, tplPathname = pathname, tplName = tpl }, viewMeta)
			end
		end

		--从源文件中载入
		local t = loadTemplateFile(r, nil, pathname)
		if t then
			return setmetatable({ __reeme = r, sourceCode = t, tplPathname = pathname, tplName = tpl }, viewMeta)
		end
	end

	return setmetatable({ __reeme = r }, viewMeta)
end
