local IndexController = {
	__index = {
		--转换地区数据
		areasAction = function(self)
			local src = 'E:/test/testcimg/8-1.jpg'
			local dst = 'E:/1.json'
			
			local output = [[{
	"requests":[
		{
			"image": {
				"content":"%s"
			},
			"features": [
				{ "type":"DOCUMENT_TEXT_DETECTION" }
			],
			"imageContext": {
				"languageHints": [ "zh-CN", "en" ]
			}
		}
	]
}]]

			local fp = io.open(src, 'rb')
			if fp then
				local srcData = fp:read('*a')
				fp:close()
				
				srcData = ngx.encode_base64(srcData)	
				
				fp = io.open(dst, 'wb')
				if fp then
					fp:write(string.format(output, srcData))
					fp:close()
				else
					return '打开输出文件失败'
				end
				
			else
				return '打开源文件失败'
			end

			return '已完成'
		end,
		
		indexAction = function(self)
			local cairo = require('reeme.cairo.cairo')
			local bitmap = require('reeme.cairo.bitmap')
			
			local bmp, ss = QRCodeEncode({ code = 'sdlfjsdkfjlksdjfl2j341823470zxcvlasdf', blocksize = 8 }, bitmap.new)
			local sr = cairo.image_surface(bmp)
			sr:save_png('d:\\1.png')
			
			ngx.say(ss)
		end,
		
		testAction = function(self)
			local j = string.json('{ "error": { "code": 400, "message": "Invalid JSON payload received. Expected , or } after key:value pair.\n\u0000\u0001\u0000\u0000\u0003\u0016 \u0000\u0005\u0000\u0000\u0000\u0001\u0000\u0000\u0003\u001e \"\u0000\u0003\u0000\u0000\u0000\u0001\u0000\u0002\u0000\u0000\u0000\u0003\u0000\u0000\u0000\u0001\u0000", "status": "INVALID_ARGUMENT" } }')
			ngx.say(string.json(j))
		end,
		
		--http://helloreeme.reeme.com/index.gettoken，获取环信的SDK Token
		gettokenAction = function(self)			
			local url = 'https://a1.easemob.com/1190170428178500/lmpc/'		
			local r = self.utils.http(url .. 'token', {
				grant_type = 'client_credentials',
				client_id = 'YXA6o6eVUCwPEee6fk03EKIFTg',
				client_secret = 'YXA6Vjtli2igL2pfrn3OlH_ZX83cJCU'
			})
			
			ngx.say(r.body)
		end,
	}
}

return function(act)
	return IndexController
end