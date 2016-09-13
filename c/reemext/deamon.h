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
	// 所有的任务执行完才算整个计划完成，否则就重新执行
	kSFlagCompleteWithAllTasks = 0x8,
} ;

enum ScheduleStatus {
	kScheduleReady,
	kScheduleRunning,
	kScheduleEnded,
};

/* 所有可用的URL Tag
	{$totaltasks}  总任务数（不含汇报进度的任务）
	{$currentno}	当前是第几个（不含汇报进度的任务）
*/


class TaskDataBase : public TListNode<TaskDataBase>
{
public:
	enum TaskType {
		kTaskHttpReq,
		kTaskScript,
	} ;

	uint32_t		taskid;
	TaskType		taskType;

	inline TaskDataBase() {}
	virtual ~TaskDataBase() {}
} ;

class HTTPRequestTask : public TaskDataBase
{
public:
	std::string		strUrl;
	std::string		strPosts;
	std::string		strDownloadTo;
	std::string		strResultForce;

	inline HTTPRequestTask() { taskType = kTaskHttpReq; }
	virtual ~HTTPRequestTask() { }

	inline const HTTPRequestTask& operator = (const HTTPRequestTask& ) { return *this; }
};

class ScriptTask : public TaskDataBase
{
public:
	bool			bDataIsFilename;
	std::string		strData;

	inline ScriptTask() { taskType = kTaskScript; }
	virtual ~ScriptTask() {}

	inline const ScriptTask& operator = (const ScriptTask& ) { return *this; }
};

class ScheduleData : public TListNode<ScheduleData>
{
public:
	typedef TList<TaskDataBase> TasksList;
	typedef TMemoryPool<HTTPRequestTask> HTTPRequestTasksPool;
	typedef TMemoryPool<ScriptTask> ScriptTasksPool;

public:
	TasksList				tasks;
	HTTPRequestTasksPool	hpool;
	ScriptTasksPool			spool;

	ScheduleMode			mode;	
	uint32_t				flags;
	uint32_t				scheduleId;
	uint32_t				repeatTimes, repeatInterval;	// 重复运行的次数以及每次重复间隔的时间
	uint64_t				startTime;						// 在这个时间点后才开始运行，如果为0则表示立即运行

	uint32_t				runIndex;						// 当前正在运行第几条任务

public:
	ScheduleData()
		: repeatTimes(0), repeatInterval(0), flags(0), startTime(0), runIndex(0)
	{

	}

	~ScheduleData()
	{
		TaskDataBase* n;
		while ((n = tasks.popFirst()) != NULL)
		{
			if (n->taskType == TaskDataBase::kTaskHttpReq)
				((HTTPRequestTask*)n)->~HTTPRequestTask();
			else if (n->taskid == TaskDataBase::kTaskScript)
				((ScriptTask*)n)->~ScriptTask();
		}
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
	WorkThread(Service* s, bool isi, bool ist) 
		: service(s) 
		, thread(boost::bind(isi ? &WorkThread::onThread2 : &WorkThread::onThread, this))
		, isRunning(false)
		, isTimeThread(ist)
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
	void onThread2();
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
	bool				isTimeThread;
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
	boost::mutex scheduleExecLock;

	boost::recursive_mutex taskCondLock;
	boost::condition_variable_any taskCond;

	boost::recursive_mutex timerCondLock;
	boost::condition_variable_any timerCond;

	boost::shared_ptr< boost::asio::io_service > ioService;
	boost::shared_ptr< boost::asio::io_service::work > work;
	boost::shared_ptr< boost::asio::ip::tcp::acceptor > acceptor;	

	Clients	clients;
	WorkThreads workThreads;
	WaitingPackets packets;
	SchedulesList schedules, schedulesExecuting;

	lua_State* L;
	DBSqlite* db;
	bool bEventLooping;
	bool bRunning;

	uint32_t	cfgMaxPacketSize;
} ;

#endif