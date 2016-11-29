list servers
============

syntax

	list servers [columns] [where ...] [limit ...]
	
List servers in a SiriDB Cluster. This command can be useful to view status
information about a server. 

columns
-------
Valid columns are:

- active_handles: returns the active handles which can be used as an indicator for how busy a server is.
- address: Server address.
- buffer_path: Path where this server keeps the buffer file.
- buffer_size: Size the server uses for one series in the buffer.
- dbpath: Path where the server stores the database.
- ip_support: IP Support setting on the server. (ALL/ IPV4ONLY/ IPV6ONLY)
- libuv: Version of libuv library.
- log_level: Current loglevel for the server.
- max\_open\_files: returns the maximum open files value used for sharding on *this* server. (If this value is lower than expected, please check the log files for SiriDB as stratup time)
- mem_usage: Shows memory usage for the server in MB's.
- name: Server name.
- online: true when te server is online.
- open_files: Number of open files for *this* database on the server.
- pool: returns the pool ID for the server.
- port: Server port.
- received_points: returns the received points on the server. A restart of the SiriDB Server will reset the counter to 0.
- reindex_progress: returns the re-index status. Only available when the database is re-indexing series over pools.
- startup_time: time it took to start the server.
- status: current server status.
- sync_progress: return synchronization status while creating a new replica server.
- uptime: uptime in seconds.
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