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
	ScriptTask scripttmp;
	HTTPRequestTask httpreqtmp;	
	ScheduleData* s = art.s = bForceOnlySaveIt ? 0 : new ScheduleData();

	lua_pushcfunction(L, &lua_string_json);
	lua_pushlstring(L, (char*)(hd + 1), hd->bodyLeng);
	lua_pushnil(L);
	lua_pushboolean(L, 0);
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
	if (luaL_optboolean(L, -1, false))
		s->flags |= kSFlagCPUFreed;

	lua_getfield(L, tblIdx, "halt_result");
	if (luaL_optboolean(L, -1, false))
		s->flags |= kSFlagHaltWhenResultNotMatched;

	lua_getfield(L, tblIdx, "halt_anyerrors");
	if (luaL_optboolean(L, -1, false))
		s->flags |= kSFlagHaltWhenHttpRequestFailed;


	// 构造SQL保存下来这个计划
	DBSqlite::Value vals1[] = {
		{ DBSqlite::kValueInt, 0, { (int)kScheduleRepeat } },
		{ DBSqlite::kValueInt, 0, { 0 } },
		{ DBSqlite::kValueInt, 0, { 0 } },
		{ DBSqlite::kValueInt, 0, { 0 } },
		{ DBSqlite::kValueNull, 0, { 0 } },
	};

	servoce->db->action(DBSqlite::kBegin);
	service->db->exec("INSERT INTO schedules(create_time,mode,repeat_interval,repeat_times,weekdays,day_time_ranges) VALUES(datetime('now','localtime'),?,?,?,?,?)", vals1, sizeof(vals1) / sizeof(vals1[0]));
	uint64_t insertId = service->db->getLastInsertId();

	// 然后保存所有的任务
	DBSqlite::Value vals2[] = {
		{ DBSqlite::kValueText, 0, { (int)insertId } },
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
			lua_getfield(L, -1, "url");
			lua_getfield(L, -2, "posts");
			lua_getfield(L, -2, "downto");
			lua_getfield(L, -2, "result");

			size_t urllen = 0, postslen = 0, downtolen = 0, resultlen = 0;
			const char* url = luaL_optlstring(L, -2, "", &urllen);
			const char* posts = luaL_optlstring(L, -1, "", &postslen);
			const char* downto = luaL_optlstring(L, -1, "", &downtolen);
			const char* result = luaL_optlstring(L, -1, "", &resultlen);

			if (postslen == 0)
			{
				// 可能是用base64编码了post数据
				lua_getfield(L, -2, "b64posts");
				posts = luaL_optlstring(L, -1, "", &postslen);
			}

			if (url)
			{
				// 至少要有URL才能认为有效
				vals2[1].len = urllen;
				vals2[1].v.s = url;
				if (postslen > 1)
				{
					vals2[2].type = DBSqlite::kValueText;
					vals2[2].len = postslen;
					vals2[2].v.s = posts;
				}
				else
				{
					vals2[2].type = DBSqlite::kValueNull;
				}

				if (service->db->exec("INSERT INTO httpreq_tasks(schid,url,posts) VALUES(?,?,?)", vals2, sizeof(vals2) / sizeof(vals2[0])))
				{
					if (s)
					{
						// 记录任务
						s.tasks.push_back(httpreqtmp);
						HTTPRequestTask& httpreq = s.tasks.back();

						httpreq.strUrl.append(url, urllen);
						httpreq.strPosts.append(posts, postslen);
						httpreq.strDownloadTo.append(downto, downtolen);
						httpreq.strResultForce.append(result, resultlen);
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

	if (validcc)
	{
		servoce->db->action(DBSqlite::kRollback);
		LOGFMTE("Packet Json have no task after parsed [bodylength=%u]", hd->bodyLeng);
		return ;
	}

	servoce->db->action(DBSqlite::kCommit);

	if (bForceOnlySaveIt)
		return ;

	// 将这个计划现在就添加到队列
	service->addSchedule(s);

	art.s = NULL;
}