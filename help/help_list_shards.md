list shards
===========

syntax

	list shards [columns] [where ...] [limit ...]

columns
-------
Valid columns are:

- sid: Shard ID
- start: Start date for the shard
- end: End date for the shard
- size: Size of the shard. This is the total shard size over all pools. When a
    pool has more servers (replicas) the displayed size can vary when running
    this query multiple times because servers are responsible for optimizing 
    their own shards and this could result in different shard sizes.

When no columns are provided the default is used. (sid, start, end, size)

Example:

	# List all shards
	list shards
	
	# List shards used for data older then 100 days
	list shards where start < now - 100d
	