local viewDir = nil 
local viewMeta = { }

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
		
		local f, err = io.open(tpl, "rb")
		if f then
			local t = f:read("*all")
			f:close()
			
			return t
		end
	end
end

local function loadSubtemplate(self, env, name)
	local t = loadTemplateFile(self.__reeme, name)
	if t then
		return string.parseTemplate(self, t, env)
	end
	return ''
end

local function setCachesection(isBegin, rets, caches, ...)	
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

local renderMethods = {
	debug = function(self, olds, source, env)
		local r = string.parseTemplate(self, source)
		rawset(self, 'finalHTML', r)
		return self
	end,
	overwrite = function(self, olds, source, env)
		local r = string.parseTemplate(self, source, env)
		rawset(self, 'finalHTML', r)
		return self
	end,
	append = function(self, olds, source, env)
		local r = string.parseTemplate(self, source, env)
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
		local src = rawget(self, 'sourceCode')
		if not src then
			error('render view with no source code')
		end
		
		--切换meta
		local vals = {
			self = self,
			reeme = self.__reeme,
			subtemplate = loadSubtemplate,
			cachesection = setCachesection,
			__cachesecs__ = rawget(self, 'caches'),
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
		local m = renderMethods[method or 'overwrite']
		if not m then
			m = renderMethods.overwrite
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
	local t = tpl and loadTemplateFile(r, tpl) or nil
	return setmetatable({ __reeme = r, sourceCode = t }, viewMeta)
end