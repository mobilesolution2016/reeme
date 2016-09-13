struct AutoRestoreTop
{
	AutoRestoreTop(lua_State* s)
	{
		L = s;
		t = lua_gettop(L);
	}
	~AutoRestoreTop()
	{
		lua_settop(L, t);
		if (s)
			delete s;
	}

	lua_State		*L;
	ScheduleData	*s;
	int				t;
};


static void packetExecute(Service* service, lua_State* L, PckHeader* hd, bool bForceOnlySaveIt)
{
	AutoRestoreTop art(L);
	ScheduleData* s = art.s = bForceOnlySaveIt ? 0 : new ScheduleData();

	lua_pushcfunction(L, &lua_string_json);
	lua_pushlstring(L, (char*)(hd + 1), hd->bodyLeng);
	lua_pushnil(L);
	lua_pushinteger(L, 0x80000000);
	lua_pcall(L, 3, 1, 0);
	int tblIdx = lua_gettop(L);
	if (!lua_istable(L, tblIdx))
	{
		std::string strJson;
		size_t summaryLeng = std::min(256U, hd->bodyLeng);
		strJson.append((char*)(hd + 1), summaryLeng);

		LOGFMTE("Packet Json Parsed Failed [bodylength=%u] Json summary:\n%s", hd->bodyLeng, strJson.c_str());
		return ;
	}

	// 取任务数组
	lua_getfield(L, tblIdx, "tasks");
	int tasksIdx = lua_gettop(L);
	if (!lua_istable(L, tasksIdx) || lua_objlen(L, -1) == 0)
	{
		LOGFMTE("Packet Json have no task [bodylength=%u]", hd->bodyLeng);
		return ;
	}

	// 取模式设置，并根据不同的配置来读取不同的参数
	lua_getfield(L, tblIdx, "mode");
	const char* mode = luaL_optstring(L, -1, "immediate");

	if (strcmp(mode, "immediate") == 0)
	{
		s->mode = kScheduleImmediate;
	}
	else if (strcmp(mode, "daycycles") == 0)
	{
		s->mode = kScheduleDayCycles;
	}
	else if (strcmp(mode, "interval") == 0)
	{
		s->mode = kScheduleInterval;
	}
	else
	{
		LOGFMTE("Packet Json with invalid mode: %s", mode);
		return ;
	}

	// 取首次运行的时间点
	lua_getfield(L, tblIdx, "start");
	const char* startDatetime = luaL_optstring(L, -1, NULL);
	if (startDatetime)
	{
	}

	// 取其它标志位配置
	lua_getfield(L, tblIdx, "cpufreed");
	if (lua_toboolean(L, -1))
		s->flags |= kSFlagCPUFreed;

	lua_getfield(L, tblIdx, "halt_result");
	if (lua_toboolean(L, -1))
		s->flags |= kSFlagHaltWhenResultNotMatched;

	lua_getfield(L, tblIdx, "halt_anyerrors");
	if (lua_toboolean(L, -1))
		s->flags |= kSFlagHaltWhenHttpRequestFailed;

	lua_getfield(L, tblIdx, "complete_withall");
	if (lua_toboolean(L, -1))
		s->flags |= kSFlagCompleteWithAllTasks;


	// 构造SQL保存下来这个计划
	DBSqlite::Value vals1[] = {
		{ DBSqlite::kValueInt, 0, { (int)kScheduleImmediate } },
		{ DBSqlite::kValueInt, 0, { 0 } },
		{ DBSqlite::kValueInt, 0, { 0 } },
		{ DBSqlite::kValueInt, 0, { 0 } },
		{ DBSqlite::kValueNull, 0, { 0 } },
	};

	service->db->action(DBSqlite::kBegin);
	service->db->exec("INSERT INTO schedules(create_time,mode,repeat_interval,repeat_times,weekdays,day_time_ranges) VALUES(datetime('now','localtime'),?,?,?,?,?)", vals1, sizeof(vals1) / sizeof(vals1[0]));
	s->scheduleId = service->db->getLastInsertId();

	// 然后保存所有的任务
	DBSqlite::Value vals2[] = {
		{ DBSqlite::kValueInt, 8, { s->scheduleId } },
		{ DBSqlite::kValueText, 0, { 0 } },
		{ DBSqlite::kValueText, 0, { 0 } },
		{ DBSqlite::kValueText, 0, { 0 } },
		{ DBSqlite::kValueText, 0, { 0 } },
	};

	int validcc = 0;	
	lua_settop(L, tasksIdx);
	for(int iTask = 1; ; ++ iTask)
	{
		lua_rawgeti(L, tasksIdx, iTask);
		if (!lua_istable(L, -1))
			break;

		// 先获取任务的类型
		lua_getfield(L, -1, "type");

		size_t typelen = 0;
		const char* type = luaL_optlstring(L, -1, "", &typelen);
		if (!type || typelen < 3)
			continue;

		if (strcmp(type, "httpreq") == 0)
		{
			// http请求任务
			lua_getfield(L, tasksIdx + 1, "url");
			lua_getfield(L, tasksIdx + 1, "posts");
			lua_getfield(L, tasksIdx + 1, "downto");
			lua_getfield(L, tasksIdx + 1, "result");

			size_t urllen = 0, postslen = 0, downtolen = 0, resultlen = 0;
			const char* url = luaL_optlstring(L, -4, "", &urllen);
			const char* posts = luaL_optlstring(L, -3, "", &postslen);
			const char* downto = luaL_optlstring(L, -2, "", &downtolen);
			const char* result = luaL_optlstring(L, -1, "", &resultlen);

			if (postslen == 0)
			{
				// 可能是用base64编码了post数据
				lua_getfield(L, tasksIdx + 1, "b64posts");
				posts = luaL_optlstring(L, -1, "", &postslen);
			}

			if (url)
			{
				// 至少要有URL才能认为有效
				vals2[1].make(url, urllen);
				if (postslen > 1)
					vals2[2].make(posts, postslen);
				else
					vals2[2].make();
				if (downtolen > 1)
					vals2[3].make(downto, downtolen);
				else
					vals2[3].make();
				if (resultlen > 1)
					vals2[4].make(result, resultlen);
				else
					vals2[4].make();

				if (service->db->exec("INSERT INTO httpreq_tasks(schid,url,posts,download_to,result_force) VALUES(?,?,?,?,?)", vals2, sizeof(vals2) / sizeof(vals2[0])))
				{
					if (s)
					{
						// 记录任务
						HTTPRequestTask* httpreq = s->hpool.newElement();
						s->tasks.append(httpreq);

						httpreq->strUrl.append(url, urllen);
						httpreq->strPosts.append(posts, postslen);
						httpreq->strDownloadTo.append(downto, downtolen);
						httpreq->strResultForce.append(result, resultlen);
					}
					validcc ++;
				}
			}
		}
		else if (strcmp(type, "script") == 0)
		{
			// 脚本任务
		}
		else
			LOGFMTE("Packet Json have invalid task type: %s", type);

		lua_settop(L, tasksIdx);
	}

	if (validcc == 0)
	{
		service->db->action(DBSqlite::kRollback);
		LOGFMTE("Packet Json have no task after parsed [bodylength=%u]", hd->bodyLeng);
		return ;
	}

	service->db->action(DBSqlite::kCommit);

	if (bForceOnlySaveIt)
		return ;

	// 将这个计划现在就添加到队列
	service->addSchedule(s);

	art.s = NULL;
}


