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
	}

	lua_State	*L;
	int			t;
};


static void packetExecute(Service* service, lua_State* L, PckHeader* hd, bool bForceOnlySaveIt)
{
	AutoRestoreTop art(L);

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

	lua_getfield(L, tblIdx, "tasks");
	int tasksIdx = lua_gettop(L);
	if (!lua_istable(L, tasksIdx))
	{
		LOGFMTE("Packet Json have no task [bodylength=%u]", hd->bodyLeng);
		return ;
	}

	// 先构造SQL保存下来这个计划
	DBSqlite::Value vals1[] = {
		{ DBSqlite::kValueInt, 0, { (int)kScheduleRepeat } },
		{ DBSqlite::kValueInt, 0, { 0 } },
		{ DBSqlite::kValueInt, 0, { 0 } },
		{ DBSqlite::kValueInt, 0, { 0 } },
		{ DBSqlite::kValueNull, 0, { 0 } },
	};

	service->db->exec("INSERT INTO schedules(create_time,mode,repeat_interval,repeat_times,weekdays,day_time_ranges) VALUES(datetime('now','localtime'),?,?,?,?,?)", vals1, sizeof(vals1) / sizeof(vals1[0]));
	uint64_t insertId = service->db->getLastInsertId();

	// 然后保存任务
	DBSqlite::Value vals2[] = {
		{ DBSqlite::kValueText, 0, { (int)insertId } },
		{ DBSqlite::kValueText, 0, { 0 } },
		{ DBSqlite::kValueText, 0, { 0 } },
	};

	int validcc = 0;
	for(int iTask = 1; ; ++ iTask)
	{
		lua_rawgeti(L, tasksIdx, iTask);
		if (!lua_istable(L, -1))
			break;

		lua_getfield(L, -1, "url");
		lua_getfield(L, -2, "posts");

		size_t urllen = 0, postslen = 0;
		const char* url = luaL_optlstring(L, -2, "", &urllen);
		const char* posts = luaL_optlstring(L, -1, "", &postslen);

		if (url)
		{
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
				validcc ++;
		}
	}

	if (!validcc)
	{
		char delsql[128] = { 0 };
		sprintf(delsql, "DELETE FROM schedules WHERE schid=%llu", insertId);
		service->db->exec(delsql);

		LOGFMTE("Packet Json have no task after parsed [bodylength=%u]", hd->bodyLeng);
		return ;
	}

	if (bForceOnlySaveIt)
		return ;
}