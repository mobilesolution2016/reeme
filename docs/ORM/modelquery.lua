--@type:doc
--name ORM.modelquery
--title 使用单模型查询

--segment 定义了模型之后，就可以用于CRUD操作，本篇将仅说明如何使用单个模型进行Query操作

--segment 仅是单个模型查询的操作，也是可以有很多种”玩法“的，带或不带Where条件，可以只返回第1条或前几记录等。为了方便用代码编写这些查询，Reeme的ORM提供了各种便利性的函数来简
---化查询操作的过程。请看下面的代码：

--[[
	local m = reeme.orm:use('testTable')

	local result = m:find()
--]]

--segment 注意第一行的代码：reeme.orm:use('testTable')。这里是使用一个模型，模型定义在模型代码文件中，要让该模型变成类型，可以被使用，就必须要用到orm:use这个函数，给出模型的
---文件名，函数将自动的去事先定义好的模型路径下去寻找相应的模型定义代码文件，找到之后加载该文件，同时返回该定义类型。基于该定义类型，接下来代码中进行了一个查询操作即m:find()。
---另外提一句，orm:use只会在第一次的时候会加载代码，之后如果参数相同的话，那就是直接使用之前加载的代码了。因此不管use多少次，其返回值都是完全一样的，也就是说，ngx.say(tostring(m))，
---你都会看到相同的地址。

--segment 各种各样的基于模型的查询方法示例代码：
--[[
	--不带任何条件在m所代表的模型中进行查询并返回结果集（但是注意这个查询并不会返回所有的结果，具体的原因请参考本节后面关于find函数的说明）
	m:find()
　
	--不带任何条件且仅返回表中的第一条记录
	m:findFirst()
　
	--不带任何条件仅返回表中的前10条记录
	m:find(10)
　
	--不带任何条件仅返回表中的从第10条开始的共100条记录
	m:find(10, 100)
　
	--查询所有列sex的值为1的记录（注意这里依然不是所有，具体的原因请参考本节后面关于find函数的说明）
	m:findBy_sex(1)
	m:find('sex', 1)
	m:find('sex=1')
　
	--查询第一条列sex的值为0的记录
	m:findFirstBy_sex(0)
	m:findFirst('sex', 1)
	m:findFirst('sex=1')
　
	--查询所有列sex的值为1且name中含有mike的记录
	m:find( { sex = 1, name = { "like '%mike%'" } } )
　
	--查询第2至第5条列sex的值为1且name中含有mike的记录
	m:find( { sex = 1, { "name like '%mike%'" } }, 2, 3 )
	m:find("sex=1 AND name like '%mike%'", 2, 3)
--]]

--segment 上面将一些常规的单模型简单查询的示例代码都列了出来。可以发现其实主要的函数就两个，但是名字和参数组合的方式却非常的多。这也是查询的特点。比如findBy_sex(1)，其实就是find('sex', 1)。
---这个findBy_sex函数在定义中其实是不存在的，这里只是通过了一个Lua的代码技术，于是实现了动态的函数名称组合，而实际上这个函数最终调用的还是find函数。所以从效率上来说，findBy_XXXX函数
---其实没有什么意义，因为他还需要再转调一次。见仁见智吧。

--segment 参数的组合方式其实也是有规律可寻的，对于find函数来说，参数只有以下几种情况：
--list find(key, value)，指定字段名称和其值
--list find({ key1 = value1, key2 = value2 })，指定字段多个字段名称和其值
--list find(N)，仅返回前N条记录
--list find(S, N)，仅返回从第S开始的共N条记录
--list find(key, value, N)，将1个和第3个的用法参数组合起来，既限定了要查询的字段名称和值，同时也指定了只返回符合条件的前N条记录
--list find(key, value, S, N），同时，也是更多参数的组合
--list find({ key = value}, S, N)，还是前面几种用法的参数组合

--segment 所以其实可见，参数就只有几种形式而已。而如果查询时的where条件需要有多个的时候，就只有以table的时候进行传入了。

--segment 对于这个table型的where条件，有一种特殊的情况需要单独说明一下，就是条件子table，这一点在上面的例子中就已经出现了，请注意看最后两行代码：<br/>
--segment <strong>m:find( { sex = 1, name = { "like '%mike%'" } } )</strong><br/>
--segment <strong>m:find( { sex = 1, { "name like '%mike%'" } }, 2, 3 )</strong><br/>

--segment name = { 'like', '%mike%' }，这个name的条件值变成了一个table，并且这个table里也只有一个值。为什么明明就是一个字符串直接放在外面就好而偏要放在table里呢？这是因为where条件中字段和值之
---间是可以有多种运算符号的，如果没有特别的说明，这个运算符号在组合出SQL语句的时候都将被默认为"="号，可实际中这是远远不够的，!=、>=、<=等很多，还有各种函数，如上面的LIKE。这些如果直接写在值
---中，会让程序无法判断出字符串里的到底是符号，还是字符串本身的字符。因此，使用一个table包围起来，这就等于告诉了ORM处理逻辑：不要再使用等于号了啊，我已经指定了符号/函数了。当然，这只有在多值的
---时候才需要这么写，像上面的m:find('sex=1 AND name like %mike%', 2, 3)这种方式，就是直接将整个条件句都写出来了，这也是完全可以的。换言之就是：如果值是一个table，那么这个table中的第1个值将会被
---直接拿出来被当成一个字符串，和key组合起来使用，ORM将不会再对其进行任何处理，包括默认了这个字符串中含有所有必要的代码。另外还有一种情况就是包括字段在内可能都是不需要的，那么写法就可以如下：<br/>
---m:find({ sex = 1, { "name like '%mike%'" } })

--segment <strong>另外，在编写条件代码字符串的时候，不要使用A~Z这26个大写字母做为名字，比如COUNT(*) AS A，这样的alias也是不可以的。因为本ORM在建立SQL语句的时候，自动的使用从A~Z的26个字母做为所有表的
---alias名字。</strong>

--@type:api
module('model')

function new()
end

function find()
end

function findFirst()
end

function query()
end

function getField()
end

function getFieldType()
end

function isFieldNull()
end

function isFieldAutoIncreasement()
end

function findUniqueKey()
end