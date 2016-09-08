#include <boost/asio.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>

#include "reeme.h"

#include "lua.hpp"
#include "re2/re2.h"
#include "re2/regexp.h"
#include "zlib/zlib.h"

#include "json.h"
#include "lua_utf8str.h"
#include "lua_string.h"
#include "lua_table.h"
#include "sql.h"

#include "commonlib.h"
#include "lua_deamon.h"
#include "sqlite/sqlitewrap.h"

// log4z, get from : https://github.com/zsummer/log4z , thanks
#include "log4z.h"
#include "packets.h"
#include "deamon.h"

ILog4zManager* logs = 0;
Service* pService = 0;

#include "sqls.h"
#include "pckexec.h"
#include "console_windows.h"

//////////////////////////////////////////////////////////////////////////
Client::Client(Service* s)
	: service(s)
	, sock(*s->ioService)
{
}

void Client::onReadHeader(const boost::system::error_code& error)
{
	if (error)
	{
		service->removeClient(this, &error);
		return ;
	}

	if (pckHeader.bodyLeng > service->cfgMaxPacketSize)
	{
		service->removeClient(this, NULL);
		return ;
	}

	readLength(pckHeader, pckHeader.bodyLeng);	
}

void Client::onReadData(char* mem, const boost::system::error_code& error)
{
	if (error)
	{
		service->removeClient(this, &error);
		return;
	}

	PckHeader* hd = (PckHeader*)mem;
	char* dst = (char*)(hd + 1);

	if (CRC32Check(dst, hd->bodyLeng) != hd->crc32)
	{
		free(hd);
		service->removeClient(this, NULL);
		return ;
	}


	service->pushPacket(hd);

	start();
}

//////////////////////////////////////////////////////////////////////////
void Service::run()
{
	LOGI("TaskDeamon started");

	bRunning = true;
	while(bEventLooping)
	{
		try
		{
			boost::system::error_code ec;
			ioService->run(ec);

			if (ec)
				LOGW("Run Error: " << ec);
			break;
		}
		catch (std::exception & ex)
		{
			LOGW("Run Catched Exception: " << ex.what());
		}
	}

	// 结束
	boost::system::error_code ec;
	acceptor->close(ec);

	clientsListLock.lock();
	Client* c;
	while(c = clients.popFirst())
	{
		c->stop();
		delete c;
	}
	clientsListLock.unlock();

	threadsListLock.lock();
	WorkThread* wt;
	for(size_t i = 0; i < workThreads.size(); ++ i)
		taskCond.notify_one();
	while(wt = workThreads.popFirst())
	{
		taskCond.notify_one();
		wt->waitExit();
		delete wt;
	}
	threadsListLock.unlock();

	ioService->reset();

	// 清理没有运行完的包内存
	flushPackets();

	bRunning = false;
}

void Service::stop()
{
	bEventLooping = false;
	ioService->stop();
}

bool Service::openDB(const char* dbpath)
{
	int r;	

	if (dbpath[0] == '?')
	{
		PATH_STRING strPath;
		getTaskDeamonAtPath(strPath, dbpath + 1);
		db = openSqliteDB(strPath.c_str(), &r);
	}
	else
	{
		db = openSqliteDB(dbpath, &r);
	}

	if (!db)
	{
		LOGFMTE("Open TaskDeamon Database at path [%s] failed, error code: %d", dbpath, r);
		return false;
	}

	uint32_t flags = 0xFFFF;
	const char* sqls[] = { SQLCreateTable_Schedule, SQLCreateTable_HttpReqTasks };

	DBSqlite::Result* result = db->query("SELECT name FROM sqlite_master WHERE type = 'table'", 0, 0, DBSqlite::kIndexNum);
	if (!result)
		return false;

	while(result->next())
	{			
		const char* s = result->getString(0);
		if (strcmp(s, "schedules") == 0)
			flags &= ~(1 << 0);
		else if (strcmp(s, "httpreq_tasks") == 0)
			flags &= ~(1 << 1);
	}
	result->drop();

	// 将需要执行的执行
	for (uint32_t i = 0; i < sizeof(sqls) / sizeof(const char*); ++ i)
	{
		if (flags & (1 << i))
		{
			if (!db->exec(sqls[i]))
				return false;
		}
	}

	// 然后查询数据库，读取已经安排了的任务
	result = db->query("SELECT * FROM schedules");
	if (!result)
		return false;

	while(result->next())
	{
		ScheduleData* sd = new ScheduleData();
		sd->scheduleId = result->getInt("schid");
		sd->mode = (ScheduleMode)result->getInt("mode");
		sd->repeatInterval = result->getInt("repeat_interval");
		sd->repeatTimes = result->getInt("repeat_times");
	}
	result->drop();

	return true;
}

