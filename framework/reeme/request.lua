local members = {
	get = function(reeme) return require("reeme.request.get")(reeme) end,
	post = function(reeme) return require("reeme.request.post")(reeme) end,
	headers = function(reeme) return require("reeme.request.headers")(reeme) end,
	--the current request's request method name. Strings like "GET" and "POST"
	method = function(reeme) return ngx.req.get_method() end,
	isPost = function(reeme) return ngx.req.get_method() == "POST" end,
	contentType = function(reeme) return ngx.var.content_type end,
	referer = function(reeme) return ngx.var.http_referer end,
	userAgent = function(reeme) return ngx.var.http_user_agent end,
	remoteAddr = function(reeme) return ngx.var.remote_addr end,
	uri = function(reeme) return ngx.var.uri end,
	serverName = function(reeme) return ngx.var.server_name end,
	serverAddr = function(reeme) return ngx.var.server_addr end,
	scheme = function(reeme) return ngx.var.scheme end,
	status = function(reeme) return ngx.status end,
	
	--a boolean indicating whether the current request is an "internal request",
	isInternal = function(reeme) return ngx.req.is_internal end,
	--returns true if the current request is an nginx subrequest, or false otherwise.
	isSubrequest = function(reeme) return ngx.is_subrequest end,
	--a floating-point number representing the timestamp (including milliseconds as the decimal part) when the current request was created
	startTime = function(reeme) return ngx.req.start_time end,
	--the HTTP version number for the current request as a Lua number
	httpVersion = function(reeme) return ngx.req.http_version end,
	--the original raw HTTP protocol header received by the Nginx server
	rawHeader = function(reeme) return ngx.req.raw_header end,
}

local Request = {
	__index = function(self, key)
		local f = members[key]
		if type(f) == "function" then
			local r = f(self.R)
			rawset(self, key, r)
			return r
		end
		
		return nil
	end,
}

return function(reeme)
	local request = { R = reeme }
	
	return setmetatable(request, Request)
end