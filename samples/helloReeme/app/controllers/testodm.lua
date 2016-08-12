local index = {
	__index = {
		index = function(self)
			local m = self.odm:use('testTable')
			local q = m:new()
			q:insertInto()
		end
	}
}

return function(act)
	local c = { }
	return setmetatable(c, index)
end