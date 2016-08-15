local index = {
	__index = {
		index = function(self)			
			local m = self.odm:use('testTable')
--[[
			local q = m:new({f = 'sdfljsdfksdf'})
			local r = q:insertInto()
			ngx.say(r.rows, r.insertid, '<br/>')
]]
			local r = m:find()
			if r then
				for k,v in pairs(r) do
					ngx.say(k, '=', tostring(v))
				end
				ngx.say('<br/>', #r)
			end
		end
	}
}

return function(act)
	local c = { }
	return setmetatable(c, index)
end