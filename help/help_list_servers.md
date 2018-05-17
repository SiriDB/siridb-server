list servers
============

syntax

	list servers [columns] [where ...] [limit ...]

List servers in a SiriDB Cluster. This command can be useful to view status
information about a server.

columns
-------
Valid columns are:

- active_handles: Returns the active handles which can be used as an indicator on how busy a server is.
- active_tasks: Returns the active tasks for the current database.
- address: Server address.
- buffer_path: Path where this server keeps the buffer file.
- buffer_size: Size the server uses for one series in the buffer.
- dbpath: Path where the server stores the database.
- fifo_files: Number of fifo files which are used to update the replica server. This value is 0 if the server has no replica. A value greater than 1 could be an indication that replication is not working.
- idle_percentage: Returns percentage of idle time since the database was loaded.
- idle_time: Returns the idle time in seconds since the database was loaded.
- ip_support: IP Support setting on the server. (ALL/ IPV4ONLY/ IPV6ONLY)
- libuv: Version of libuv library.
- log_level: Current loglevel for the server.
- max\_open\_files: Returns the maximum open files value used for sharding on *this* server. (If this value is lower than expected, please check the log files for SiriDB as startup time)
- mem_usage: Shows memory usage for the server in MB's.
- name: Server name.
- online: True when the server is online.
- open_files: Number of open files for *this* database on the server.
- pool: Returns the pool ID for the server.
- port: Server port.
- received_points: Returns the number of received points by the server. On each restart of the SiriDB Server the counter will reset to 0. This value is only incremented when the server is receiving points from a client.
- reindex_progress: Returns the re-index status. Only available when the database is re-indexing series over pools.
- selected_points: Returns the selected points on the server. On each restart of the SiriDB Server the counter will reset to 0. This value includes all points which are read from the local shards and the points received from other servers to respond to a select query. The value is only incremented when the server received the select query from a client.
- startup_time: Time it takes to start the server.
- status: Current server status.
- sync_progress: Return synchronization status while creating a new replica server.
- uptime: Uptime in seconds.
- uuid: Server UUID (unique ID)
- version: SiriDB version

When no columns are provided the default is used. (name, pool, version, online, status)

examples
--------

	# list all servers in a SiriDB cluster.
	list servers

	# list all offline servers
	list servers where online == false

	# view memory usage and open files on all servers.
	list servers name, mem_usage, open_files


	# sample output (list servers)
	{
		"columns": ["name", "pool", "version", "online", "status"],
		"servers": [
			["siri1:9010", 0, "2.0.10", true, "running"],
			["siri2:9010", 1, "2.0.10", true, "running"]
		]
	}
