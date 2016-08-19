local index = {
	__index = {
		index = function(self)
			self.response:write("is working")
			do return end
			self.response.status = self.statusCode.HTTP_FORBIDDEN
			self.response.headers.coder = "reeme"
			--self.exec("/login")
			--self.responseredirect("/login")
			--local res = self.capture("/login", { user = "admin" })
			--self.response:write("capture: ", res.status--[[, res.header, res.body, res.truncated]])
			--local res1, res2 = self.captureMulti({ { "/login", { user = "admin" } }, { "/login" }} )
			--self.utils.setTimer(5, function(premature, name)
			--	self.log(nil, "onTimer", name)
			--end, self.request.serverName)
			--self.response:write(self.quoteSql('a is "b"'))
			
			--[[local rets, bOk = self.validator({
				{ "id", "123", self.validator:number(), nil },
				{ "id2", "456", self.validator:number(), self.validator:equal(2), 234 }
			})
			ngx.say("bOk", bOk, rets.id, rets.id2)]]
			
			for k, v in pairs(self.request.post.files) do
				self.response:write("file name", v.name)
				self.response:write("file type", v.type)
				self.response:write("file file", v.file)
				self.response:write("file temp", v.temp)
				self.response:write("moveFile", v:moveFile("e:/" .. v.file))
			end
			
			self.response:write(self.cookie.id)
			self.cookie({ key = "id", value = "abcd" })
			
			self.response:begin()
			self.response:write(self.request.get.a)
			self.response:write(self.request.post.b)
			self.response:write(self.request.request.c)
			self.response:write(self.request.headers["user-agent"])
			self.response:write(self.response.headers.Connection)
			self.response:write(self.request.serverName)
			self.response:finish()
			
			self.response:begin()
			self.response:write("some thing")
			self.response:clear()
			self.response:finish()
			
			local view = self.response:initView("error.html", {
				message = "hello " .. self.request.remoteAddr
			})
			view.message = "message write"
			view:render()
			
			self.log(self.logLevel.WARN, "index/index")
			
			--mongodb
			local mongodb, mongoClient = self("mongodb")
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
			--mongoClient:close()
		
		
			--mysql
			local mysqldb = self("mysqldb")
			local res, err, errcode, sqlstate = mysqldb:query("SHOW TABLES")
			if not res then
				error("bad result: " .. err .. ": " .. errcode .. ": " .. sqlstate .. ".")
			end
			
			res, err, errcode, sqlstate = mysqldb:query("SELECT * FROM testTable")
			if not res then
				error("bad result: " .. err .. ": " .. errcode .. ": " .. sqlstate .. ".")
			end
			
			local ok, err = mysqldb:set_keepalive(10000, 100)
			if not ok then
				error("failed to set keepalive: " .. err)
			end
			--mysqldb:close()
			--return res
			
			
			--memcache
			local memc = self("memcache")
			ok, err = memc:flush_all()
			if not ok then
				error("failed to flush all: " .. err)
			end
			
			local res, flags, err = memc:get("thing")
			if err then
				error("failed to get thing: " .. err)
			end
			if not res then
				self.response:write("thing not found")
			else
				self.response:write("thing: ", res)
			end
	
			local ok, err = memc:set("thing", 32)
			if not ok then
				error("failed to set thing: " .. err)
			end
			local res, flags, err = memc:get("thing")
			if err then
				error("failed to get thing: " .. err)
			end
			if not res then
				self.response:write("thing not found")
			end
			self.response:write("thing: ", res)
			
			ok, err = memc:flush_all()
			if not ok then
				error("failed to flush all: " .. err)
			end
			
			local ok, err = memc:set_keepalive(10000, 100)
			if not ok then
				error("cannot set keepalive: " .. err)
			end
			--memc:close()
			
			
			--redis
			local red = self("redis")
			local res, err = red:get("thing")
			if not res then
				error("failed to get thing: " .. err)
			end

			if res == ngx.null then
				self.response:write("thing not found.")
			else
				self.response:write("thing: ", res)
			end

			red:init_pipeline()
			red:set("thing", "Marry")
			red:set("horse", "Bob")
			red:get("cat")
			red:get("horse")
			local results, err = red:commit_pipeline()
			if not results then
				error("failed to commit the pipelined requests: " .. err)
			end

			for i, res in ipairs(results) do
				if type(res) == "table" then
					if res[1] == false then
						self.response:write("failed to run command ", i, ": ", res[2])
					end
				end
			end

			local ok, err = red:set_keepalive(10000, 100)
			if not ok then
				error("failed to set keepalive: " .. err)
				return
			end
			--red:close()
			
			--return { id = "001", msg = "this is a msg" }
			--return "hello reeme"
			return view
		end
	}
}

return function(act)
	local c = { }
	return setmetatable(c, index)
end