local models = {}
local modelmeta = require('reeme.orm.model')
local builder = require('reeme.orm.mysqlbuilder')
local parseFields = require('reeme.orm.common').parseFields

local mysql = {
	__index = {
		defdb = function(self, db)
			local old = self.__defdb
			if db then
				self.__defdb = db
			end
			return old
		end,
		
		--使用一个定义的模型
		--不能使用require直接引用一个模型定义的Lua文件来进行使用，必须通过本函数来引用
		--可以使用.号表示多级目录，名称的最后使用@符号可以表示真实的表名，这样可以同模型多表，比如: msgs@msgs_1
		use = function(self, name, db)
			local truename = string.plainfind(name, '@')
			if truename then
				--将真实表名和模型名分离
				truename, name = name:sub(truename + 1), name:sub(1, truename - 1)
			end

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

				local err = parseFields(m)
				if err ~= true then
					error(string.format("mysql:use('%s') parse failed: %s", name, err))
					return nil
				end

				models[idxName] = m
			end
			
			local tbname = string.plainfind(name, '.')
			if tbname then
				name = name:sub(tbname + 1)
			end

			local r = setmetatable({
				__reeme = reeme,
				__dbtype = 'mysql',
				__builder = builder,
				__name = truename or name,
				__fields = m.__fields,
				__fieldsPlain = m.__fieldsPlain,
				__fieldIndices = m.__fieldIndices,
				__db = self.__defdb
			}, modelmeta)

			if db then
				r.__db = type(db) == 'string' and reeme(db) or db
			end

			return r
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
				m.__fields, m.__fieldPlain, m.__fieldIndices = nil, nil, nil
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
		
		--事务
		begin = function(self, db)
			if db then
				db:query('SET AUTOCOMMIT=0')
				db:query('BEGIN')
			end
			return self
		end,
		commit = function(self, db)
			if db then
				db:query('COMMIT')
			end
			return self
		end,
		rollback = function(self, db)
			if db then
				db:query('ROLLBACK')
			end
			return self
		end,
		
		--使用回调的方式执行事务，当事务函数返回true时就会提交，否则就会回滚
		autocommit = function(self, db, func)
			if db and func then
				db:query('BEGIN')
				if func() == true then
					db:query('COMMIT')
					return true
				else
					db:query('ROLLBACK')
				end
			end

			return false
		end,
	},
	
	__call = function(self, p1)
		return self:use(p1)
	end
}

return function(reeme)
	return setmetatable({ R = reeme }, mysql)
end