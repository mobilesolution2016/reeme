local models = {}
local modelmeta = require('reeme.orm.model')
local builder = require('reeme.orm.mysqlbuilder')
local parseFields = require('reeme.orm.common').parseFields

local mysql = {
	__index = {
		--使用一个定义的模型
		--不能使用require直接引用一个模型定义的Lua文件来进行使用，必须通过本函数来引用
		use = function(self, name, db)
			local idxName = string.format('%s-%s', ngx.var.APP_NAME or ngx.var.APP_ROOT, name)
			local m = models[idxName]
			local reeme = self.R

			if not m then			
				local cfgs = reeme:getConfigs()
				
				m = require(string.format('%s.%s.%s', cfgs.dirs.appBaseDir, cfgs.dirs.modelsDir, name))
				if type(m) ~= 'table' or type(m.fields) ~= 'table' then
					error(string.format("mysql:use('%s') get a invalid model declaration", name))
					return nil
				end
				
				local oldm = getmetatable(m)
				if oldm and type(oldm.__index) ~= 'table' then
					error(string.format("mysql:use('%s') get a model table, but the __index of its metatable not a table", name))
					return nil
				end
				
				m.__name = name
				m.__oldm = oldm
				m.__builder = builder
				m.__dbtype = 'mysql'

				local err = parseFields(m)
				if err ~= true then
					error(string.format("mysql:use('%s') parse failed: %s", name, err))
					return nil
				end

				setmetatable(oldm and oldm.__index or m, modelmeta)
				models[idxName] = m
			end

			m.__reeme = reeme
			m.__db = type(db) == 'string' and reeme(db) or db
			
			return m
		end,
		
		uses = function(self, names, db)
			local tp = type(names)
			
			if type(db) == 'string' then
				db = reeme(db)
			end
			
			if tp == 'table' then
				local r = table.new(0, #names)
				for i = 1, #names do
					local n = names[i]
					r[n] = self:use(n, db)
				end
				return r
			elseif tp == 'string' then
				local r = table.new(0, 10)
				string.split(names, ',', string.SPLIT_ASKEY, r)
				
				for k,v in pairs(r) do
					r[k] = self:use(k, db)
				end
				return r
			end
		end,
		
		--清理所有的model缓存
		clear = function(self)
			for k,m in pairs(models) do
				if m.__oldm then
					setmetatable(m, m.__oldm)
				end
				
				m.__name, m.__fields, m.__fieldPlain, m.__oldm, m.__db = nil, nil, nil, nil, nil
			end
			
			models = {}
		end,
		
		--重新加载指定的Model
		reload = function(self, name, db)
			local idxName = string.format('%s-%s', ngx.var.APP_NAME or ngx.var.APP_ROOT, name)
			if models[idxName] then
				models[idxName] = nil
				return self:use(idxName, db)
			end
		end,
	},
	
	__call = function(self, p1)
		return self:use(p1)
	end
}

return function(reeme)
	return setmetatable({ R = reeme }, mysql)
end