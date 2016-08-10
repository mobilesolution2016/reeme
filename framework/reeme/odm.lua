local ODM = {
	__index = {
		useModel = function(self, name)
			require('odm.model')
		end
	}
}


return function(reeme)
	local odm = { R = reeme }
	
	return setmetatable(odm, ODM)
end