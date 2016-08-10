local index = {
	__index = {
		index = function(self)
			self.response.status = 403
			self.response.headers.coder = "reeme"
			
			self.response:write(self.cookie.id)
			self.cookie({ key = "id", value = "abcd" })
			
			self.response:begin()
			self.response:write(self.request.get.a)
			self.response:write(self.request.post.a)
			self.response:write(self.request.headers["user-agent"])
			self.response:write(self.response.headers.Connection)
			self.response:write(self.request.serverName)
			self.response:finish("<br/>")
			
			self.response:begin()
			self.response:write("some thing")
			self.response:clear()
			self.response:finish()
			
			local view = self.response:useView("error.html")
			view.message = "hello " .. self.request.remoteAddr
			view:render()
			
			view:render("error.html", { message = "second render" })
			
			self:log("index/index")
			local mongodb, mongoClient = self("mongoDb")
			if not mongodb then
				error("mongodb connect failed")
			end
			
			self.response:write("mongodb connect succeed")
			
			local users = mongodb:get_col("users")
			--local user = {
			--	username = "user",
			--	password = "pwd",
			--}
			--local ret, errmsg = users:insert({ user }, 0, 1)
			--ngx.say(ret, errmsg)
			
			local cursor = users:find({ })
			self.response:write("users count: " .. users:count({ }))
			for k, v in cursor:pairs() do
				self.response:write("username:", v.username, "password:", v.password)
			end
			mongoClient:set_keepalive(10000, 100)
		
			return { id = "001", msg = "this is a msg" }
			--return "hello reeme"
		end
	}
}


return function(act)
	local c = { }
	return setmetatable(c, index)
end