void Service::flushPackets()
{
	tasksListLock.lock();
	while(packets.size())
	{
		PckHeader* hd = packets.front();
		packets.pop_front();

		packetExecute(this, L, hd, true);
		free(hd);			
	}
	tasksListLock.unlock();
}

void Service::pushPacket(PckHeader* hd)
{
	tasksListLock.lock();
	packets.push_back(hd);
	tasksListLock.unlock();

	boost::recursive_mutex::scoped_lock lock(condLock);
	taskCond.notify_one();
}

bool Service::listenAt(const char* ip, lua_Integer port)
{
	try
	{
		boost::asio::ip::tcp::resolver resolver(*ioService);
		boost::asio::ip::tcp::resolver::query query(
			ip,
			boost::lexical_cast< std::string >(port)
			);
		boost::asio::ip::tcp::endpoint endpoint = *resolver.resolve(query);

		acceptor->open(endpoint.protocol());
		acceptor->set_option(boost::asio::ip::tcp::acceptor::reuse_address(false));
		acceptor->bind(endpoint);
		acceptor->listen(boost::asio::socket_base::max_connections);

		acceptOne();			
	}
	catch (std::exception & ex)
	{
		LOGFMTE("Listen at %s:%u failed\n", ip, port);
		return false;
	}

	return true;
}

void Service::acceptOne()
{
	Client* c = new Client(this);
	acceptor->async_accept(c->sock, boost::bind(&Service::onAccept, this, c, boost::asio::placeholders::error));
}

void Service::removeClient(Client* c, const boost::system::error_code* err)
{
	clientsListLock.lock();
	bool ok = clients.remove(c);
	clientsListLock.unlock();

	if (ok)
	{
		c->stop();
		delete c;
	}
}

void Service::onAccept(Client* c, const boost::system::error_code& error)
{
	if (error)
	{
		delete c;
		return ;
	}

	clientsListLock.lock();
	clients.append(c);
	clientsListLock.unlock();

	c->start();

	acceptOne();
}

bool Service::parserStartup(const char* pszStartupArgs, size_t nCmdLength)
{
	// 将启动参数（一个JSON）加载到strStartupJson字符串中
	bool bInQuote = false;
	std::string strStartupJson;

	if (pszStartupArgs[0] == '"')
	{
		if (pszStartupArgs[nCmdLength - 1] != '"')
			return false;

		bInQuote = true;
		pszStartupArgs ++;
		nCmdLength -= 2;
	}

	if (strncmp(pszStartupArgs, "json:", 5) == 0)
	{
		pszStartupArgs += 5;
		nCmdLength -= 5;

		if (bInQuote)
		{
			strStartupJson.reserve(nCmdLength);
			for (size_t i = 0; i < nCmdLength; ++ i)
			{
				char ch = pszStartupArgs[i];
				if (ch == '\\')
				{
					++ i;
					continue;
				}

				strStartupJson += ch;
			}
		}
		else
			strStartupJson.append(pszStartupArgs, nCmdLength);
	}
	else if (strncmp(pszStartupArgs, "file:", 5) == 0)
	{
		strStartupJson.append(pszStartupArgs + 5, nCmdLength);
		FILE* fp = fopen(strStartupJson.c_str(), "rt");
		if (!fp)
		{
			LOGFMTE("Open startup file \"%s\" failed\n", pszStartupArgs + 5);
			return false;
		}

		fseek(fp, 0L, SEEK_END);
		long s = ftell(fp);
		fseek(fp, 0L, SEEK_SET);

		strStartupJson.clear();
		if (s > 2)
		{				
			strStartupJson.resize(s);
			fread(const_cast<char*>(strStartupJson.c_str()), 1, s, fp);;
		}
		fclose(fp);
	}

	if (strStartupJson.size() < 2)
	{
		LOGE("Invalid startup json data");
		return false;
	}

	// 初始化LuaJIT和相关的函数库
	L = luaL_newstate();
	luaL_openlibs(L);
	initCommonLib(L);

	// 解析JSON
	lua_pushcfunction(L, &lua_string_json);
	lua_pushlstring(L, strStartupJson.c_str(), strStartupJson.length());
	lua_pcall(L, 1, 1, 0);
	int jsonIdx = lua_gettop(L);
	if (!lua_istable(L, jsonIdx))
		return false;


	// 读监听的地址参数
	lua_Integer port = 5918;
	const char* host = "127.0.0.1";

	lua_getfield(L, jsonIdx, "listen");
	if (lua_istable(L, -1))
	{
		lua_getfield(L, jsonIdx, "host");
		host = luaL_optstring(L, -1, "127.0.0.1");

		lua_getfield(L, jsonIdx, "port");
		port = luaL_optinteger(L, -1, 5918);
	}

	// 线程池的最大线程数量
	lua_Integer nbWorkThreads = 1;

	lua_getfield(L, jsonIdx, "workthreads");
	nbWorkThreads = luaL_optinteger(L, -1, 1);

	// 数据库文件存放位置
	lua_getfield(L, jsonIdx, "dbpath");
	const char* dbpath = luaL_optstring(L, -1, "?/taskdeamon.sqlite3");

	// 日志
	lua_getfield(L, jsonIdx, "logpath");
	const char* logpath = luaL_optstring(L, -1, "?/taskdeamon-logs");

	// 最大可接收的单个包容量
	lua_getfield(L, jsonIdx, "maxpacketsize");
	cfgMaxPacketSize = luaL_optinteger(L, -1, UINT_MAX);

	// 初始化日志
	logs = ILog4zManager::getInstance();
	if (logpath[0] == '?')
	{
		std::string strPath;
		getTaskDeamonAtPath(strPath, logpath + 1);
		logs->setLoggerPath(0, strPath.c_str());
	}
	else
	{
		logs->setLoggerPath(0, logpath);
	}	
	logs->start();

	LOGI("TaskDeamon starting ...");

	// 读参完毕，启动服务
	if (!this->listenAt(host, port))
		return false;

	// 创建DB
	if (!this->openDB(dbpath))
		return false;

	// 启动服务成功，初始化线程池
	for(uint32_t i = 0; i < nbWorkThreads; ++ i)
	{
		WorkThread* wt = new WorkThread(this);
		workThreads.append(wt);
		wt->start();
	}

	lua_settop(L, 0);
	return true;
}

