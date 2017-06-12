local openidLib = ffi.load(ngx.var.APP_ROOT .. '/app/openid' .. (not ffi.abi('win') and '.so' or ''))
ffi.cdef([[
	typedef struct
	{
		uint64_t		uid;
		uint64_t		aid;
		uint32_t		strleng;
		char			str[64];
	} OpenIdOutput;

	size_t encOpenId(unsigned char* openid, uint64_t uid, uint64_t aid, const char* str, size_t strleng);
	bool decOpenId(const unsigned char* openid, OpenIdOutput* outid);
]])

local encodedOpenID = ffi.new('char[?]', 128)
local decodedOpenId = ffi.new('OpenIdOutput')

local encOpenId = function(uid, aid, token)
	local len

	if type(uid) == 'string' then uid = tonumber(uid) end
	if type(aid) == 'string' then aid = tonumber(aid) end

	if token then
		len = openidLib.encOpenId(encodedOpenID, uid, aid, token, #token)
	else
		len = openidLib.encOpenId(encodedOpenID, uid, aid, nil, 0)
	end
	return ffi.string(encodedOpenID, len)
end
local decOpenId = function(openid)
	if openidLib.decOpenId(openid, decodedOpenId) then
		local a, b = tonumber(decodedOpenId.uid), tonumber(decodedOpenId.aid)
		if decodedOpenId.strleng > 0 then
			return a, b, ffi.string(decodedOpenId.str, decodedOpenId.strleng)
		else
			return a, b
		end
	end
end


local TestController = {
	__index = {
		indexAction = function(self)
			--[[local f = io.open('E:\\test\\testcimg\\3.jpg', 'rb')
			local img = f:read("*all")
			f:close()
			
			local posts = {
				requests = {
					{
						image = {
							content = ngx.encode_base64(img)
						},
						features = {
							{
								type = "TEXT_DETECTION",
								maxResults = 1
							}
						}
					}
				}
			}
			
			posts = string.json(posts, string.JSON_UNICODES + string.JSON_SIMPLE_ESCAPE)
			f = io.open('E:\\1.json', 'wb')
			f:write(posts)
			f:close()]]
			
			--local r, e = self.utils.http('https://vision.googleapis.com/v1/images:annotate?key=AIzaSyCbCADfyp4O66MmGHcxxPpW-oAJzUdEe0k', posts, nil, { ['Content-Type'] = 'application/json' })
			--return r or tostring(e)
			
			local fp = io.open("d:/areas.csv", "rb")
			if fp then
				local txt = fp:read('*all')
				local lines = txt:split('\n')
				local mainCity = { ['北京市'] = 1, ['上海市'] = 2, ['天津市'] = 3, ['重庆市'] = 4 }
				local areas, fastIndices = table.new(0, 100), table.new(0, 1000)
				
				local out = io.open('d:/sql.txt', "wb")

				for i = 2, #lines do
					local line = lines[i]:split(',')
					local id, code, name, parentId,  first = table.unpack(line)
					
					id = tonumber(id)
					parentId = tonumber(parentId)
					name = name:sub(2, -2)
					first = first:sub(2, 2)

					local one = { name = name, first = first }
					fastIndices[id] = one

					if parentId ~= 0 then
						local parent = fastIndices[parentId]
						if parent then
							if not parent.child then
								parent.child = { one }
							else
								parent.child[#parent.child + 1] = one
							end
						else
							error(string.format('%s with parentid=%u not found', name, parentId))
						end
					else
						areas[#areas + 1] = one
					end
				end
				
				local function recursion(area, baseName, orderIndex, baseId, level)
					if mainCity[area.name] then
						area.fullname = area.name
					else
						area.fullname = baseName .. area.name
					end
					area.id = bit.bor(bit.lshift(orderIndex, (4 - level) * 8), baseId)
					
					out:write(string.format("INSERT INTO areas(areaid,name,fullname,firstletter,level) VALUES(%u,'%s','%s','%s',%u);\n", area.id, area.name, area.fullname, area.first, level))
					
					if area.child then
						for i = 1, #area.child do
							recursion(area.child[i], area.fullname, i, area.id, level + 1)
						end
					end
				end
				
				for i = 1, #areas do
					recursion(areas[i], '', i, 0, 1)
				end
				
				out:close()
				fp:close()
			end
	
			return 'ok'
		end,
		
		schoolsAction = function(self)
			--[[local mysql = require "resty.mysql"
			local db, err = mysql:new()
			
			db:set_timeout(30000)
			local ok, err, errcode, sqlstate = db:connect(    {
				host = '127.0.0.1',
				port = 3306,
				database = "lm",
				user = "root",
				password = "",
				pool = 'longmen',
				max_packet_size = 1024 * 1024
			})
			if not ok then
				error(string.format("failed to connect mysql: %s\n<br/>errcode=%s, state=%s", err, errcode, sqlstate))
			end
			
			db:query("SET character_set_connection=utf8,character_set_results=utf8,character_set_client=binary")
			
			local res, err, code = db:query("SELECT areaid,name,fullname,level FROM areas ORDER BY areaid ASC")
			if not res then
				error(err)
			end
			
			local currentPriv, currentCity
			local provinces = table.new(32, 0)
			local nameFrom1, nameTo = { '省', '市' }, { '', '' }
			local specNames = { ['内蒙古自治区'] = '内蒙古', ['广西壮族自治区'] = '广西', ['西藏自治区'] = '西藏', ['宁夏回族自治区'] = '宁夏', ['新疆维吾尔自治区'] = '新疆' }

			for i = 1, #res do
				local area = res[i]
				if bit.band(area.areaid, 0x00FFFFFF) == 0 then
					--省
					currentPriv = area
					currentPriv.cities = table.new(6, 0)
					
					provinces[area.name] = currentPriv
					
					local shortName = specNames[area.name]
					if not shortName then
						shortName = area.name:replace(nameFrom1, nameTo)
					end
					
					if shortName ~= area.name then
						provinces[shortName] = currentPriv
					end
					
				elseif bit.band(area.areaid, 0xFFFF0000) ~= 0 and bit.band(area.areaid, 0x0000FFFF) == 0 then
					--市
					currentCity = area
					currentCity.counties = table.new(6, 0)
					
					currentPriv.cities[area.name] = currentCity
					
				elseif bit.band(area.areaid, 0xFFFFFF00) ~= 0 and bit.band(area.areaid, 0x000000FF) == 0 then
					--区
					currentCity.counties[area.name] = area
				end
			end		
		
			local cExtLib = findmetatable('REEME_C_EXTLIB')
			local readOne = function(line)
				local types = 0
				local areaid = 0
				local tokens = cExtLib.sql_token_parse(line)
				
				if tokens[12] == "'初中'" then
					types = 2
				elseif tokens[12] == "'高中'" then
					types = 4
				end
				
				local name, addr, province = tokens[6], tokens[7], tokens[13]
				
				if province then
					province = provinces[ province:sub(2, -2) ]
					if not province then
						error('省未找到：' .. line)
					end
				else
					for privName, priv in pairs(provinces) do
						local privNameEnd = name:find(privName, 1, true)
						if privNameEnd then
							local cityNameEnd = name:find('市')
							if cityNameEnd then
								name:sub(privNameEnd, cityNameEnd)
								break
							end
						end
					end
				end

				return string.format("INSERT INTO schools2(fullname,postcode,main_phone,website_url,type_levels,register_areaid) VALUES(%s,%s,%s,%s,%d,%u)", name, tokens[10]:sub(2, -2), tokens[8], tokens[11], types, areaid)
			end]]
			
			local fp = io.open("e:/school.sql", "rb")
			local outfp = io.open('d:/schools.sql', 'wt')
			if fp then
				local txt = fp:read('*all')
				local lines = txt:split('\n')
				
				for i = 1, #lines do
					local line = lines[i]
					if not string.find(line, '幼儿园') and not string.find(line, "小学'") then
						outfp:write(line)
						outfp:write('\n')
					end
				end
			end
			outfp:close()
		end,
		
		
		ocrAction = function(self)
			local function requestOCR(self, action, get, post)
				local svr = {
					key = 'i78Uskkv02884iFHssh9284',
					--ip = '192.168.70.30',
					ip = '192.168.137.128',
					port = 5800,
				}
				local data = { method = 'GET' }		
				local url = string.format('http://%s:%u/%s?key=%s', svr.ip, svr.port, action, svr.key)
				
				if get then
					local urls
					if type(get) == 'table' then
						urls = { url, '&' }
						for k,v in pairs(get) do
							local i = #urls
							urls[i + 1] = k
							urls[i + 2] = '='
							urls[i + 3] = ngx.escape_uri(v)							
						end							
					else
						urls = { url, '&', get }
					end

					url = table.concat(urls, '')
				end
				
				if post then
					data.method = 'POST'
					if type(post) == 'string' then
						data.body = post
					elseif type(post) == 'table' then
						data.body = string.json(post, string.JSON_UNICODES)
					end
				end
				
				ngx.say('Request send')
				local res, err = require("resty.http").new():request_uri(url, data, true)
				if res and not err then
					res.status = tonumber(res.status)
					ngx.say('Status:', res.status)
					return res
				end
				
				ngx.say('Error:', err)
				return false, err
			end

			requestOCR(self, 'recog.blankpaper', { uploadid = 9913 }, {
				{
					id = 1,
					path = '/home/lyz/Downloads/2.jpg'
				}
			})
		end,
	}
}

return function(act)
	return TestController
end