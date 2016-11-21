count shards
============

Syntax:

	count shards [where ...]
	
Count shards returns the number of shards in the SiriDB cluster. When using
multiple pools with replicas this answer can be different depending on which
server is answering.
This difference can happen because one server can have points for series in its
buffer while the replica has send the points to shards. Note that the
difference can be even bigger when servers in a cluster are using different
buffer sizes. (view buffer size on each server:
`list servers name, buffer_size`)

Example:

	# Get number of shards
	count shards 
	
	# Get number of shards which are larger then 1GB
	count shards where size > 1024 ** 3

Example output:

	{"shards": 51}