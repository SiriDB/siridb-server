list shards
===========

syntax

	list shards [columns] [where ...] [limit ...]

columns
-------
Valid columns are:

- start: Start timestamp for the shard
- end: End timestamp for the shard
- sid: Shard identifier (the same sid usually exist on multiple servers).
- server: Server name on which the shard exists.
- pool: Pool where the shard in exists.
- status: Status flags for the shard.
- type: Type of the shard (number or log).
- size: Size of the shard. This is the total shard size over all pools. When a
    pool has more servers (replicas) the displayed size can vary when running
    this query multiple times because servers are responsible for optimizing
    their own shards and this could result in different shard sizes.

When no columns are provided the default is used. (sid, pool, server, start, end)

Example:

	# List all shards
	list shards

	# List shards used for data older then 100 days
	list shards where start < now - 100d

	# sample output (list shards)
	{
	    "columns": ["sid", "pool", "server", "start", "end"
	    ],
	    "shards": [
	        [1449705600, 0, "srv01:9010", 1449705600, 1450310400],
	        [1449705601, 0, "srv01:9010", 1449705600, 1450310400],
	        ...        
	    ]
	}
