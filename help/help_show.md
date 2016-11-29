show
====

Syntax

	show [<option> [,<option> [...]]

See available options for more info on each show command:

- `show active_handles`: returns the active handles which can be used as an indicator for how busy a server is.
- `show buffer_path`: returns the local buffer path on *this* server.
- `show buffer_size`: returns the buffer size in bytes on *this* server.
- `show dbname`: returns the database name.
- `show dbpath`: returns the local database path on *this* server.
- `show drop_threshold`: return the current drop threshold. (value between 0 and 1 representing a percentage)
- `show duration_log`: returns the sharding duration for log data on *this* database. (not supported yet)
- `show duration_num`: returns the sharding duration for num data on *this* database.
- `show ip_support`: returns the ip support setting on *this* server.
- `show libuv`: returns the version of libuv on *this* server.
- `show log_level`: returns the current log level for *this* server.
- `show max_open_files`: returns the maximum open files value used for sharding on *this* server. (If this value is lower than expected, please check the log files for SiriDB as stratup time)
- `show mem_usage`: returns the current memory usage in MB's on *this* server.
- `show open_files`: returns the number of open files on *this* server for the selected database. (should be 0 when the server is in backup_mode)
- `show pool`: returns the pool ID for *this* server.
- `show received_points`: returns the received points for *this* server. On each restart of the SiriDB Server the counter will reset to 0.
- `show reindex_progress`: returns the re-index status on *this* server. Only available when the database is re-indexing series over pools.
- `show server`: returns *this* server name. The name has format *host:port*
- `show startup_time`: returns the time in seconds it took to startup the SiriDB database on *this* server.
- `show status`: returns the current status for *this* server.
- `show sync_progress`: return synchronization status while creating a new replica server on *this* server.
- `show time_precision`: returns the time precision for *this* database.
- `show timezone`: returns the timezone for *this* database.
- `show uptime`: returns the uptime in seconds *this* server is running.
- `show uuid`: returns the UUID (unique ID) for *this* server.
- `show version`: returns the SiriDB version running on *this* server.
- `show who_am_i`: returns the user who is running *this* request.

examples
--------
	
	# show the database name and time precision
	show dbname, time_precision


	# sample output
	{
		"data": [
			{
				"name": "dbname",
				"value": "mydb"
			}, 
			{
				"name": "time_precision",
				"value": "s"
			}
		]
	}