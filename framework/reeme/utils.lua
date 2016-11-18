local ffi = require 'ffi'

local socklib = nil
if ffi.abi('win') then
	ffi.cdef([[
		struct hostent {
			char  *h_name;            /* official name of host */
			char **h_aliases;         /* alias list */
			short  h_addrtype;        /* host address type */
			short  h_length;          /* length of address */
			char **h_addr_list;       /* list of addresses */
		};

		struct hostent *gethostbyname(const char *name);
		struct myaddr { uint8_t b1, b2, b3, b4; };
	]])

	socklib = ffi.load('Ws2_32.dll')
else
	ffi.cdef([[
		struct hostent {
			char  *h_name;            /* official name of host */
			char **h_aliases;         /* alias list */
			int    h_addrtype;        /* host address type */
			int    h_length;          /* length of address */
			char **h_addr_list;       /* list of addresses */
		};

		struct hostent *gethostbyname(const char *name);
		struct myaddr { uint8_t b1, b2, b3, b4; };
	]])

	socklib = ffi.C
end

ffi.cdef[[
typedef unsigned char u_char;
u_char * ngx_hex_dump(u_char *dst, const u_char *src, size_t len);
]]


function resolveHost(name, returnFirst)
	if type(name) ~= 'string' then
		return
	end

	local hostent = socklib.gethostbyname(name)
	if hostent == nil then
		return
	end

	local total = hostent.h_length / 4
	if total > 0 then
		local i, hosts = 0, {}
		while i < total do
			local myaddr = ffi.cast('struct myaddr*', hostent.h_addr_list[i])

			i = i + 1
			myaddr = string.format('%d.%d.%d.%d', myaddr.b1, myaddr.b2, myaddr.b3, myaddr.b4)
			if returnFirst then
				return myaddr
			end

			hosts[i] = myaddr
		end

		return hosts
	end
end


