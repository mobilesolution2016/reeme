return function(app)
	app:init({
		mongodb = function(reeme)
			--https://github.com/LuaDist2/lua-resty-mongol3
			local mongol = require("resty.mongol")
			local client = mongol:new()
			client:set_timeout(1000)
			if not client:connect("192.168.3.252") then
				error("mongodb connect failed")
			end
			local testDb = client:new_db_handle("testDb")
			return testDb, client
		end,
		
		mysqldb = function(reeme)	
			--https://github.com/openresty/lua-resty-mysql
			local mysql = require "resty.mysql"
			local db, err = mysql:new()
			if not db then
				error("failed to instantiate mysql: ", err)
			end
			db:set_timeout(1000)
			local ok, err, errcode, sqlstate = db:connect({
				host = "192.168.3.252",
				port = 3306,
				database = "test",
				user = "root",
				password = "mysql@2016",
				max_packet_size = 1024 * 1024 
			})
			
			if not ok then
				error("failed to connect mysql: " .. err .. ": " .. errcode .. " " .. sqlstate)
			end
			
			return db
		end,
		
		memcache = function(reeme)
			--https://github.com/openresty/lua-resty-memcached
			local memcached = require "resty.memcached"
			local memc, err = memcached:new()
			if not memc then
				error("failed to instantiate memc: " .. err)
			end
			
			memc:set_timeout(1000)
			local ok, err = memc:connect("192.168.3.252", 11211)
			if not ok then
				error("failed to connect memcache: " .. err)
			end
			
			return memc
		end,
		
		redis = function(name)
			----https://github.com/openresty/lua-resty-redis
			local redis = require("resty.redis")
			local red = redis:new()
			red:set_timeout(1000)
			
			local ok, err = red:connect("192.168.3.252", 6379)
			if not ok then
				error("failed to connect: " .. err)
			end
			
			return red
		end
	})
	
	app:forever({
		preResponse = function(reeme)
			reeme.response.headers['Content-type'] = 'text/html;charset=utf-8'
			reeme.response.headers["Access-Control-Allow-Origin"] = "*"
		end,
	})
	
	app:run()
end