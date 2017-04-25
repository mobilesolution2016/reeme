--[[ 本类基本用法：

基于一个Lua的Table构造（该该Table会被自动的转Json字符串）
1、self.mysql:json({ 一个Table })
基于一个合法的Json字符串直接构造（可以省去了自动编码Json为字符串的过程）
2、self.mysql:json('{"a":1, "b":"Hello!"}')
不基于Json构造
3、self.mysql:json()

以上3种常规用法，前2种其实是一个道理，就是基于一个合法的Json来构造成Json表达式，而第2种不带任何参数的使用则只应出现在对Json类型的
字段进行操作，但是又不需要使用到Json的时候，比如搜索一个Json型的字段里是否包含某些文字内容，就需要不带参数的构造

以下是一些使用的样例：

local model = self.mysql:use('xxxxx')

model:query():where('jsonfield', self.mysql:json({ a = 1 }):contains())	--这一行代码和下一行代码是完全等效的
model:query():where('jsonfield', self.mysql:json():contains({ a = 1 }))

model:query():where('jsonfield', self.mysql:json():findall('%cindy%'))	--查找Json内容中包含有cindy的全部记录

local r = model:new()
r.jsonfield = { a = 1, b = 2, name = 'hello cindy' }	--这一行代码和下一行代码是完全等效的
r.jsonfield = self.mysql:json({ a = 1, b = 2, name = 'hello cindy' })

r = model:query():exec()
r.jsonfield = self.mysql:json({ added = true }):merge()
r:save()


如果对于mysql:json构造的时候给出Json还是诸如merge/findall/contains等调用的时候给出Json，只需要遵循以下原则即可：
设置为新的值用的json构造，必须放在构造函数里
而做为where用的json构造，则无所谓放在构造函数里
但是总的一点：放在构造函数里的只能是有效的Json（Table or String），不能是一个普通的字符串，否则一定会报错
]]

local myjsonMeta = {
	__index = {
		--合并两个Json(val如果不存在，则在json构造函数里必须传入了)
		merge = function(self, json)
			self[1] = json
			self._func = 'JSON_MERGE'
			return self
		end,
		
		--在现有的Json中插入新的Path(val如果不存在，则在json构造函数里必须传入了)
		insert = function(self, path, json)
			assert(type(path) == 'string')
			
			self[1] = json
			self._path = path
			self._func = 'JSON_INSERT'
			return self
		end,
		
		--在现有的Json中替换Path为新的Json(val如果不存在，则在json构造函数里必须传入了)
		replace = function(self, path, json)
			assert(type(path) == 'string')
			
			self[1] = json
			self._path = path
			self._func = 'JSON_REPLACE'
			return self
		end,
		
		--在现有的Json中指定的Path后追加数组(val如果不存在，则在json构造函数里必须传入了)
		appendArray = function(self, path, json)
			assert(type(path) == 'string')
			
			self[1] = json
			self._path = path
			self._func = 'JSON_ARRAY_APPEND'
			return self
		end,
		
		--在现有的Json中移动指定的Path
		remove = function(self, path)
			assert(type(path) == 'string')
			
			self._path = path
			self._func = 'JSON_REMOVE'
			return self
		end,
		
		--根据val查找全部
		findall = function(self, val)
			self._all, self._not = true, nil
			self[1] = val
			return self
		end,
		--根据val查找第1个
		findone = function(self, val)
			self._all, self._not = nil, nil
			self[1] = val
			return self
		end,
		
		--根据val查找不存在的全部
		findallnot = function(self, val)
			self._all, self._not = true, true
			self[1] = val
			return self
		end,
		--根据val查找不存在的第1个
		findonenot = function(self, val)
			self._all, self._not = nil, true
			self[1] = val
			return self
		end,
		
		--根据val查找包含的
		contains = function(self, val)
			self[1] = val
			return self
		end,
	}
}

--如果传入一个Json字符串或者Table，可以直接构造一个json对象返回，这样可以省去通过self.mysql:json这样的调用
--如果想空构造一个Json，则doc参数值为true即可
return function(doc)
	if doc then
		local json, tp = {}, type(doc)
		if tp == 'table' or tp == 'string' then
			json[0] = doc
		end
		return setmetatable(json, myjsonMeta)
	end
	
	return myjsonMeta
end