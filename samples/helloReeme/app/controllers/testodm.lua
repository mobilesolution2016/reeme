printRow = function(r)
	for k,v in pairs(r) do
		ngx.say(k, '=', v)
	end
	ngx.say('<br/>')
end

local index = {
	__index = {
		index = function(self)
			local m = self.odm('testTable')
			local m2 = self.odm('mytable')
--[[
			local q = m:new()
			q({f = '23424ADFSDCXVSDF@#4@#$#@$@#$@#$', b = 'test123'})
			local r = q:insertInto()
			ngx.say(r.rows, r.insertid, '<br/>')
]]
			local r2 = m2:query()
			local r = m:query()
				:expr('DISTINCT a')
				:where('a=1')
				:join(r2, 'left')
				:limit(10)
				:order('a')
				:exec()
			
			if r then
				while r:nextRow() do
					printRow(r)
				end
			end
		end
	}
}

return function(act)
	local c = { }
	return setmetatable(c, index)
end