#ifdef _WINDOWS
void Service::getTaskDeamonAtPath(std::wstring& path, const char* dir)
{
	size_t dirleng = 0;
	wchar_t szThisModule[512] = { 0 };
	DWORD cchDir = 0, cchModule = GetModuleFileNameW(GetModuleHandle(L"taskdeamon"), szThisModule, 512);

	if (dir)
	{
		if (dir[0] == '/')
			dir ++;

		dirleng = strlen(dir);
		cchDir = MultiByteToWideChar(CP_UTF8, 0, dir, dirleng, 0, 0);
	}

	wchar_t* pszModFilename = wcsrchr(szThisModule, L'\\');
	assert(pszModFilename);
	pszModFilename[1] = 0;
	cchModule = wcslen(szThisModule);

	path.resize(cchModule + cchDir);
	wchar_t* dst = const_cast<wchar_t*>(path.c_str());
	
	wcsncpy(dst, szThisModule, cchModule);
	if (cchDir)
		MultiByteToWideChar(CP_UTF8, 0, dir, dirleng, dst + cchModule, cchDir + 1);

	std::replace(path.begin(), path.end(), L'\\', L'/');
}
#endif
void Service::getTaskDeamonAtPath(std::string& path, const char* dir)
{
#ifdef _WINDOWS
	std::wstring wpath;
	getTaskDeamonAtPath(wpath, dir);

	int cch = WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), wpath.length(), 0, 0, 0, 0);
	path.resize(cch);
	WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), wpath.length(), const_cast<char*>(path.c_str()), cch + 1, 0, 0);
#else
#endif
}

//////////////////////////////////////////////////////////////////////////
void WorkThread::onThread()
{
	isRunning = true;

	PckHeader* hd;
	boost::recursive_mutex& mtx = service->condLock;
	while(service->bEventLooping)
	{
		boost::recursive_mutex::scoped_lock locker(mtx);
		service->taskCond.wait(mtx);

		while(service->bEventLooping)
		{
			hd = 0;
			service->tasksListLock.lock();
			if (service->packets.size())
			{
				hd = service->packets.front();
				service->packets.pop_front();
			}		
			service->tasksListLock.unlock();

			if (!hd)
				break;

			packetExecute(service, L, hd, false);
			free(hd);
		}
	}

	isRunning = false;
}

//////////////////////////////////////////////////////////////////////////
// main函数，确保全局只有一个进程
#ifdef _WINDOWS
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	const char* pszStartupArgs = lpCmdLine;
	size_t nCmdLength = strlen(pszStartupArgs);
	if (nCmdLength < 6)
		return 0;

	HANDLE  hSingleInstance = CreateEventW(NULL, FALSE, FALSE, L"reemext.task_service");
	if (hSingleInstance == NULL)
	{
		CloseHandle(hSingleInstance);
		return 0;
	}

	if (GetLastError() == ERROR_ALREADY_EXISTS)
	{
		// 已经存在了
		CloseHandle(hSingleInstance);
		return 0;
	}
#else
int main(int argc, char* argv[])
{
	if (argc != 2)
		return 0;

	const char* pszStartupArgs = argv[1];
	size_t nCmdLength = pszStartupArgs;
	if (nCmdLength < 6)
		return 0;	
#endif

	pService = new Service();

	if (pService->parserStartup(pszStartupArgs, nCmdLength))
	{
		pService->run();
	}
	
	delete pService;
	if (logs) logs->stop();

#ifdef _WINDOWS
	CloseHandle(hSingleInstance);
#else
#endif
	return 0;
}