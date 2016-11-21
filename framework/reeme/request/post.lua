--from reqargs
local upload  = require "resty.upload"
local tmpname = os.tmpname
local concat  = table.concat
local type    = type
local find    = string.find
local open    = io.open
local sub     = string.sub
local ngx     = ngx
local req     = ngx.req
local var     = ngx.var
local body    = req.read_body
local data    = req.get_body_data
local pargs   = req.get_post_args
local fd 	  = require('reeme.fd')()

local function rightmost(s, sep)
    local p = 1
    local i = find(s, sep, 1, true)
    while i do
        p = i + 1
        i = find(s, sep, p, true)
    end
    if p > 1 then
        s = sub(s, p)
    end
    return s
end

local function basename(s)
    return rightmost(rightmost(s, "\\"), "/")
end

local function kv(r, s)
    if s == "formdata" then return end
    local e = find(s, "=", 1, true)
    if e then
        r[sub(s, 2, e - 1)] = sub(s, e + 2, #s - 1)
    else
        r[#r+1] = s
    end
end

local function parse(s)
    if not s then return nil end
    local r = {}
    local i = 1
    local b = find(s, ";", 1, true)
    while b do
        local p = sub(s, i, b - 1)
        kv(r, p)
        i = b + 1
        b = find(s, ";", i, true)
    end
    local p = sub(s, i)
    if p ~= "" then kv(r, p) end
    return r
end

local function getTempFileName()
	local ffi = require("ffi")
	local tname = os.tmpname()

	local fpos = string.rfindchar(tname, '/')
	if not fpos then
		fpos = string.rfindchar(tname, '\\')
		if not fpos then
			fpos = #tname
		end
	end

	if fd.pathIsDir(string.sub(tname, 1, fpos)) then
		return tname
	end

	if ffi.abi('win') then
		ffi.cdef[[
			unsigned long GetTempPathA(unsigned long nBufferLength, char* lpBuffer);
		]]

		local tempPathLengh = 256
		local tempPathBuffer = ffi.new("char[?]", tempPathLengh)
		ffi.C.GetTempPathA(tempPathLengh, tempPathBuffer)
		return ffi.string(tempPathBuffer) or "d:/"
	end

	assert(0, 'cannot get temp-filename')
	return ''
end
	
local function getPostArgsAndFiles(options)
    local post = { }
    local files = { }
	local bodydata = nil
	local request_method = ngx.req.get_method()
	
    local ct = var.content_type
	if not options then options = { } end
    if ct == nil then return post, files end
	
	function _move(self, dstFilename)
		if fd.pathIsFile(dstFilename) and not fd.deleteFile(dstFilename) then
			return false
		end		
		return os.rename(self.temp, dstFilename)
	end
								
    if sub(ct, 1, 19) == "multipart/form-data" then
        local chunk = options.chunk_size or 8192
        local form, e = upload:new(chunk)
        if not form then return nil, e end
        local h, p, f, o
        form:set_timeout(options.timeout or 1000)
        while true do
            local t, r, e = form:read()
            if not t then return nil, e end
            if t == "header" then
                if not h then h = {} end
                if type(r) == "table" then
                    local k, v = r[1], parse(r[2])
                    if v then h[k] = v end
                end
            elseif t == "body" then
                if h then
                    local d = h["Content-Disposition"]
                    if d then
                        if d.filename then
                            f = {
                                name = d.name,
                                type = h["Content-Type"] and h["Content-Type"][1],
                                file = basename(d.filename),
                                temp = getTempFileName(),
								moveFile = _move,
                            }
                            o, e = open(f.temp, "wb")
                            if not o then return nil, e end
                            o:setvbuf("full", chunk)
                        else
                            p = { name = d.name, data = { n = 1 } }
                        end
                    end
                    h = nil
                end
                if o then
                    local ok, e = o:write(r)
                    if not ok then return nil, e end
                elseif p then
                    local n = p.data.n
                    p.data[n] = r
                    p.data.n = n + 1
                end
            elseif t == "part_end" then
                if o then
                    f.size = o:seek()
                    o:close()
                    o = nil
                end
                local c, d
                if f then
                    c, d, f = files, f, nil
                elseif p then
                    c, d, p = post, p, nil
                end
                if c then
                    local n = d.name
                    local s = d.data and concat(d.data) or d
                    if n then
                        local z = c[n]
                        if z then
                            if z.n then
                                z.n = z.n + 1
                                z[z.n] = s
                            else
                                z = { z, s }
                                z.n = 2
                            end
                            c[n] = z
                        else
                            c[n] = s
                        end
                    else
                        c.n = c.n + 1
                        c[c.n] = s
                    end
                end
            elseif t == "eof" then
                break
            end
        end
        local t, r, e = form:read()
        if not t then return nil, e end
    elseif sub(ct, 1, 16) == "application/json" then
        body()
        post = string.json(data()) or {}
    else
        body()
        post = pargs()
		if request_method == 'POST' then
			bodydata = data()
		end
    end
    return post, files, bodydata
end

return function(reeme)
	local postArgs, files, body = getPostArgsAndFiles()
	local post = { __R = reeme, __post = postArgs, files = files or { }, body = body }
	
	return setmetatable(post, { 
		__index = postArgs or { },
	})
end