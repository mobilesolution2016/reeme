local LoginController = { }

--http://localhost/login/login?username=admin&password=1234567
function LoginController:login(reeme)
	local username = reeme.request.username
	local password = reeme.request.password
	
	local ret = {
		code = 0,
	}
	
	local mongol = require("resty.mongol")
	local conn = mongol:new()
	conn:set_timeout(2000)
	if conn:connect("192.168.3.252") then
		
		local testDb = conn:new_db_handle("testDb")
		local users = testDb:get_col("users")
		
		local user = {
			username = "user",
			password = "pwd",
		}
		--local ret, errmsg = users:insert({ user }, 0, 1)
		--ngx.say(ret, errmsg)
		
		local cursor = users:find({ })
		for k, v in cursor:pairs() do
			ngx.say(k)
			--for kk, vv in pairs(v) do
			--	ngx.say(kk, vv)
			--end
		end
		
		conn:set_keepalive(10000, 100)
		--conn:close()
	end
	--ngx.say(mongol)
	
	if not username or #username < 5 then
		--error("username too short")
		ret.code = 1
		ret.msg = "username too short"
	elseif not password or #password < 6 then
		ret.code = 2
		--error("password too short")
		ret.msg = "password too short"
	end
	
	if ret.code == 0 then
		ret.data = {
			username = username,
		}
	end
	
	return require("cjson").encode(ret)
end

return LoginController