local staticHosts = { 
	['127.0.0.1'] = '127.0.0.1', 
	localhost = '127.0.0.1' 
}
--常见文件扩展名到mimetype的映射表
local mimeTypesMap = {
	['323'] = 'text/h323', acx = 'application/internet-property-stream', ai = 'application/postscript', aif = 'audio/x-aiff', aifc = 'audio/x-aiff', aiff = 'audio/x-aiff',
	asf = 'video/x-ms-asf', asr = 'video/x-ms-asf', asx = 'video/x-ms-asf', au = 'audio/basic', avi = 'video/x-msvideo', axs = 'application/olescript', bas = 'text/plain',
	bcpio = 'application/x-bcpio', bin = 'application/octet-stream', bmp = 'image/bmp', c = 'text/plain', cat = 'application/vnd.ms-pkiseccat', cdf = 'application/x-cdf',
	cdf = 'application/x-netcdf', cer = 'application/x-x509-ca-cert', class = 'application/octet-stream', clp = 'application/x-msclip', cmx = 'image/x-cmx', cod = 'image/cis-cod',
	cpio = 'application/x-cpio', crd = 'application/x-mscardfile', crl = 'application/pkix-crl', crt = 'application/x-x509-ca-cert', csh = 'application/x-csh', css = 'text/css',
	dcr = 'application/x-director', der = 'application/x-x509-ca-cert', dir = 'application/x-director', dll = 'application/x-msdownload', dms = 'application/octet-stream',
	doc = 'application/msword', dot = 'application/msword', dvi = 'application/x-dvi', dxr = 'application/x-director', eps = 'application/postscript', etx = 'text/x-setext',
	evy = 'application/envoy', exe = 'application/octet-stream', fif = 'application/fractals', flr = 'x-world/x-vrml', gif = 'image/gif', gtar = 'application/x-gtar', gz = 'application/x-gzip',
	h = 'text/plain', hdf = 'application/x-hdf', hlp = 'application/winhlp', hqx = 'application/mac-binhex40', hta = 'application/hta', htc = 'text/x-component', htm = 'text/html',
	html = 'text/html', htt = 'text/webviewhtml', ico = 'image/x-icon', ief = 'image/ief', iii = 'application/x-iphone', ins = 'application/x-internet-signup', isp = 'application/x-internet-signup',
	jfif = 'image/pipeg', jpe = 'image/jpeg', jpeg = 'image/jpeg', jpg = 'image/jpeg', js = 'application/x-javascript', latex = 'application/x-latex', lha = 'application/octet-stream',
	lsf = 'video/x-la-asf', lsx = 'video/x-la-asf', lzh = 'application/octet-stream', m13 = 'application/x-msmediaview', m14 = 'application/x-msmediaview', m3u = 'audio/x-mpegurl',
	man = 'application/x-troff-man', mdb = 'application/x-msaccess', me = 'application/x-troff-me', mht = 'message/rfc822', mhtml = 'message/rfc822', mid = 'audio/mid', mny = 'application/x-msmoney',
	mov = 'video/quicktime', movie = 'video/x-sgi-movie', mp2 = 'video/mpeg', mp3 = 'audio/mpeg', mpa = 'video/mpeg', mpe = 'video/mpeg', mpeg = 'video/mpeg', mpg = 'video/mpeg',
	mpp = 'application/vnd.ms-project', mpv2 = 'video/mpeg', ms = 'application/x-troff-ms', msg = 'application/vnd.ms-outlook', mvb = 'application/x-msmediaview', nc = 'application/x-netcdf',
	nws = 'message/rfc822', oda = 'application/oda', p10 = 'application/pkcs10', p12 = 'application/x-pkcs12', p7b = 'application/x-pkcs7-certificates', p7c = 'application/x-pkcs7-mime',
	p7m = 'application/x-pkcs7-mime', p7r = 'application/x-pkcs7-certreqresp', p7s = 'application/x-pkcs7-signature', pbm = 'image/x-portable-bitmap', pdf = 'application/pdf',
	pfx = 'application/x-pkcs12', pgm = 'image/x-portable-graymap', pko = 'application/ynd.ms-pkipko', pma = 'application/x-perfmon', pmc = 'application/x-perfmon', pml = 'application/x-perfmon',
	pmr = 'application/x-perfmon', pmw = 'application/x-perfmon', pnm = 'image/x-portable-anymap', pot = 'application/vnd.ms-powerpoint', ppm = 'image/x-portable-pixmap', pps = 'application/vnd.ms-powerpoint',
	ppt = 'application/vnd.ms-powerpoint', prf = 'application/pics-rules', ps = 'application/postscript', pub = 'application/x-mspublisher', qt = 'video/quicktime', ra = 'audio/x-pn-realaudio',
	ram = 'audio/x-pn-realaudio', ras = 'image/x-cmu-raster', rgb = 'image/x-rgb', rmi = 'audio/mid', roff = 'application/x-troff', rtf = 'application/rtf', rtx = 'text/richtext', scd = 'application/x-msschedule',
	sct = 'text/scriptlet', setpay = 'application/set-payment-initiation', setreg = 'application/set-registration-initiation', sh = 'application/x-sh', shar = 'application/x-shar', sit = 'application/x-stuffit',
	snd = 'audio/basic', spc = 'application/x-pkcs7-certificates', spl = 'application/futuresplash', src = 'application/x-wais-source', sst = 'application/vnd.ms-pkicertstore', stl = 'application/vnd.ms-pkistl',
	stm = 'text/html', sv4cpio = 'application/x-sv4cpio', sv4crc = 'application/x-sv4crc', svg = 'image/svg+xml', swf = 'application/x-shockwave-flash', t = 'application/x-troff', tar = 'application/x-tar',
	tcl = 'application/x-tcl', tex = 'application/x-tex', texi = 'application/x-texinfo', texinfo = 'application/x-texinfo', tgz = 'application/x-compressed', tif = 'image/tiff', tiff = 'image/tiff',
	tr = 'application/x-troff', trm = 'application/x-msterminal', tsv = 'text/tab-separated-values', txt = 'text/plain', uls = 'text/iuls', ustar = 'application/x-ustar', vcf = 'text/x-vcard', vrml = 'x-world/x-vrml',
	wav = 'audio/x-wav', wcm = 'application/vnd.ms-works', wdb = 'application/vnd.ms-works', wks = 'application/vnd.ms-works', wmf = 'application/x-msmetafile', wps = 'application/vnd.ms-works',
	wri = 'application/x-mswrite', wrl = 'x-world/x-vrml', wrz = 'x-world/x-vrml', xaf = 'x-world/x-vrml', xbm = 'image/x-xbitmap', xla = 'application/vnd.ms-excel', xlc = 'application/vnd.ms-excel',
	xlm = 'application/vnd.ms-excel', xls = 'application/vnd.ms-excel', xlt = 'application/vnd.ms-excel', xlw = 'application/vnd.ms-excel', xof = 'x-world/x-vrml', xpm = 'image/x-xpixmap',
	xwd = 'image/x-xwindowdump', z = 'application/x-compress', zip = 'application/zip', rar = 'application/rar',
}


