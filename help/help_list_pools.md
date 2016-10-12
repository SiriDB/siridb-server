list pools
==========

syntax

	list pools [columns] [where ...] [limit ...]

columns
-------
Valid columns are:

- pool: Pool ID
- servers: Number of servers in the pool.
- series: Number of series in the pool.

When no columns are provided the default is used. (pool, servers, series)

Example:

	# View pools
	list pools
	