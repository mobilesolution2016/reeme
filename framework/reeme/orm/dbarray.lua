local dbArrayMeta = {
	__index = {
		from = function(self, reeme, name, offset)
			local s = #self + 1
			
			for i = offset or 1, 99 do			
				local db = reeme(name, i)
				logger.d(i, tostring(db))
				if not db then
					break
				end
				
				rawset(self, s, db)
				s = s + 1
				i = i + 1
			end
			
			return self
		end,
	}
}

return dbArrayMeta