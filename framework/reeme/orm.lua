local models = {}
local validTypes = { s = 1, i = 2, n = 3, b = 4, d = 5, t = 6 }
local validIndex = { primary = 1, unique = 2, index = 3 }
local modelmeta = require('reeme.orm.model')

local parseFields = function(m)
	local aiExists = false
	local fields, plains, indices = {}, {}, {}
	
	for k,v in pairs(m.fields) do
		if type(k) == 'string' then
			local first = v:byte()
			local isai, allownull = false, false
			
			if first == 35 then --#
				v = v:sub(2)
				allownull = true
			elseif first == 42 then --*
				v = v:sub(2)
				if aiExists then
					return false
				end
				
				aiExists = true
				isai = true
			end
			
			local maxl = v:match('((%d+))')
			if maxl then
				local t = validTypes[v:sub(1, 1)]
				if not t then
					return false
				end
				
				local defv = v:find(')')
				if defv and #v > defv then
					defv = v:sub(defv + 1)
				else
					defv = nil
				end

				local newf = { maxlen = tonumber(maxl), ai = isai, null = allownull, type = t, default = defv, colname = k }
				
				--date/datetime强制转为string型，然后再打标记
				if t == 5 then
					newf.type = 1
					newf.maxlen = 10
					newf.isDate = true
				elseif t == 6 then
					newf.type = 1
					newf.maxlen = 19
					newf.isDate, newf.isDateTime = true, true
				end

				fields[k] = newf
				plains[#plains + 1] = k
			else
				return false
			end
		end
	end
	
	if m.indices then
		for k,v in pairs(m.indices) do
			local tp = validIndex[v]
			if tp ~= nil then
				local cfg = fields[k]				
				local idx = { type = tp }
				
				if cfg and cfg.ai then
					idx.autoInc = true
				end
				
				indices[k] = idx
			end
		end
	end
	
	if #plains > 0 then
		m.__fields = fields
		m.__fieldsPlain = plains
		m.__fieldIndices = indices
		return true
	end
	
	return false
end

local ORM = {
	__index = {
		--使用一个定义的模型
		--不能使用require直接引用一个模型定义的Lua文件来进行使用，必须通过本函数来引用
		use = function(self, name)
			local idxName = string.format('%s-%s', ngx.var.APP_NAME or ngx.var.APP_ROOT, name)
			local m = models[idxName]
			local reeme = self.R
			
			if not m then			
				local cfgs = reeme:getConfigs()
				
				m = require(string.format('%s.%s.%s', cfgs.dirs.appBaseDir, cfgs.dirs.modelsDir, name))
				if type(m) ~= 'table' or type(m.fields) ~= 'table' then
					error(string.format("use('%s') get a invalid model declaration", name))
					return nil
				end
				
				local oldm = getmetatable(m)
				if oldm and type(oldm.__index) ~= 'table' then
					error(string.format("use('%s') get a model table, but the __index of its metatable not a table", name))
					return nil
				end
				
				m.__name = name
				m.__oldm = oldm
				if not parseFields(m) then
					error(string.format("use('%s') parse failed: may be no valid field or field(s) declaration error", name))
					return nil
				end

				setmetatable(oldm and oldm.__index or m, modelmeta)
				models[idxName] = m
			end
			
			m.__reeme = reeme
			return m
		end,
		
		uses = function(self, names)
			local tp = type(names)
			
			if tp == 'table' then
				local r = table.new(0, #names)
				for i = 1, #names do
					local n = names[i]
					r[n] = self:use(n)
				end
				return r
			elseif tp == 'string' then
				local r = table.new(0, 10)
				string.split(names, ',', string.SPLIT_ASKEY, r)
				
				for k,v in pairs(r) do
					r[k] = self:use(k)
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
				
				m.__name, m.__fields, m.__fieldPlain, m.__oldm = nil, nil, nil, nil
			end
			
			models = {}
		end,
		
		--重新加载指定的Model
		reload = function(self, name)
			local idxName = string.format('%s-%s', ngx.var.APP_NAME or ngx.var.APP_ROOT, name)
			if models[idxName] then
				models[idxName] = nil
				return self:use(idxName)
			end
		end,
	},
	
	__call = function(self, p1)
		return self:use(p1)
	end
}

local cLib = findmetatable('REEME_C_EXTLIB')
ORM.__index.parseExpression = cLib.sql_expression_parse

return function(reeme)
	local orm = { R = reeme }
	
	setmetatable(orm, ORM)
	rawset(reeme, 'orm', orm)

	return orm
end