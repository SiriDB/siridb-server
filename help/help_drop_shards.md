drop shards
===========

Syntax:

	drop shards [where ...] [set ignore_threshold true/false]
	
Drops an existing shard using the shard id (sid). Use `list shards` for an 
overview of the current shards. 

Example:

	# Drop shard with sid 1446076800
	drop shard 1446076800