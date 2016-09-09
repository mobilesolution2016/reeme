#include <boost/thread.hpp>

#include "reeme.h"
#include "sqlitewrap.h"
extern "C" {
#include "sqlite3.h"
}

#include "log4z.h"

class implDBSqlite;
class implSqliteResult : public DBSqlite::Result
{
public:
	implSqliteResult()
		: bFinished		(false)
		, pStmt			(0)
	{
	}
	~implSqliteResult()
	{
	}

	void reset()
	{
		bFinished = false;
		sqlite3_reset(pStmt);
	}
	bool next() 
	{
		if (bFinished)
			return false;

		int rc = SQLITE_BUSY;
		while (rc == SQLITE_BUSY)
			rc = sqlite3_step(pStmt);

		if (rc == SQLITE_DONE)
		{
			bFinished = true;
			return false;
		}

		if (rc != SQLITE_ROW)
			return false;

		return true;
	}
	void close() 
	{
		if (pStmt)
			sqlite3_finalize(pStmt);
		pStmt = 0;
	}
	void drop();

	const char* getColumnName(int32_t nIndex) const
	{
		return sqlite3_column_name(pStmt, nIndex);
	}
	DBSqlite::ValueType getType(int32_t nIndex, uint32_t* pnValueLeng) const
	{
		if (pnValueLeng)
			*pnValueLeng = sqlite3_column_bytes(pStmt, nIndex);
		return (DBSqlite::ValueType)sqlite3_column_type(pStmt, nIndex);
	}

	int32_t getInt(int32_t nIndex) const
	{
		return sqlite3_column_int(pStmt, nIndex);
	}
	int64_t getInt64(int32_t nIndex) const
	{
		return sqlite3_column_int64(pStmt, nIndex);
	}
	double getDouble(int32_t nIndex) const
	{
		return sqlite3_column_double(pStmt, nIndex);
	}
	const char* getString(int32_t nIndex, uint32_t* pnValueLeng) const
	{
		if (pnValueLeng)
			*pnValueLeng = sqlite3_column_bytes(pStmt, nIndex);
		return (const char*)sqlite3_column_text(pStmt, nIndex);
	}
	const void* getBlob(int32_t nIndex, uint32_t* pnValueLeng = 0) const
	{
		if (pnValueLeng)
			*pnValueLeng = sqlite3_column_bytes(pStmt, nIndex);
		return sqlite3_column_blob(pStmt, nIndex);
	}

	int32_t mapCol(const char* name) const
	{
		StringPtrKey key(name);
		NameIndices::const_iterator ite = indices.find(key);
		if (ite != indices.end())
			return ite->second;
		return -1;
	}

public:
	inline void setColCount(uint32_t v)
	{
		m_nColsCount = v;
	}

	void mapFields()
	{
		for (int32_t i = 0; i < (int32_t)m_nColsCount; ++ i)
		{
			StringPtrKey key(sqlite3_column_name(pStmt, i));
			indices.insert(NameIndices::value_type(key, i));
		}
	}

public:
	typedef MAP_CLASS_NAME<StringPtrKey, uint32_t> NameIndices;

	implDBSqlite			*pDB;
	sqlite3_stmt			*pStmt;
	NameIndices				indices;
	bool					bFinished;
};

//////////////////////////////////////////////////////////////////////////
class implDBSqlite : public DBSqlite
{
public:
	implDBSqlite()
	{
	}
	~implDBSqlite()
	{
	}

	bool bindValues(sqlite3_stmt* pStmt, const Value* vals, uint32_t count)
	{
		int r = SQLITE_OK;
		for (uint32_t i = 0; i < count; ++ i)
		{
			const Value& v = vals[i];
			int colIndex = i + 1;

			switch (v.type)
			{
			case kValueInt: 
				if (v.len <= 4) 
					r = sqlite3_bind_int(pStmt, colIndex, v.v.i);
				else 
					r = sqlite3_bind_int64(pStmt, colIndex, v.v.i64);
				break;
			case kValueNumeric: r = sqlite3_bind_double(pStmt, colIndex, v.v.d); break;
			case kValueText: r = sqlite3_bind_text(pStmt, colIndex, v.v.s, v.len, SQLITE_STATIC); break;
			case kValueBlob: r = sqlite3_bind_blob(pStmt, colIndex, v.v.p, v.len, SQLITE_STATIC); break;
			case kValueNull: r = sqlite3_bind_null(pStmt, colIndex); break;
			}

			if (r != SQLITE_OK)
			{
				LOGFMTE("SQLITE bind value error [%d] %s", r,  sqlite3_errmsg(db));
				return false;
			}
		}
	}

