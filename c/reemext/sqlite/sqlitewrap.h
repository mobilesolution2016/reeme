#ifndef _REEME_EXTC_SQLITE_H__
#define _REEME_EXTC_SQLITE_H__

class DBSqlite
{
public:
	typedef MAP_CLASS_NAME<StringPtrKey, const char*> ColumnsMap;

	enum
	{
		kIndexNum = 1,
		kIndexName = 2,
		kIndexAll = 3
	};

	enum Action
	{
		kBegin,
		kCommit,
		kRollback
	} ;

	//!值类型
	enum ValueType
	{
		kValueInt = 1,
		kValueNumeric,
		kValueText,
		kValueBlob,
		kValueNull,
		kValueError,
	};

	//!值组
	struct Value
	{
		ValueType		type;
		size_t			len;

		union {
			double		d;
			int32_t		i;
			int64_t		i64;
			const char	*s;
			const void	*p;
		} v;
	};

	//!结果集
	class Result
	{
	public:
		virtual void reset() = 0;
		virtual bool next() = 0;
		virtual void drop() = 0;

		virtual const char* getColumnName(int32_t nIndex) const = 0;
		virtual ValueType getType(int32_t nIndex, uint32_t* pnValueLeng = 0) const = 0;

		virtual int32_t getInt(int32_t nIndex) const = 0;
		virtual int64_t getInt64(int32_t nIndex) const = 0;
		virtual double getDouble(int32_t nIndex) const = 0;
		virtual const char* getString(int32_t nIndex, uint32_t* pnValueLeng = 0) const = 0;
		virtual const void* getBlob(int32_t nIndex, uint32_t* pnValueLeng = 0) const = 0;

		virtual int32_t mapCol(const char* name) const = 0;

		inline int32_t getInt(const char* name) const
		{
			int32_t id = mapCol(name);
			if (id != -1)
				return getInt(id);
			return 0;
		}
		inline int64_t getInt64(const char* name) const
		{
			int32_t id = mapCol(name);
			if (id != -1)
				return getInt64(id);
			return 0;
		}
		inline double getDouble(const char* name) const
		{
			int32_t id = mapCol(name);
			if (id != -1)
				return getDouble(id);
			return 0;
		}
		inline const char* getString(const char* name, uint32_t* pnValueLeng = 0) const
		{
			int32_t id = mapCol(name);
			if (id != -1)
				return getString(id);
			return 0;
		}
		inline const void* getBlob(const char* name, uint32_t* pnValueLeng = 0) const
		{
			int32_t id = mapCol(name);
			if (id != -1)
				return getBlob(id);
			return 0;
		}		

		inline bool isNull(int32_t nIndex) const { return getType(nIndex) == kValueNull; }
		inline uint32_t getColsCount() const { return m_nColsCount; }

	protected:
		uint32_t			m_nColsCount;
	};

public:
	//!查询并返回一个结果集
	/*!
	* 执行一个SQL，并且在执行完之后返回一个结果集，所以，非SELECT类的SQL，使用exec会效率更高。函数执行之前会清空最后一条错误信息
	* \param sql SQL语句
	* \param vals 允许SQL语句中使用?这个SQLITE的替换符使用本参数给定的值进行替换
	* \param count vals数组的大小
	* \param rowIndexType 返回的结果集的索引方式，默认将生成数字和列名索引。不过有时候可以为了性能，只生成数字索引，这样的效率可以高一点点
	* \return 返回SQL操作的结果集，只要SQL没有出错，返回的结果集永远不会为NULL
	*/
	virtual Result* query(const char* sql, const Value* vals = 0, uint32_t count = 0, uint32_t rowIndexType = kIndexAll) = 0;

	//!执行一条SQL语句
	/*!
	* 简单的执行一条SQL语句，不获取返回结果，所以适合所有UPDATE、DELETE、ALERT等无须返回结果的语句。函数执行之前会清空最后一条错误信息
	* \param sql SQL语句
	* \param vals 允许SQL语句中使用?这个SQLITE的替换符使用本参数给定的值进行替换
	* \param count vals数组的大小
	*/
	virtual bool exec(const char* sql, const Value* vals = 0, uint32_t count = 0) = 0;

	//!事务管理
	virtual void action(Action k) = 0;

	//!修改表
	/*!
	* 由于Sqlite本身的不足，ALERT TABLE主要只能用来添加字段或更改字段名称这类的简单操作，如果要删除字段，目前可以施行的办法只能是重建一张COPY表，将新表
	* 的字段设置好，然后将原表的数据COPY过去，再将原表删除，最后将新表更名。如果表的数据量已经较大，则此操作将可能会耗费较长的时间以及较多的代码，因此，
	* 本类提供了这样一个修改表的函数，用以提升修改表的效率以达到最高
	* \param tableName 现在的表名
	* \param tmpTableName 已经建立好的新的复制表的名称，待操作完成之后，本表名称会被修改为参数1指定的表名
	* \param reservedColumns 需要保留的字段。将所有的字段以StringPtrKey传入，不指定的字段，将不会在复制时被处理。所以，如果新表存在的字段而本字符串集中
	* 没有指定，就要求新表的列必须指定了默认值，否则复制数据时会出错
	* \return 返回true表示操作成功。如果是异步操作下，将永远返回true
	*/
	virtual bool updateTable(const char* tableName, const char* tmpTableName, const ColumnsMap& reservedColumns) = 0;

	//!获取最后一次插入的ID
	virtual uint64_t getLastInsertId() const = 0;

	//!关闭数据库并delete this
	virtual void destroy() = 0;
} ;

extern DBSqlite* wrapSqliteDB(void* dbOpened);
#ifdef _WINDOWS
extern DBSqlite* openSqliteDB(const wchar_t* path, int* r = 0);
#endif
extern DBSqlite* openSqliteDB(const char* path, int* r = 0);

#endif