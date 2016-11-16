local _isFile = 0
local _isDir = 0
local _openDir = 0
local _createDir = 0

local dirt = {}

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
	]]

	local kernel32 = ffi.load('kernel32.dll')
	local shlwapi = ffi.load('shlwapi.dll')

	_isFile = function(self, path)
		return shlwapi.PathFileExistsA(path) ~= 0
	end

	_isDir = function(self, path)
		return shlwapi.PathIsDirectoryA(path) ~= 0
	end

	_openDir = function(self, path, filter)
		local last = path:sub(#path)
		local r = { (last == '\\' or last == '/') and path or (path .. '/'), ffi.new('WIN32_FIND_DATAA') }

		r[3] = kernel32.FindFirstFileA(r[1] .. (filter == nil and '*.*' or filter), r[2])
		if tostring(r[3]) == 'cdata<void *>: 0xffffffffffffffff' then
			r[2] = nil
			return nil
		end

		return setmetatable(r, dirt)
	end

	dirt.__index = {
		pick = function(self)
			if not self[4] or kernel32.FindNextFileA(self[3], self[2]) ~= 0 then
				self[4] = ffi.string(self[2].cFileName)
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
	}

	_createDir = function(self, path, mode)
		return kernel32.CreateDirectoryA(path, nil) ~= 0
	end

else
	ffi.cdef[[
		void* opendir(const char* path);
		int closedir(void* handle);

		const char* readdirinfo(void* handle, const char* filter);
		bool pathisfile(const char* path);
		bool pathisdir(const char* path);
		bool createdir(const char* path, int mode);
	]]

	local readdirinfo = _G.libreemext.readdirinfo
	local pathisfile = _G.libreemext.pathisfile
	local pathisdir = _G.libreemext.pathisdir
	local createdir = _G.libreemext.createdir

	_isFile = function(self, path)
		return pathisfile(path)
	end
	_isDir = function(self, path)
		return pathisdir(path)
	end

	_openDir = function(self, path, filter)
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
	}

	_createDir = function(self, path, mode)
		return createdir(path, mode or 0)
	end
end

local fsysPub = {
	__index = {
		pathIsFile = _isFile,
		pathIsDir = _isDir,

		openDir = _openDir,

		createDir = function(self, path, mode, recur)
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
					if (drvLet and i == 1) or _isDir(self, chk) then
						newpath = chk .. '/'
					else
						s = i
						break
					end
				end

				if s ~= 0 then
					for i = s, #segs do
						newpath = newpath .. segs[i]
						if not _createDir(self, newpath) then
							return false
						end

						newpath = newpath .. '/'
					end
				end

				return true
			end

			return _createDir(self, path, mode)
		end,
		deleteDir = function(self, dir)
			return libreemext.deleteDirectory(dir) ~= 0
		end,

		deleteFile = function(self, dir)
			return libreemext.deleteFile(dir) ~= 0
		end,

		filesize = function(self, path) return io.filesize(path) end,
	}
}

return function()
	return setmetatable({}, fsysPub)
end
