local viewf = require("reeme.response.view")

local writeMembers = {
	status = function(self, value)
		ngx.status = value
	end,
}

local ResponseBase = {
	__index = {
		headers = require("reeme.response.headers")(),
	}
}

local Response = {
	__index = {
		--立即输出模式，不缓存（此种模式下，请自行处理redirect调用）
		immediate = function()
			rawset(self, 'body', nil)
		end,

		--缓存要输出的数据
		write = function(self, ...)
			local body = rawget(self, 'body')
			local v = string.merge(...)

			if body then
				body[#body + 1] = v
			else
				ngx.say(v)
			end
		end,

		--缓存一个渲染好的view
		writeView = function(self, view, env)
			if env then
				view:render(env)
			end

			local t = view:content()
			if t and #t > 0 then
				local body = rawget(self, 'body')
				if body then
					body[#body + 1] = t
				else
					ngx.say(v)
				end
			end
		end,

		--清空当前已有的数据
		clear = function(self)
			if rawget(self, 'body') then
				rawset(self, 'body', {})
			end
		end,

		--重定向（如果没有调用immediate，也仅用write/writeView输出过内容，那么在控制器Action结束之前的任何时候都可以重定向）
		redirect = function(self, url)
			rawset(self, 'body', nil)
			rawset(self, '__redirect', url)
		end,

		--结束（结束之后不能再使用write/writeView/redirect这些函数）
		finish = function(self)
			local body
			if not rawget(self, '__redirect') then
				body = rawget(self, 'body')
				if body then
					body = table.concat(body, '')
					if #body == 0 then
						body = nil
					end
				end
			end

			rawset(self, 'body', nil)
			return body
		end,

		--加载并渲染一个view
		view = function(self, tpl, env, method)
			local t = viewf(rawget(self, "R"), tpl)
			return t:render(env or {}, method)
		end,
		--安全解析模式加载一个view
		viewsafe = function(self, tpl, env, method)
			local t = viewf(rawget(self, "R"), tpl)
			return t:safemode(true):render(env or {}, method)
		end,
		--仅加载一个view返回该view实例
		loadView = function(self, tpl)
			return viewf(rawget(self, "R"), tpl)
		end,
	},

	__newindex = function(self, key, value)
		local f = writeMembers[key]
		if f then
			f(self, value)
		end
	end,
}
setmetatable(Response.__index, ResponseBase)

return function(reeme)
	local response = { R = reeme, body = {} }

	return setmetatable(response, Response)
end
