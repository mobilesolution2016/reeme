local index = {
	__index = {
		index = function(self)			
			local m = self.odm:use('testTable')
--[[
			local q = m:new()
			q({f = '23424ADFSDCXVSDF@#4@#$#@$@#$@#$', b = 'test123'})
			local r = q:insertInto()
			ngx.say(r.rows, r.insertid, '<br/>')
]]

			local r = m:query():where('a=1'):andWhere('d=0'):limit(1):order('a'):exec()
			if r then
				while r:nextRow() do
					for k,v in pairs(r) do
						ngx.say(k, '=', tostring(v))
					end
					ngx.say('<br/>')
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