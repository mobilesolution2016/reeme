local _isFile = 0
local _isDir = 0
local _openDir = 0
local _createDir = 0
local _deleteDir = 0

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
		pick = function(dirt)
			if not dirt[4] or kernel32.FindNextFileA(dirt[3], dirt[2]) ~= 0 then
				dirt[4] = ffi.string(dirt[2].cFileName)
				return true
			end

			return false
		end,
		close = function(dirt)
			if dirt[3] then
				kernel32.FindClose(dirt[3])
				dirt[3], dirt[2] = nil, nil
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
	]]
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