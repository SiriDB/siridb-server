show
====

Syntax

	show [<option> [,<option> [...]]

See available options for more info on each show command:

- `show active_handles`: Returns the active handles which can be used as an indicator for how busy a server is.
- `show buffer_path`: Returns the local buffer path on *this* server.
- `show buffer_size`: Returns the buffer size in bytes on *this* server.
- `show dbname`: Returns the database name.
- `show dbpath`: Returns the local database path on *this* server.
- `show drop_threshold`: Returns the current drop threshold (value between 0 and 1 representing a percentage).
- `show duration_log`: Returns the sharding duration for log data on *this* database (not supported yet).
- `show duration_num`: Returns the sharding duration for num data on *this* database.
- `show fifo_files`: Returns the number of fifo files which are used to update the replica server. This value is 0 if the server has no replica. A value greater than 1 could be an indication that replication is not working.
- `show ip_support`: Returns the ip support setting on *this* server.
- `show libuv`: Returns the version of libuv on *this* server.
- `show list_limit`: Returns the maximum value which can be used as limit in a list query.
- `show log_level`: Returns the current log level for *this* server.
- `show max_open_files`: Returns the maximum open files value used for sharding on *this* server (if this value is lower than expected, please check the log files for SiriDB as startup time).
- `show mem_usage`: Returns the current memory usage in MB's on *this* server.
- `show open_files`: Returns the number of open files on *this* server for the selected database (should be 0 when the server is in backup_mode).
- `show pool`: Returns the pool ID for *this* server.
- `show received_points`: Returns the number of received points for *this* server. On each restart of the SiriDB Server the counter will reset to 0. This value is only incremented when *this* server is receiving points from a client.
- `show reindex_progress`: Returns the re-index status on *this* server. Only available when the database is re-indexing series over pools.
- `show selected_points`: Returns the selected points for *this* server. On each restart of the SiriDB Server the counter will reset to 0. This value includes all points which are read from the local shards and the points received from other servers to respond to a select query. The value is only incremented when *this* server received the select query from a client.
- `show select_points_limit`: Returns the maximum number of points which can be returned with a select query.
- `show server`: Returns *this* server name. The name has format *host:port*
- `show startup_time`: Returns the time in seconds it took to startup the SiriDB database on *this* server.
- `show status`: Returns the current status for *this* server.
- `show sync_progress`: Return synchronization status while creating a new replica server on *this* server.
- `show time_precision`: Returns the time precision for *this* database.
- `show timezone`: Returns the timezone for *this* database.
- `show uptime`: Returns the uptime in seconds *this* server is running.
- `show uuid`: Returns the UUID (unique ID) for *this* server.
- `show version`: Returns the SiriDB version running on *this* server.
- `show who_am_i`: Returns the user who is running *this* request.

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
