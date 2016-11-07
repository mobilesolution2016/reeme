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
	resolveHost = function(name, returnFirst)
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
	end,
}

return Utils