	sqlite3_stmt* prepareStmt(const char* pszSql)
	{
		const char* szLeftover;
		sqlite3_stmt* pStmt = 0;
		int nRetry = 0, rc = SQLITE_OK;

		while ((rc == SQLITE_OK || (rc == SQLITE_SCHEMA && (++ nRetry < 2))) && pszSql[0])
		{
			rc = sqlite3_prepare_v2(db, pszSql, -1, &pStmt, &szLeftover);
			if (rc != SQLITE_OK)
			{
				LOGFMTE("SQLITE prepare sql error [%d] %s\n\t%s", rc,  sqlite3_errmsg(db), pszSql);
				if (pStmt)
					sqlite3_finalize(pStmt);
				pStmt = 0;
				break;
			}
			if (!pStmt)
			{
				pszSql = szLeftover;
				continue;
			}

			break;
		}

		return pStmt;
	}
	
	bool doExec(const char* sql, const Value* vals, uint32_t count)
	{
		sqlite3_stmt* pStmt = prepareStmt(sql);
		if (!pStmt)
			return false;
		if (count && !bindValues(pStmt, vals, count))
			return false;

		int rc = sqlite3_step(pStmt);
		sqlite3_finalize(pStmt);
		if (rc != SQLITE_DONE)
		{
			LOGE("SQLITE: " << sqlite3_errmsg(db));
			return false;
		}

		return true;
	}

	Result* doQuery(const char* sql, const Value* vals, uint32_t count, uint32_t rowIndexType)
	{
		sqlite3_stmt* pStmt = prepareStmt(sql);
		if (!pStmt)
			return 0;
		if (count)
			bindValues(pStmt, vals, count);

		poolLock.lock();
		implSqliteResult* r = resultsPool.newElement();
		poolLock.unlock();

		r->setColCount(sqlite3_column_count(pStmt));
		r->pStmt = pStmt;
		r->pDB = this;

		if (rowIndexType & kIndexName)
			r->mapFields();

		return r;
	}

	bool doUpdateTable(const char* sql, const char* renameSql, const ColumnsMap& reservedColumns, bool bCommit)
	{
		// 查询出所有的数据
		int32_t cc = 0;
		sqlite3_stmt* pStmt = prepareStmt(sql);
		if (!pStmt)
			return false;

		int32_t nbCols = sqlite3_column_count(pStmt);

		// 使用BIND方式构造出插入新表的INSERT语句
		std::string strInsert;
		strInsert.reserve(512);

		const char* name = strstr(renameSql, " TABLE ") + 7;
		const char* nameEnd = strchr(name, ' ');

		strInsert += "INSERT INTO ";
		strInsert.append(name, nameEnd - name);
		strInsert += '(';

		cc = 0;
		for (ColumnsMap::const_iterator ite = reservedColumns.begin(), iteEnd = reservedColumns.end(); ite != iteEnd; ++ ite, ++ cc)
		{
			if (cc)
				strInsert += ',';
			strInsert += ite->second;
		}

		strInsert += ") VALUES(";

		for(cc = 0; cc < reservedColumns.size(); ++ cc)
		{
			if (cc)
				strInsert += ',';
			strInsert += '?';
		}

		strInsert += ')';

		// 扔掉原表的语句
		std::string strDrop = "DROP TABLE ";
		name = strstr(renameSql, "RENAME TO ") + 10;
		strDrop += name;

		// 开始处理
		if (bCommit)
			doExec("BEGIN", 0, 0);

		int rc;
		bool bErros = false;
		for( ; ; )
		{
			while((rc = sqlite3_step(pStmt)) == SQLITE_BUSY);

			if (rc == SQLITE_ROW)
			{
				sqlite3_stmt* pInsert = prepareStmt(strInsert.c_str());
				if (!pInsert)
				{
					bErros = true;
					break;
				}

				// 逐个绑定所有的列
				for (cc = 0; cc < nbCols; ++ cc)
				{
					switch (sqlite3_column_type(pStmt, cc))
					{
					case kValueInt:
						rc = sqlite3_bind_int64(pInsert, cc + 1, sqlite3_column_int64(pStmt, cc));
						break;

					case kValueNumeric:
						rc = sqlite3_bind_double(pInsert, cc + 1, sqlite3_column_double(pStmt, cc));
						break;

					case kValueText:
						rc = sqlite3_bind_text(pInsert, cc + 1, (const char*)sqlite3_column_text(pStmt, cc), sqlite3_column_bytes(pStmt, cc), SQLITE_STATIC);
						break;

					case kValueBlob:
						rc = sqlite3_bind_blob(pInsert, cc + 1, sqlite3_column_blob(pStmt, cc), sqlite3_column_bytes(pStmt, cc), SQLITE_STATIC);
						break;

					case kValueNull:
						rc = sqlite3_bind_null(pInsert, cc + 1);
						break;
					}
				}

				assert(rc == SQLITE_OK);

				// 执行
				while ((rc = sqlite3_step(pInsert)) == SQLITE_BUSY);

				sqlite3_finalize(pInsert);
				if (rc != SQLITE_DONE)
				{
					LOGE("SQLITE: " << sqlite3_errmsg(db));
					bErros = true;
					break;
				}
			}
			else if (rc == SQLITE_DONE)
			{
				break;
			}
			else
			{
				bErros = true;
				break;
			}
		}

		sqlite3_finalize(pStmt);

		if (!bErros && (!bCommit || doExec("COMMIT", 0, 0)))
		{			
			doExec(strDrop.c_str(), 0, 0);
			return doExec(renameSql, 0, 0);
		}
		else if (bCommit)
		{
			doExec("ROLLBACK", 0, 0);			
		}

		return false;
	}

public:
	bool exec(const char* sql, const Value* vals, uint32_t count)
	{
		return doExec(sql, vals, count);
	}

