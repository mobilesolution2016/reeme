local ffi = require('ffi')
local _isFile = 0
local _isDir = 0
local _isExists = 0
local _openDir = 0
local _createDir = 0
local _getAttr = 0
local _checkAttr = 0

local dirt = {}
local mode2n = function(mode)
	if type(mode) == 'string' and (#mode == 3 or #mode == 4) then
		local s, g, o = string.byte(string.byte(mode) == 48 and string.sub(mode , 2) or mode, 1, 3)
		s = bit.lshift(s - 48, 6)
		g = bit.lshift(g - 48, 3)
		return bit.bor(bit.bor(s, g), o - 48)
	end
end

local pathsegs = table.new(6, 0)
local mergePath = function(self)
	if self.driver then pathsegs[1] = self.driver .. ':' end
	if self.path then pathsegs[#pathsegs + 1] = self.path end
	if self.name then pathsegs[#pathsegs + 1] = self.name end
	if self.ext then pathsegs[#pathsegs + 1] = self.ext end
	pathsegs[#pathsegs + 1] = nil
	return table.concat(pathsegs, '/')
end

if ffi.os == 'Windows' then
	ffi.cdef[[
		int PathFileExistsA(const char*);
		int PathIsDirectoryA(const char*);

		typedef struct _FILETIME {
			unsigned long dwLowDateTime;
			unsigned long dwHighDateTime;
		} FILETIME;

		typedef struct _WIN32_FIND_DATA {
			unsigned	dwFileAttributes;
			FILETIME	ftCreationTime;
			FILETIME	ftLastAccessTime;
			FILETIME	ftLastWriteTime;
			unsigned	nFileSizeHigh;
			unsigned	nFileSizeLow;
			unsigned	dwReserved0;
			unsigned	dwReserved1;
			char		cFileName[260];
			char		cAlternateFileName[14];
		} WIN32_FIND_DATAA;

		void* __stdcall FindFirstFileA(const char*, WIN32_FIND_DATAA*);
		int __stdcall FindNextFileA(void*, WIN32_FIND_DATAA*);
		int __stdcall FindClose(void* hFindFile);
		int __stdcall CreateDirectoryA(const char*, void*);
		unsigned __stdcall GetFileAttributesA(const char*);
	]]

	local kernel32 = ffi.load('kernel32.dll')
	local shlwapi = ffi.load('shlwapi.dll')
	
	local FILE_ATTRIBUTE_TEMPORARY = 0x100
	local FILE_ATTRIBUTE_ARCHIVE = 0x20
	local FILE_ATTRIBUTE_DIRECTORY = 0x10
	local FILE_ATTRIBUTE_HIDDEN = 0x2
	local FILE_ATTRIBUTE_READONLY = 0x1
	local FILE_ATTRIBUTE_EXECUTE = 0x80000000
	
	local executes = { exe = 1, com = 1, bat = 1 }
	
	local attrChecks = {
		file = function(val)
			return bit.band(val, FILE_ATTRIBUTE_ARCHIVE) ~= 0 and true or false
		end,
		dir = function(val)
			return bit.band(val, FILE_ATTRIBUTE_DIRECTORY) ~= 0 and true or false
		end,
		hidden = function(val)
			return bit.band(val, FILE_ATTRIBUTE_HIDDEN) ~= 0 and true or false
		end,
		link = function(val)
			return false
		end,
		temporary = function(val)
			return bit.band(val, FILE_ATTRIBUTE_TEMPORARY) ~= 0 and true or false
		end,
		socket = function(val)
			return false
		end,
	}

	_isFile = function(path)
		return shlwapi.PathFileExistsA(path) ~= 0
	end
	_isDir = function(path)
		return shlwapi.PathIsDirectoryA(path) ~= 0
	end
	_isExists = function(path)
		if shlwapi.PathFileExistsA(path) ~= 0 then
			return 1
		end
		if shlwapi.PathIsDirectoryA(path) ~= 0 then
			return 2
		end
	end
	
	_getAttr = function(path)
		local r = kernel32.GetFileAttributesA(path)
		if r ~= 0xFFFFFFFF then
			local fext = string.rfindchar(path, '.')
			if fext then
				fext = string.sub(path, fext + 1)
				if fext and executes[string.lower(fext)] then					
					r = bit.bor(r, FILE_ATTRIBUTE_EXECUTE)
				end
			end
			
			return r
		end
	end
	_checkAttr = function(path, what)
		if type(what) == 'string' then
			local val = type(path) == 'string' and kernel32.GetFileAttributesA(path) or path
			if val ~= 0xFFFFFFFF then
				local f = attrChecks[what]
				if f then return f(val) end
				
				if #what == 3 or #what == 4 then
					local r = true
					local s, g, o = string.byte(string.byte(what) == 48 and string.sub(what , 2) or what, 1, 3)

					if (bit.band(s, 2) ~= 0 or bit.band(g, 2) ~= 0 or bit.band(o, 2) ~= 0) and bit.band(val, FILE_ATTRIBUTE_READONLY) ~= 0 then
						r = false
					end
					if (bit.band(s, 1) ~= 0 or bit.band(g, 1) ~= 0 or bit.band(o, 1) ~= 0) and bit.band(val, FILE_ATTRIBUTE_EXECUTE) == 0 then
						r = false
					end
					
					return r
				end
			end
		end
	end

	_openDir = function(path, filter)
		local last = path:sub(#path)
		local r = { (last == '\\' or last == '/') and path or (path .. '/'), ffi.new('WIN32_FIND_DATAA') }

		r[3] = kernel32.FindFirstFileA(r[1] .. (filter == nil and '*.*' or filter), r[2])
		if r[3] == nil then
			r[2] = nil
			return nil
		end

		return setmetatable(r, dirt)
	end

	dirt.__index = {
		pick = function(self)
			if not self[4] or kernel32.FindNextFileA(self[3], self[2]) ~= 0 then
				self[4] = ffi.string(self[2].cFileName)
				self[5] = self[2].dwFileAttributes
				return true
			end

			return false
		end,
		close = function(self)
			if self[3] then
				kernel32.FindClose(self[3])
				self[3], self[2] = nil, nil
			end
		end,
		name = function(self)
			return self[4]
		end,
		fullname = function(self)
			return self[1] .. self[4]
		end,
		
		getAttrs = function(self)
			return self[5]
		end,
	}

	_createDir = function(path, mode)
		return kernel32.CreateDirectoryA(path, nil) ~= 0
	end

else
	ffi.cdef[[
		void* opendir(const char* path);
		int closedir(void* handle);

		const char* readdirinfo(void* handle, const char* filter);
		bool pathisfile(const char* path);
		bool pathisdir(const char* path);
		unsigned pathisexists(const char* path);
		bool createdir(const char* path, int mode);
		unsigned getpathattrs(const char* path);
	]]

	local readdirinfo = _G.libreemext.readdirinfo
	local getpathattrs = _G.libreemext.getpathattrs
	local pathisexists = _G.libreemext.pathisexists
	
	local FILE = 1
	local DIR = 2
	local LINK = 4
	local SOCKET = 8
	local HIDDEN = 16
	local O_READ, O_WRITE, O_EXEC = 256, 128, 64
	local G_READ, G_WRITE, G_EXEC = 32, 16, 8
	local R_READ, R_WRITE, R_EXEC = 4, 2, 1

	local attrChecks = {
		file = function(val)
			return bit.band(val, FILE) ~= 0 and true or false
		end,
		dir = function(val)
			return bit.band(val, DIR) ~= 0 and true or false
		end,
		hidden = function(val)
			return bit.band(val, HIDDEN) ~= 0 and true or false
		end,
		link = function(val)
			return bit.band(val, LINK) ~= 0 and true or false
		end,
		temporary = function(val)
			return false
		end,
		socket = function(val)
			return bit.band(val, SOCKET) ~= 0 and true or false
		end,
	}
	
	_isFile = _G.libreemext.pathisfile
	_isDir = _G.libreemext.pathisdir
	_isExists = function(path)
		local r = pathisexists(path)
		if r ~= 0 then
			return r
		end
	end

	_getAttr = function(path)
		return getpathattrs(path)
	end
	_checkAttr = function(path, what)
		if type(what) == 'string' then
			local val = type(path) == 'string' and getpathattrs(path) or path
			if val ~= 0 then
				local f = attrChecks[what]
				if f then  return f(val) end
				
				f = mode2n(what)
				return (f and bit.band(val, f) == f) and true or false
			end
		end
	end

	_openDir = function(path, filter)
		local handle = ffi.C.opendir(path)
		if handle == nil then
			return nil
		end

		local r = table.new(4, 0)
		local last = path:sub(#path)

		if last == '\\' then
			r[2] = string.sub(path, 1, -2) .. '/'
		elseif last ~= '/' then
			r[2] = path .. '/'
		else
			r[2] = path
		end
		r[1] = handle
		r[3] = (type(filter) == 'string' and #filter > 0) and filter or '*'

		return setmetatable(r, dirt)
	end

	dirt.__index = {
		pick = function(self)
			if self[1] ~= nil then
				local name = readdirinfo(self[1], self[3])
				if name ~= nil then
					self[4] = ffi.string(name)
					self[5] = nil
					return true
				end
			end
			return false
		end,
		close = function(self)
			if self[1] ~= nil then
				ffi.C.closedir(self[1])
				self[1], self[4] = nil, nil
			end
		end,
		name = function(self)
			return self[4] or ''
		end,
		fullname = function(self)
			return self[2] .. (self[4] or '')
		end,
		
		getAttrs = function(self)
			if not self[5] then
				self[5] = getpathattrs(self[2] .. (self[4] or ''))
			end
			return self[5]
		end,
	}

	_createDir = function(path, mode)
		return createdir(path, mode2n(mode) or 0)
	end
end

local fsysPub = {
	__index = {
		--检测路径是否是个文件
		pathIsFile = _isFile,
		--检测路径是否是个目录
		pathIsDir = _isDir,
		--检测路径是否存在（文件=1或是目录=2），返回nil说明该路径不存在（文件也不是目录也不是）
		pathIsExists = _isExists,

		--获取属性值，然后可以用于checkAttr函数的参数1，这样可以减少同一个path多次checkAttr时的IO操作次数
		getAttr = _getAttr,

		--checkAttr(path, checkWhat)
		--path可以是一个路径，也可以是getAttr函数的返回值
		--what可以是：file|dir|hidden|temporary|link|socket，或者777、755、422、511等等，这种8进制表示的权限（但请注意要传入字符串而不能是数字，因为Lua原生不支持0777这种8进制数）
		checkAttr = _checkAttr,

		--openDir(path[, filter])
		--filter为文件名过滤器，通配符可随意用，如a.*，或xxx*。也可以没有，且指定为*|.|*.*和不指定是没有区别的，还不如不指定
		openDir = _openDir,

		--支持多级创建
		createDir = function(path, mode, recur)
			if recur then
				local s, drvLet = 0, false
				local first = path:sub(1)
				local segs = string.split(path, '/\\')
				local newpath = (first == '\\' or first == '/') and path or ''

				if ffi.os == 'Windows' and #segs[1] == 2 and string.byte(segs[1], 2) == 58 then
					drvLet = true
				end

				for i = 1, #segs do
					local chk = newpath .. segs[i]
					if (drvLet and i == 1) or _isDir(chk) then
						newpath = chk .. '/'
					else
						s = i
						break
					end
				end

				if s ~= 0 then
					for i = s, #segs do
						newpath = newpath .. segs[i]
						if not _createDir(newpath) then
							return false
						end

						newpath = newpath .. '/'
					end
				end

				return true
			end

			return _createDir(path, mode)
		end,
		--支持多级删除
		deleteDir = function(dir)
			return libreemext.deleteDirectory(dir) ~= 0
		end,

		deleteFile = function(dir)
			return libreemext.deleteFile(dir) ~= 0
		end,

		--取文件尺寸
		filesize = io.filesize(path),
		
		--取文件扩展名，未成功返回nil
		fileext = function(path)
			local ext = string.rfindchar(path, '.')
			if ext then
				return string.lower(string.sub(path, ext + 1))
			end
		end,
		
		--分解路径
		pathinfo = function(path)
			local r = table.new(0, 6)
			local namepos = string.rfindchar(path, '/', 1, true)
			if not namepos then
				namepos = string.rfindchar(path, '\\', 1, true)
			end
			
			if namepos then
				r.path = string.sub(path, 1, namepos)
				r.name = string.sub(path, namepos + 1)
				if #r.name > 1 then
					local ext = string.rfindchar(r.name, '.')
					if ext then
						r.ext = string.lower(string.sub(r.name, ext + 1))
						r.name = string.sub(r.name, 1, ext)
					end
				end
			else
				r.path = path
			end
			
			if ffi.os == 'Windows' then
				local second, third = string.byte(r.path, 2, 3)
				if second == 58 and (third == 47 or third == 92) then
					r.driver = string.sub(r.path, 1, 1)
					r.path = string.sub(r.path, 4)
				end
			end
			
			r.merge = mergePath

			return r
		end,
	}
}

return function()
	return setmetatable({}, fsysPub)
end
