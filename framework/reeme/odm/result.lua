local execModelInstance = function(self, db, op, limit, full)
	local q = self.model:query()

	q.op = op
	q.__full = full

	if limit then 
		q:limit(1) 
	end
	return q:exec(db, self)
end

local resultMeta = {
	__index = {
		query = function(self, sql)
			local bytes, err, code = self.db:send_query(sql)
			if not bytes then
				self.lastErr = err
				self.lastErrCode = code
				return false
			end
			
			self.querySql = sql
			return true
		end,
		
		first = function(self, sql)
			local bytes, err, code = self.db:send_query(sql)
			if bytes then
				res, err, code = self.db:read_result()
				if res then
					return res
				end
			end
			
			if err and code then
				self.lastErr = err
				self.lastErrCode = code	
			end
		end,
		
		next = function(self)
			local res, err, code = self.db:read_result()
			if res then
				self.rows = res
				self.rowId = self.rowId + 1
				return true
			end
			
			if err and code then
				self.lastErr = err
				self.lastErrCode = code	
			end
			
			return false
		end,
		
		reset = function()
			if self:query(self.querySql) then
				self.rowId = 0
				return true
			end
			return false
		end,
		
		saveTo = function(self, db)
			if not db then db = self.db
			else self.db = db end
			
			return execModelInstance(self, db == nil and self.db or db, 'UPDATE', true, false)
		end,
		fullSaveTo = function(self, db)
			if not db then db = self.db
			else self.db = db end
			
			return execModelInstance(self, db == nil and self.db or db, 'UPDATE', true, true)
		end,
		insertInto = function(self, db)
			if not db then db = self.db
			else self.db = db end
			
			return execModelInstance(self, db == nil and self.db or db, 'INSERT', false, false)
		end,
		fullInsertInto = function(self, db)
			if not db then db = self.db
			else self.db = db end

			return execModelInstance(self, db == nil and self.db or db, 'INSERT', false, true)
		end,
	},
}

return function(r, d, m)
	if r == nil then
		r = setmetatable({ db = d, rowId = 0, model = m }, resultMeta)
	end
	
	return r
end