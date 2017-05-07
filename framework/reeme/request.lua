local readables = {
	get = function(reeme) return ngx.req.get_uri_args() end,
	args = function(reeme, asArray) 
		local args = table.new(0, 12)
		local get, post = reeme.request.get, reeme.request.post
		
		if get then
			for k, v in pairs(get) do
				args[k] = v
			end
		end			
		if post.__post then
			for k, v in pairs(post.__post) do
				args[k] = v
			end
		end

		return args
	end,
	argsArray = function(reeme)
		local args = table.new(0, 16)
		local get, post = reeme.request.get, reeme.request.post
		
		if get then
			for k, v in pairs(get) do
				args[#args + 1] = string.format('%s=%s', k, v)
			end
		end
		if post.__post then
			for k, v in pairs(post.__post) do
				args[#args + 1] = string.format('%s=%s', k, v)
			end
		end
		
		return args
	end,
	post = function(reeme) return require("reeme.request.post")(reeme) end,
	headers = function(reeme) return require("reeme.request.headers")(reeme) end,
	
	fulluri = function(reeme)
		local query = ngx.var.query_string
		if query then
			return string.format('%s://%s%s?%s', ngx.var.scheme, ngx.var.host, ngx.var.uri, query)
		end
		return string.format('%s://%s%s', ngx.var.scheme, ngx.var.host, ngx.var.uri)
	end,
	
	isInternal = function(reeme) return ngx.req.is_internal end,
	startTime = function(reeme) return ngx.req.start_time end,
	version = function(reeme) return ngx.req.http_version end,
	rawHeader = function(reeme) return ngx.req.raw_header end,
	method = function(reeme) return ngx.req.get_method() end,
	isPost = function(reeme) return ngx.req.get_method() == "POST" end,
	isSubrequest = function(reeme) return ngx.is_subrequest end,
	contentLength = function(reeme) return ngx.req.content_length end,
	contentType = function(reeme) return ngx.var.content_type end,
	host = function(reeme) return ngx.var.host end,
	hostname = function(reeme) return ngx.var.hostname end,
	cookie = function(reeme) return ngx.var.http_cookie end,
	referer = function(reeme) return ngx.var.http_referer end,
	userAgent = function(reeme) return ngx.var.http_user_agent end,
	via = function(reeme) return ngx.var.http_via end,
	xForwardedFor = function(reeme) return ngx.var.http_x_forwarded_for end,
	remoteAddr = function(reeme) return ngx.var.remote_addr end,
	remotePort = function(reeme) return ngx.var.remote_port end,
	remoteUser = function(reeme) return ngx.var.remote_user end,
	remotePassword = function(reeme) return ngx.var.remote_passwd end,
	uri = function(reeme) return ngx.var.uri end,
	scheme = function(reeme) return ngx.var.scheme end,
	status = function(reeme) return ngx.status end,
	serverName = function(reeme) return ngx.var.server_name end,
	serverAddr = function(reeme) return ngx.var.server_addr end,
}

local writeables = {
	method = ngx.req.set_method,
	uri = ngx.req.set_uri,
	uriArgs = ngx.req.set_uri_args,
}

local RequestBase = {
	__index = function(self, key)
		local f = readables[key]
		if type(f) == "function" then
			local r = f(self.R)
			rawset(self, key, r)
			return r
		end
		
		return f
	end,
	__newindex = function(self, key, value)
		local f = writeables[key]
		local tp = type(f)
		if tp == "function" then
			f(value)
		end
	end,
	__call = function(self, keys)
		local get, post = self.get, self.post
		if type(keys) == 'table' then
			local cc = #keys
			if cc > 0 then
				local r = table.new(0, #keys)
				for i = 1, #keys do
					local k = keys[i]
					r[k] = get[k] or post[k]
				end
				return r
			else
				for k,v in pairs(keys) do
					keys[k] = get[k] or post[k]
				end
				return keys
			end
		end

		local r = table.new(0, 6)
		string.split(keys, ',', string.SPLIT_ASKEY, r)
		
		for k,_ in pairs(r) do
			r[k] = get[k] or post[k]
		end
		
		return r
	end
}

local RequestStatics = {
	clearHeadeer = function()
		ngx.req.clear_header()
	end,
	setBodyData = function(self, data)
		ngx.req.set_body_data(data)
	end,
	setBodyFile = function(self, filename, bAutoClear)
		ngx.req.set_body_file(filename, bAutoClear)
	end,
	initBody = function(self, bufferSize)
		ngx.req.init_body(bufferSize)
	end,
	appendBody = function(self, dataChunk)
		ngx.req.append_body(dataChunk)
	end,
	finishBody = function()
		ngx.req.finish_body()
	end,
}

return function(reeme)
	local request = table.new(0, 8)
	
	request.R = reeme
	for k,f in pairs(RequestStatics) do
		request[k] = f
	end

	return setmetatable(request, RequestBase)
end