--本库所有方法都是静态调用的，不要用:来调用
local Utils = {
	--en/decode
	hex = function(str)
		if not str or type(str) ~= "string" then return nil end

		local len = #str * 2
		local buf = ffi.new(ffi.typeof("uint8_t[?]"), len)
		ffi.C.ngx_hex_dump(buf, str, #str)
		return ffi.string(buf, len)
	end,

	--使用系统内置函数解析域名为IP地址
	resolveHost = resolveHost,

	--将URL中的域名部分解析成IP地址然后返回完整的URL
	resolveUrl = function(url)
		local protocol = string.find(url, ':', 1, true)
		if not protocol then
			return false
		end

		local port = 80
		local pathbegin = string.find(url, '/', protocol + 3, true)
		local host, path = string.sub(url, protocol + 3, pathbegin and pathbegin - 1 or #url), pathbegin and string.sub(url, pathbegin) or '/'

		host, port = string.cut(host, ':')
		host = staticHosts[host] or resolveHost(host, true)

		if port and port ~= '80' then
			host = host .. ':' .. port
		end

		return string.format('%s//%s%s', string.sub(url, 1, protocol), host, path)
	end,

	--HTTP上传
	http = function(url, posts, method, headers)
		local data = {
			ssl_verify = false,
	        method = method or 'GET',
	        headers = 0
	    }
		
		if type(headers) == 'table' then
			data.header = table.new(0, 6)
			data.header['Content-Type'] = 'text/html; charset=utf-8'
			for k,v in pairs(headers) do
				data.headers[k] = v
			end
		else
			data.headers = table.new(0, 2)
			data.headers['Content-Type'] = 'text/html; charset=utf-8'
		end

		if posts then
			data.method = 'POST'
			data.body = posts
		end

		local res, err = require("resty.http").new():request_uri(url, data)
		if res and not err then
			return res
		end

		return false, err
	end,
	
	--http上传文件. posts = { afile = { filename = 'abc.xxx', mimetype = 'xxxx', handle = io.open() }, a_string_value = 'xxxxxxx' }，其中filename和mimetype都是可选的，filename没有的时候将使用name代替。可以文件和普通字符串混合一起传
	httpUpload = function(url, posts, headers)
		if type(posts) ~= 'table' then
			assert(nil, 'posts with utils.httpUpload function call must be a table')
			return 
		end
		
		local data = {
			ssl_verify = false,
	        method = 'POST',
	        headers = 0
	    }

		--fixed boundary string(60 bytes)
		local boundary = "------Resty-ReemeFramework-Boundary-A3c8E1f0z5P7s6J9w4D0lxD-"
		local contentType = 'multipart/form-data; boundary=' .. boundary
		local contentLen, parts, cc = 0, table.new(4, 0), 1
		
		if type(headers) == 'table' then
			data.header = table.new(0, 6)
			data.header['Content-Type'] = contentType
			for k,v in pairs(headers) do
				data.headers[k] = v
			end
		else
			data.headers = table.new(0, 2)
			data.headers['Content-Type'] = contentType
		end
		
		--加载文件并格式化数据
		for name, value in pairs(posts) do
			if type(value) == 'table' then
				local mimeType = 'application/octet-stream'				
				if value.mimetype then
					mimeType = value.mimetype
					
				elseif value.fileext then
					mimeType = mimeTypesMap[string.lower(value.fileext)]
					
				elseif value.filename then
					local fext = string.rfindchar(value.filename, '.')
					if fext then
						fext = string.sub(value.filename, fext + 1)
						if #fext > 0 then
							mimeType = mimeTypesMap[string.lower(fext)]
						end
					end
				end
			
				parts[cc] = string.format('%s--%s\r\nContent-Disposition: form-data; name="%s"; filename="%s"\r\nContent-Type: %s\r\n\r\n', cc > 1 and '\r\n' or '', boundary, name, value.filename or name, mimeType)
				parts[cc + 1] = value.handle:read()
				value.handle:close()								
			else
				parts[cc] = string.format('%s--%s\r\nContent-Disposition: form-data; name="%s"\r\n\r\n', cc > 1 and '\r\n' or '', boundary, name)
				parts[cc + 1] = tostring(value)
			end
			
			cc = cc + 2
		end

		if cc > 0 then
			parts[cc] = string.format('\r\n--%s--', boundary)
		end

		--将所有数据合并成一个大字符串
		data.body = table.concat(parts, '')
		data.headers['Content-Length'] = #data.body

		local res, err = require("resty.http").new():request_uri(url, data)

		data.body, data.headers = nil, nil
		if res and not err then
			return res
		end

		return false, err
	end,
}

return Utils
