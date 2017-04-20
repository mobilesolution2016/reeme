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
			ngx.say(os.time())
		end,
		
		templateAction = function(self)
			return self.response:view('schooldetail')
		end,
	}
}

return function(act)
	return IndexController
end