	Result* query(const char* sql, const Value* vals, uint32_t count, uint32_t rowIndexType)
	{
		if (rowIndexType == 0)
			return 0;

		return doQuery(sql, vals, count, rowIndexType);
	}

	bool action(Action k)
	{
		const char* szLeftover;
		sqlite3_stmt* pStmt = 0;
		const char* sqls[] = { "BEGIN", "COMMIT", "ROLLBACK" };

		int rc = sqlite3_prepare_v2(db, sqls[k], -1, &pStmt, &szLeftover);
		rc = sqlite3_step(pStmt);
		if (rc != SQLITE_DONE)
		{
			LOGE("SQLITE: " << sqlite3_errmsg(db));
			return false;
		}
		sqlite3_finalize(pStmt);

		return true;
	}

	bool updateTable(const char* tableName, const char* tmpTableName, const ColumnsMap& reservedColumns)
	{
		//先组合出SQL
		uint32_t cc = 0;
		std::string strSqlAll, strSqlEnd = "ALTER TABLE ";
		ColumnsMap::const_iterator ite, iteEnd = reservedColumns.end();

		strSqlAll.reserve(384);
		strSqlAll = "SELECT ";

		for (ite = reservedColumns.begin(); ite != iteEnd; ++ ite, ++ cc)
		{
			if (cc)
				strSqlAll += ',';
			strSqlAll += ite->first.pString;
		}

		strSqlAll += " FROM ";
		strSqlAll += tableName;

		//更名SQL
		strSqlEnd += tmpTableName;
		strSqlEnd += " RENAME TO ";
		strSqlEnd += tableName;

		return doUpdateTable(strSqlAll.c_str(), strSqlEnd.c_str(), reservedColumns, true);
	}

	uint64_t getLastInsertId() const
	{
		return sqlite3_last_insert_rowid(db);
	}

	void destroy()
	{
		sqlite3_close(db);
		delete this;
	}

	inline void backResult(implSqliteResult* r)
	{
		poolLock.lock();
		resultsPool.deallocate(r);
		poolLock.unlock();
	}

public:
	sqlite3							*db;
	TMemoryPool<implSqliteResult>	resultsPool;
	boost::mutex					poolLock;
};

void implSqliteResult::drop()
{
	if (pStmt)
		sqlite3_finalize(pStmt);

	this->~implSqliteResult();
	pDB->backResult(this);
}

//////////////////////////////////////////////////////////////////////////
DBSqlite* wrapSqliteDB(void* dbOpened)
{
	implDBSqlite* d = new implDBSqlite();
	d->db = (sqlite3*)dbOpened;
	return d;
}

#ifdef _WINDOWS
DBSqlite* openSqliteDB(const wchar_t* path, int* rv)
{
	sqlite3* db = 0;
	int r = sqlite3_open16(path, &db);
	if (rv)
		*rv = r;

	if (r == SQLITE_OK)
	{
		implDBSqlite* d = new implDBSqlite();
		d->db = db;
		return d;
	}

	return 0;
}
#endif

DBSqlite* openSqliteDB(const char* path, int* rv)
{
	sqlite3* db = 0;
	int r = sqlite3_open(path, &db);

	if (rv)
		*rv = r;

	if (r == SQLITE_OK)
	{
		implDBSqlite* d = new implDBSqlite();
		d->db = db;
		return d;
	}

	return 0;
}