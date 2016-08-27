local viewDir = nil 
local viewMeta = { }

local function loadTemplateFile(reeme, tpl)
	if type(tpl) == 'string' then 
		local cfgs = reeme:getConfigs()
		local dirs = cfgs.dirs
		local f, err = io.open(string.format("%s/%s/%s/%s%s", dirs.appRootDir, dirs.appBaseDir, dirs.viewsDir, tpl:replace('.', '/'), cfgs.viewFileExt), "rb")
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
	--设置source
	setSource = function(self, souce)
		local tp = type(source)
		if tp == 'string' then
			rawset(self, 'sourceCode', source)
		elseif tp == 'table' then
			rawset(self, 'sourceCode', table.concat(source, ''))
		end
		return self
	end,
	--获取source
	source = function(self)
		return rawget(self, 'sourceCode')
	end,
	
	--参数2为所有的模板参数，参数3为nil表示重新渲染并且清除掉上一次的结果，否则将会累加在上一次的结果之后（如果本参数不是true而是string，那么将会做为join字符串放在累加的字符串中间）
	render = function(self, env, method)
		--如果没有source那么就无法渲染
		local src = rawget(self, 'sourceCode')
		if not src then
			return self
		end
		
		--切换meta
		local meta = { __index = {
			self = self,
			reeme = self.__reeme,
			subtemplate = loadSubtemplate
		}}
		setmetatable(meta.__index, { __index = _G })

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
		
		--换回meta
		if oldMeta then
			setmetatable(env, oldMeta)
		end
		meta = nil
		
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