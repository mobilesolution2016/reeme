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
			local str = ' <p class=\"bodyer_5_p_l\"><span>【试题来源】</span><a href=\"paperList.aspx?mod=paper&amp;ac=st&amp;paperID=148265&amp;rid=4414&amp;op=listsub\" target=\"_blank\" title=\"（同步测试）\">（同步测试）</a></p><p><span class=\"span\">【答案解析】</span> </p><p></p><div>\n<style>\n\np.MsoNormal, li.MsoNormal, div.MsoNormal {margin1:0cm;margin1-bottom:.0001pt;text-align:justify;text-justify:inter-ideograph;font-size:11pt;font-family:\"Times New Roman\";}\ndiv.Section1 {page:Section1;}\n</style>\n<span style=\"FONT-SIZE: 11pt; FONT-FAMILY: 宋体\">D</span> </div><p></p>  <div class=\"news-title-list\" id=\"ping_lun_list\" style=\"margin: 10px 0px 0px 0px;\"><div style=\" font-weight:bold;height:23px;color:#009900\"><a href=\"javascript:void(0);\" onclick=\"ExamanSelectAll(\'148265\'); return false;\" style=\"float:left; font-weight:normal;\">网友解析 ︾</a></div> <ul id=\"examan_div_ul_list_148265\" style=\"display:none;\">  <div style=\"font-size:12px;font-weight:normal;padding:0px 0 5px 20px;color: black;   \" id=\"\">加载中……</div>  </ul>  </div>'
			ngx.say(string.removemarkups(string.stripslashes(str)))
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