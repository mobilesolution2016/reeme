#ifdef _WINDOWS

class ConsoleWindow
{
public:
	ConsoleWindow();
	~ConsoleWindow();

	bool open();
	void close();

	void write(int type, const char* lpString, int leng);

public:
	HANDLE			m_hConsole;
	DWORD			m_OldColorAttrs;
	bool			m_bCreate;
	bool			m_bAttrs;
} ;

static ConsoleWindow consolewin;

static BOOL WINAPI consoleHandlerRoutine(DWORD dwCtrlType)
{
	if (dwCtrlType == CTRL_CLOSE_EVENT || dwCtrlType == CTRL_SHUTDOWN_EVENT)
	{
		pService->stop();
		while(pService->bRunning)
			boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
	}
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////
ConsoleWindow::ConsoleWindow()
{
	m_hConsole = NULL;
	m_bCreate = false;
	open();
}

ConsoleWindow::~ConsoleWindow()
{
	close();
}

bool ConsoleWindow::open()
{
	if (AllocConsole())
	{
		m_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		SetConsoleMode(m_hConsole, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT);
		SetConsoleTitle(L"CUI Debug Console");

		//顺便将stdin，out重定向
		freopen("CONIN$", "r+t", stdin);
		freopen("CONOUT$", "w+t", stdout);

		m_bCreate = true;
	}
	else
	{
		m_hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
		if (m_hConsole == INVALID_HANDLE_VALUE)
			m_hConsole = NULL;
	}

	if (m_hConsole)
	{
		CONSOLE_SCREEN_BUFFER_INFO csbiInfo;  
		if (GetConsoleScreenBufferInfo(m_hConsole, &csbiInfo))
		{
			m_bAttrs = true;
			m_OldColorAttrs = csbiInfo.wAttributes;  
		}
		else
		{
			m_bAttrs = false;
			m_OldColorAttrs = 0;
		}

		SetConsoleCtrlHandler(&consoleHandlerRoutine, TRUE);

		return true;
	}

	return false;
}

void ConsoleWindow::close()
{
	if (m_bCreate)
	{
		FreeConsole();
		m_bCreate = false;
	}
}

#endif