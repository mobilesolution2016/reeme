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
	

local staticHosts = { ['127.0.0.1'] = '127.0.0.1', localhost = '127.0.0.1' }
local Utils = {
	--en/decode
	hex = function(str)
		if not str or type(str) ~= "string" then return nil end
		
		local len = #str * 2
		local buf = ffi.new(ffi.typeof("uint8_t[?]"), len)
		ffi.C.ngx_hex_dump(buf, str, #str)
		return ffi.string(buf, len)
	end,
	
	--network
	resolveHost = resolveHost,
	
	--http请求
	http = function(url, posts, method, headers)
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
		
		local newurl = string.format('%s//%s%s', string.sub(url, 1, protocol), host, path)
		local data = {
			ssl_verify = false,
	        method = method or 'GET',
	        headers = headers or { ['Content-Type'] = 'text/html; charset=utf-8' }
	    }
		
		if posts then
			data.method = 'POST'
			data.body = posts
		end

		local res, err = require("resty.http").new():request_uri(newurl, data)
		if res and not err then
			return res
		end
		
		return false, err
	end,
}

return Utils