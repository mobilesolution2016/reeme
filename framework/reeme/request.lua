local readables = {
	get = function(reeme) return ngx.req.get_uri_args() end,
	request = function(self, reeme) 
		local args = { }
		local get = rawget(self, "get")
		local post = rawget(self, "post")
		if not get then
			get = ngx.req.get_uri_args()
			rawset(self, "get", get)
		end
		if not post then
			post = require("reeme.request.post")(reeme)
			rawset(self, "post", post)
		end
		
		for k, v in pairs(get) do
			args[k] = v
		end
		for k, v in pairs(post.__post) do
			args[k] = v
		end
		
		return args
	end,
	post = function(self, reeme) return require("reeme.request.post")(reeme) end,
	headers = function(self, reeme) return require("reeme.request.headers")(reeme) end,
}

local RequestBase = {
	__index = function(self, key)
		local f = readables[key]
		if type(f) == "function" then
			local r = f(self, self.R)
			rawset(self, key, r)
			return r
		end
		
		return nil
	end,
}

local writeables = {
	method = ngx.req.set_method,
	uri = ngx.req.set_uri,
	uriArgs = ngx.req.set_uri_args,
}

local Request = {
	__index = {
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
	},
	__newindex = function(self, key, value)
		local f = writeables[key]
		local tp = type(f)
		if tp == "function" then
			f(value)
		end
	end,
}
setmetatable(Request.__index, RequestBase)

return function(reeme)
	local request = { 
		R = reeme, 
		
		isInternal = ngx.req.is_internal,
		startTime = ngx.req.start_time,
		version = ngx.req.http_version,
		rawHeader = ngx.req.raw_header,
		method = ngx.req.request_method,
		isPost = ngx.req.get_method() == "POST",
		isSubrequest = ngx.is_subrequest,
		contentLength = ngx.req.content_length,
		contentType = ngx.var.content_type,
		host = ngx.var.host,
		hostname = ngx.var.hostname,
		cookie = ngx.var.http_cookie,
		referer = ngx.var.http_referer,
		userAgent = ngx.var.http_user_agent,
		via = ngx.var.http_via,
		xForwardedFor = ngx.var.http_x_forwarded_for,
		remoteAddr = ngx.var.remote_addr,
		remotePort = ngx.var.remote_port,
		remoteUser = ngx.var.remote_user,
		remotePassword = ngx.var.remote_passwd,
		uri = ngx.var.uri,
		scheme = ngx.var.scheme,
		status = ngx.status,
		serverName = ngx.var.server_name,
		serverAddr = ngx.var.server_addr,
	}
	
	return setmetatable(request, Request)
end