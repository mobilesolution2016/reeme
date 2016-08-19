printRow = function(r)
	for k,v in pairs(r) do
		ngx.say(k, '=', v)
	end
	ngx.say('<br/>')
end

local index = {
	__index = {
		index = function(self)
			local m = self.orm('testTable')
			local m2 = self.orm('mytable')
			
			local v = '中华人民共和国'
			ngx.say('utf=', u8string.det3(v), '<br/>')
			ngx.say('len=', u8string.len(v), '<br/>')
			ngx.say('sub=', u8string.sub(v, 3, 3), '<br/>')
--[[
			local q = m:new()
			q({f = '23424ADFSDCXVSDF@#4@#$#@$@#$@#$', b = 'test123'})
			local r = q:insertInto()
			ngx.say(r.rows, r.insertid, '<br/>')
]]
			local r2 = m2:query():where('sex=1')
			local r = m:query()
				:expr('DISTINCT a')
				:excepts('b')
				:where({ a = { '=1' }, { "f LIKE '%nn%'" } } )
				:join(r2, 'left')
				:limit(10)
				:order('a')
				:exec()

			if r then
				r.f = 'SDLkfjKSDFJ'
				r:save()
				r:create()
				
				repeat
					printRow(r)
				until not (r + 1)
				
				r = -r
				repeat
					printRow(r)
				until not (r + 1)
			end
--[[
			local v1, v2 = '', ''
			local s1 = os.clock()
			for i = 1, 10000 do
				local f = string.template('abcdefghi{1}', 'sdlfkjsdlf', 2343, 'sdlfkjsdlf', 2343)
				v1 = v1 .. f
			end
			local e1 = os.clock()
			
			local s2 = os.clock()
			for i = 1, 10000 do
				local f = string.format('abcdefghi%s', 'sdlfkjsdlf', 2343, 'sdlfkjsdlf', 2343)
				v2 = v2 .. f
			end
			local e2 = os.clock()
			
			ngx.say('time1=', tostring(e1 - s1), ' time2=', tostring(e2 - s2))
]]
		end
	}
}

return function(act)
	local c = { }
	return setmetatable(c, index)
end