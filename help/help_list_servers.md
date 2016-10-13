list servers
============

syntax

	list servers [columns] [where ...] [limit ...]
	
List servers in a SiriDB Cluster. This command can be useful to view status
information about a server. 

columns
-------
Valid columns are:

- address: Server address.
- buffer_path: Path where this server keeps the buffer file.
- buffer_size: Size the server uses for one series in the buffer.
- dbpath: Path where the server stores the database.
- debug: true when debug mode is enabled on the server.
- loglevel: Current loglevel for the server.
- manhole: Shows the manhole port for the server when installed.
- max\_open\_files: returns an array [*x*, *y*] where *x* is the maximum open files setting and *y* is the actual value used by the server. (*y* can be lower in case the limit for open files is reached)
- mem_usage: Shows memory usage for the server.
- name: Server name.
- online: true when te server is online.
- open_files: Number of open files for *this* database on the server.
- optimize_task: returns information about the shard optimization task on the server.
- pool: returns the pool ID for the server.
- port: Server port.
- received_points: returns the received points on the server. A restart of the SiriDB Server will reset the counter to 0.
- reindex_progress: returns the re-index status. Only available when the database is re-indexing series over pools.
- sharding_buffering: returns the sharding buffering settings.
- show sharding\_max_chunk_points: maximum number of points the server should keeps in a chunk. 
- startup_time: time it took to start the server.
- status: current server status.
- sync_progress: return synchronization status while creating a new replica server.
- task_queue: returns an array [*x*, *y*] where *x* is the number of running insert and query tasks on *this* server for the current database and *y* is the total number of tasks running on the server.
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
	
	# view memory usage and number of running tasks on all servers.
	list servers name, mem_usage, task_queue


	# sample output (list servers)
	{
		"columns": ["name", "pool", "version", "online", "status"],
		"servers": [
			["siri1:9010", 0, "0.9.14", true, "running"], 
			["siri2:9010", 1, "0.9.14", true, "running"]
		]
	}