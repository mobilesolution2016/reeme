#ifndef _REEME_DEAMON_H__
#define _REEME_DEAMON_H__

enum ScheduleMode {
	// 立即执行
	kScheduleImmediate = 1,
	// 按星期+时间段周期运行
	kScheduleDayCycles,
	// 按间隔时间周期运行（单位：秒）
	kScheduleInterval,
};

enum ScheduleFlags {
	// 在CPU空闲时才执行
	kSFlagCPUFreed = 0x1,
	// 一旦有任何一个任务的返回值不匹配，就中断执行
	kSFlagHaltWhenResultNotMatched = 0x2,
	// 一旦有任何一个请求发生超时或错误，就中断执行
	kSFlagHaltWhenHttpRequestFailed = 0x4,
} ;

/* 所有可用的URL Tag
	{$totaltasks}  总任务数（不含汇报进度的任务）
	{$currentno}	当前是第几个（不含汇报进度的任务）
*/


class TaskDataBase
{
public:
	enum TaskType {
		kTaskHttpReq,
		kTaskScript,
	} ;

	uint32_t		taskid;
	TaskType		taskType;
} ;

class HTTPRequestTask : public TaskDataBase
{
public:
	std::string		strUrl;
	std::string		strPosts;
	std::string		strDownloadTo;
	std::string		strResultForce;

	inline const HTTPRequestTask& operator = (const HTTPRequestTask& ) { return *this; }
};

class ScriptTask : public TaskDataBase
{
public:
	bool			bDataIsFilename;
	std::string		strData;

	inline const ScriptTask& operator = (const ScriptTask& ) { return *this; }
};

class ScheduleData : public TListNode<ScheduleData>
{
public:
	typedef std::list<TaskData> TasksList;

	TasksList		tasks;

	ScheduleMode	mode;
	uint32_t		flags;
	uint32_t		scheduleId;
	uint32_t		repeatTimes, repeatInterval;	// 重复运行的次数以及每次重复间隔的时间
	uint64_t		startTime;						// 在这个时间点后才开始运行，如果为0则表示立即运行

public:
	ScheduleData()
		: repeatTimes(0), repeatInterval(0), flags(0), startTime(0)
	{

	}
};

typedef TList<ScheduleData> SchedulesList;

//////////////////////////////////////////////////////////////////////////
class Service;
class Client : public TListNode<Client>
{
public:
	typedef boost::asio::ip::tcp::socket tcpsocket;

public:
	Client(Service* s);

	void start()
	{
		boost::asio::async_read(sock,
			boost::asio::buffer((char*)&pckHeader, sizeof(PckHeader)),
			boost::bind(&Client::onReadHeader, this, boost::asio::placeholders::error)
			);
	}

	void readLength(PckHeader& hd, size_t leng)
	{
		PckHeader* dst = (PckHeader*)malloc(leng + sizeof(hd));
		*dst = hd;

		boost::asio::async_read(sock,
			boost::asio::buffer((char*)(dst + 1), leng),
			boost::bind(&Client::onReadData, this, (char*)dst, boost::asio::placeholders::error)
			);
	}

	void stop()
	{
		boost::system::error_code ec;
		sock.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
		sock.close(ec);
	}

	void onReadHeader(const boost::system::error_code& error);
	void onReadData(char* mem, const boost::system::error_code& error);

public:
	tcpsocket			sock;
	Service				*service;
	PckHeader			pckHeader;	
} ;

class WorkThread : public TListNode<WorkThread>
{
public:
	WorkThread(Service* s) 
		: service(s) 
		, thread(boost::bind(&WorkThread::onThread, this))
		, isRunning(false)
	{
		L = luaL_newstate();
		luaL_openlibs(L);
		initCommonLib(L);
	}

	~WorkThread()
	{
		lua_close(L);
	}

	void onThread();
	void start()
	{
		thread.detach();
	}
	void waitExit()
	{
		while(isRunning)
			boost::this_thread::sleep_for(boost::chrono::milliseconds(50));
	}

public:
	Service				*service;
	boost::thread		thread;
	lua_State			*L;
	bool				isRunning;
} ;

class Service
{
public:
	typedef TList<Client> Clients;
	typedef TList<WorkThread> WorkThreads;
	typedef std::list<PckHeader*> WaitingPackets;

public:
	Service()
		: ioService(new boost::asio::io_service)
		, work(new boost::asio::io_service::work(*ioService))
		, acceptor(new boost::asio::ip::tcp::acceptor(*ioService))		
		, bEventLooping(true)
		, bRunning(false)
		, db(NULL)
		, L(NULL)
	{
	}
	~Service()
	{
		if (L) lua_close(L);
		if (db) db->destroy();
	}

	void run();
	void stop();
	bool openDB(const char* dbpath);
	void flushPackets();
	void pushPacket(PckHeader* hd);
	bool listenAt(const char* ip, lua_Integer port);
	void acceptOne();
	void removeClient(Client* c, const boost::system::error_code* err);
	void onAccept(Client* c, const boost::system::error_code& error);
	bool parserStartup(const char* pszStartupArgs, size_t nCmdLength);
	void addSchedule(ScheduleData* p);

#ifdef _WINDOWS
	void getTaskDeamonAtPath(std::wstring& path, const char* dir);
#endif
	void getTaskDeamonAtPath(std::string& path, const char* dir);

public:
	boost::mutex clientsListLock;
	boost::mutex threadsListLock;
	boost::mutex pcksListLock;
	boost::mutex scheduleLock;

	boost::recursive_mutex condLock;
	boost::condition_variable_any taskCond;

	boost::shared_ptr< boost::asio::io_service > ioService;
	boost::shared_ptr< boost::asio::io_service::work > work;
	boost::shared_ptr< boost::asio::ip::tcp::acceptor > acceptor;	

	Clients	clients;
	WorkThreads workThreads;
	WaitingPackets packets;
	SchedulesList schedules;

	lua_State* L;
	DBSqlite* db;
	bool bEventLooping;
	bool bRunning;

	uint32_t	cfgMaxPacketSize;
} ;

#endif