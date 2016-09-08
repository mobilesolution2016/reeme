const char SQLCreateTable_Schedule[] = {
	"CREATE TABLE schedules("
	"	schid INTEGER PRIMARY KEY ASC AUTOINCREMENT,"					// 计划ID
	"	create_time DATETIME,"											// 创建这条计划的时间

	"	mode INTEGER,"													// 任务执行模式，可用的值定义在枚举ScheduleMode中
	"	flags INTEGER DEFAULT 0,"										// 任务的一些配置的组合标志

	"	repeat_interval UNSIGNED INT DEFAULT 0,"						// 重复执行计划的间隔时间（单位：秒）
	"	repeat_times UNSIGNED INT DEFAULT 0,"							// 重复执行的次数，0表示不重复执行。重复执行指的是达到执行条件之后

	"	weekdays UNSIGNED INT DEFAULT 0,"								// 每周的哪些天执行，1表示周1，2表示周二，周日是7，使用位标识 1 << day来组合7天
	"	day_time_ranges TEXT DEFAULT ''"								// 每天执行的时间范围，这是一个数组，每一行表示一天，第0行是周一，第6行是周日，每天可以有多个允许执行的时间段，每个时间段以,分隔，每个时间段的写法：00:00-23:59
	")"
};

const char SQLCreateTable_HttpReqTasks[] = {
	"CREATE TABLE httpreq_tasks("
	"	taskid INTEGER PRIMARY KEY ASC AUTOINCREMENT,"
	"	schid INTEGER DEFAULT 0,"
	"	url VARCHAR(280) DEFAULT '',"
	"	download_to VARCHAR(280) DEFAULT NULL,"
	"	posts TEXT DFAULT NULL,"	
	"	result_force TEXT DFAULT NULL"	
	")"
};

const char SQLCreateTable_ScriptRunTasks[] = {
	"CREATE TABLE scriptrun_tasks("
	"	taskid INTEGER PRIMARY KEY ASC AUTOINCREMENT,"
	"	schid INTEGER DEFAULT 0,"
	"	script_file VARCHAR(280) DEFAULT NULL,"
	"	script_codes TEXT DFAULT NULL"
	")"
};

