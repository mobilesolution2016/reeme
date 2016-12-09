--常规实现的templatecache
--[[
	set函数的最后一个参数tp是表明v是个什么类型的值，可能出现的字符串值如下：
	'loaded' 表示这个v已经是一个经过了loadstring加载过后的函数，该函数只要给出self和env调用就可以完成模板的解析
	'parsed' 表示这个v是一个刚刚经过了parseTemplate翻译后的字符串，也就是Lua代码，需要使用loadstring函数来进行加载后才能使用

	而get函数返回为双值，第二个值用于描述第一个是什么类型的值，除了可以是上面的两种值之外还可以是下面的：
	'b-code' 表示返回值是一个string.dump出来的字符串，直接使用loadstring就可以加载，会得到一个函数，和set时传入的函数一样
	
	以下是模板缓存在各种测试场景下所带来的效果（测试时均按1000次连续请求来进行计算），时间的单位是：秒：
	
	1、lua_code_cache=on:
		开启线程内函数级缓存：
			不使用缓存 = 0.272s
			使用缓存   = 0.008s
			
		关闭线程内函数级缓存：
			不使用缓存 = 0.268s
			使用缓存   = 0.023s
	
	2、lua_code_cache=off:
		本模式下，是否开启线程内函数级缓存是没有区别的
			不使用缓存 = 0.267s
			使用缓存   = 0.023s
			
	上面的结果将会随着模板的复杂程度而逐渐拉开更大的差距（测试所使用的模板为25KB左右），如果模板的内容比较复杂，尤其是需要解析
	的内容比较多（比如还含有语言包且大量被使用）的话，那差距还会进一步加大。所以使用模板缓存在产品期是非常有必要的。
]]

local cacheFuncs = table.new(0, 128)

local decParams = function(r, uts)
	local ts, pos = string.hasinteger(r)
	if ts == uts then
		local code, tp = string.sub(r, pos + 6), string.sub(r, pos, pos + 5)
		return code, tp
	end
end

return function(sharedName)
	local setfunc = require('reeme.response.view')
	
	if sharedName then
		local caches = ngx.shared[sharedName]
		local pub = {
			--取出的时候，如果fullpath所对应的文件时间已经变了，则缓存会失效
			get = function(self, reeme, fullpath)
				local uts = reeme.fd.fileuts(fullpath)
				
				--优先查找函数级函数
				local r = cacheFuncs[fullpath]
				if r then
					r = decParams(r, uts)
					if not r then
						cacheFuncs[fullpath] = nil
					end
					return r, 'loaded'
				end
				
				--然后再是共享内存
				if caches then
					r = caches:get(fullpath)
					if r then
						local v, t = decParams(r, uts)
						if not v then
							caches:delete(fullpath)
						end
						return v, t
					end
				end
			end,

			--存入的时候，函数会记录fullpath即模板所在文件的文件更新时间
			set = function(self, reeme, fullpath, v, tp, uts)
				if not uts then
					uts = reeme.fd.fileuts(fullpath) or 0
				end
				
				if tp == 'loaded' then
					local r = { uts, 'b-code', string.dump(v) }
					caches:set(fullpath, table.concat(r, ''))
					cacheFuncs[fullpath] = v
					
				elseif tp == 'parsed' then
					local r = { uts, 'parsed', v }
					caches:set(fullpath, table.concat(r, ''))
				end
			end,
		}
		
		setfunc('templatecache', pub)
		return pub
	else
		setfunc('templatecache', nil)
	end
end