static void scheduleLogHttpReq(ScheduleData* sch, HTTPRequestTask* task, CURLcode code, FILE* fp, std::string* pstrResult)
{	
	if (pstrResult && task->strResultForce.length() && task->strResultForce != *pstrResult)
	{
		LOGFMTE("Schedule [id=%llu] task index [%u] responses not equal to result forced", sch->scheduleId, sch->runIndex, task->strResultForce.c_str(), pstrResult->c_str());
		return ;
	}

	// 检测code值并输出相应的Log
}

static void scheduleLogScriptRun(ScheduleData* sch, lua_State* L, ScriptTask* task, int Lr)
{	
	if (Lr)
	{
		// 报错
		LOGFMTE("Schedule [id=%llu] task index [%u] do script code failed: %s", sch->scheduleId, sch->runIndex, lua_tostring(L, -1));
		return ;
	}
}


static void scheduleExecute(Service* service, lua_State* L, ScheduleData* sch)
{
	// 查询该任务在数据库中是否存在，以及其状态
	DBSqlite::Value bindValues[5];

	bindValues[0].make(sch->scheduleId);
	DBSqlite::Result* schResult = service->db->query("SELECT status FROM schedules WHERE schid=?", bindValues, 1, DBSqlite::kIndexNum);
	if (!schResult)
	{
		LOGFMTW("Schedule [id=%u] need execute but cannot find its db record", sch->scheduleId);
		return ;
	}

	int schStatus = schResult->getInt(0);
	if (schStatus != kScheduleReady)
	{
		LOGFMTW("Schedule [id=%u] need execute but the status [%d] is not ready", sch->scheduleId, schStatus);
		return ;
	}

	LOGFMTI("Schedule [id=%u] executin started", sch->scheduleId);

	// 更改状态为执行
	service->db->exec("UPDATE schedules SET status=1 WHERE schid=?", bindValues, 1);

	// 执行所有的task	
	TaskDataBase* next;
	uint32_t taskOks = 0, taskTotal = sch->tasks.size();

	sch->runIndex = 1;
	while ((next = sch->tasks.popFirst()) != NULL)
	{
		if (next->taskType == TaskDataBase::kTaskHttpReq)
		{
			// 发出HTTP请求的任务
			HTTPRequestTask* task = (HTTPRequestTask*)next;

			CURL* c;
			CURLcode code;
			std::string strResult;
			std::string& strUrl = task->strUrl;

			if (task->strPosts.length())
				c = curlInitOne(strUrl.c_str(), task->strPosts.c_str());
			else
				c = curlInitOne(strUrl.c_str());

			if (task->strDownloadTo.length())
			{
				FILE* fp = fopen(task->strDownloadTo.c_str(), "wb");
				if (!fp)
				{
					LOGFMTE("Schedule [id=%u] task index [%u] create download to file [%s] failed", sch->scheduleId, sch->runIndex, task->strDownloadTo.c_str());
					goto _end_hr;
				}

				code = curlExecOne(c, fp);
				fflush(fp);
				rewind(fp);

				scheduleLogHttpReq(sch, task, code, fp, NULL);
				fclose(fp);
			}
			else
			{
				code = curlExecOne(c, strResult);
				scheduleLogHttpReq(sch, task, code, NULL, &strResult);
			}

			taskOks ++;

		_end_hr:
			if (!(sch->flags & kSFlagCompleteWithAllTasks))
			{
				// 执行完一条删除一条
				bindValues[0].make(sch->scheduleId);
				service->db->exec("DELETE FROM httpreq_tasks WHERE taskid=?", bindValues, 1);
			}
			task->~HTTPRequestTask();
		}
		else if (next->taskType == TaskDataBase::kTaskScript)
		{
			// 执行脚本的任务
			ScriptTask* task = (ScriptTask*)next;
			int top = lua_gettop(L);

			if (task->bDataIsFilename)
			{
				FILE* fp = fopen(task->strData.c_str(), "rb");
				if (!fp)
				{
					LOGFMTE("Schedule [id=%u] task index [%u] open script file [%s] failed", sch->scheduleId, sch->runIndex, task->strData.c_str());
					goto _end_st;
				}

				fseek(fp, 0L, SEEK_END);
				long size = ftell(fp);
				fseek(fp, 0L, SEEK_SET);

				task->strData.resize(size);
				fread(const_cast<char*>(task->strData.c_str()), 1, size, fp);
				fclose(fp);
			}

			int Lr = luaL_dostring(L, task->strData.c_str());
			scheduleLogScriptRun(sch, L, task, Lr);
			taskOks ++;

		_end_st:
			if (!(sch->flags & kSFlagCompleteWithAllTasks))
			{
				// 执行完一条删除一条
				bindValues[0].make(sch->scheduleId);
				service->db->exec("DELETE FROM scriptrun_tasks WHERE taskid=?", bindValues, 1);
			}

			lua_settop(L, top);
			task->~ScriptTask();
		}

		++ sch->runIndex;
	}

	// 删除记录
	bindValues[0].make(sch->scheduleId);
	service->db->exec("DELETE FROM schedules WHERE schid=?", bindValues, 1);

	if (sch->flags & kSFlagCompleteWithAllTasks)
	{
		// 全部执行完了一次性删除
		bindValues[0].make(sch->scheduleId);
		service->db->exec("DELETE FROM scriptrun_tasks WHERE schid=?", bindValues, 1);
	}

	LOGFMTI("Schedule [id=%u] executin completed, task ok=%u, total=%u", sch->scheduleId, taskOks, taskTotal);
}