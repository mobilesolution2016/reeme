require("reeme.application")({
	mongoDb = function(reeme)
		local mongol = require("resty.mongol")
		local client = mongol:new()
		client:set_timeout(1000)
		if client:connect("192.168.3.252") then
			local testDb = client:new_db_handle("testDb")
			return testDb, client
		end
	end,
})