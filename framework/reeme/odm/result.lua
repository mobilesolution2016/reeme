local execModelInstance = function(self, db, op, limit, full)
	local q = rawget(self, '__model'):query()

	q.op = op
	q.__full = full

	if limit then 
		q:limit(1) 
	end
	return q:exec(db, self)
end

local resultMeta = {}
resultMeta.__index = {
	query = function(self, db, sql, maxrows)
		local res, err, code = db:query(sql, maxrows)
		if res then
			rawset(self, '__totalRows', #res)
			rawset(self, '__currentRow', 0)
			rawset(self, '__allRows', res)
			return res
		end

		if err then
			error(string.format("ODM Query execute failed:<br/>&nbsp;&nbsp;Sql = %s<br/>&nbsp;&nbsp;Error = %s", sqls, err))
		end

		return false
	end,
	next = function(self)
		local curr = rawget(self, '__currentRow')
		if resultMeta.__index.gotoRowId(self, curr + 1) then
			rawset(self, '__currentRow', curr + 1)
			return true
		end
		return false
	end,
	gotoRowId = function(self, rowId)
		local total = rawget(self, '__totalRows')
		if rowId < 1 or rowId >= total then
			return false
		end
		
		local rows = rawget(self, '__allRows')
		local meta = getmetatable(self)
		
		meta.__index = rows[1]
		meta.__newindex = rows[1]
		
		setmetatable(meta.__index, resultMeta)
		setmetatable(self, meta)
		
		return true
	end,
	
	getValues = function(self)
		return getmetatable(self).__index
	end,
	
	saveTo = function(self, db)
		return execModelInstance(self, db, 'UPDATE', true, false)
	end,
	fullSaveTo = function(self, db)
		return execModelInstance(self, db, 'UPDATE', true, true)
	end,
	insertInto = function(self, db)
		return execModelInstance(self, db, 'INSERT', false, false)
	end,
	fullInsertInto = function(self, db)
		return execModelInstance(self, db, 'INSERT', false, true)
	end
}

local getRowsLen = function(self)
	return rawget(self, '__totalRows') or 0
end
local getRowPairs = function(self, _1, _2)
	local vals = getmetatable(self).__index
	return function(t, k)
		local k, v = next(vals, k)
		return k, v
	end
end

return function(r, m)
	if r == nil then
		local rVals = {}
		local valsMeta = { __index = rVals, __newindex = rVals, __len = getRowsLen, __pairs = getRowPairs }
		
		setmetatable(rVals, resultMeta)
		r = setmetatable({}, valsMeta)
	end
	
	rawset(r, '__model', m)
	return r
end