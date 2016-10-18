local sqlwords = nil

local rawsqlMeta = {
	__index = {
		func = function(self, name)
			local tp = sqlwords[name]
			assert(tp == 1, string.format("name '%s' for rawsql.func not a function"))
			if tp == 1 then
				self[0] = name
			end
			return self
		end,		
	}
}

return function(words)
	if words then
		sqlwords = words
	end
	
	return rawsqlMeta
end