count pools
===========

Syntax:

	count pools [where ...]
	
Count pools returns the number of pools in the SiriDB cluster.

Examples:

	# Get number of pools
	count pools 
	
	# Get number of pools with less or equal to 100 series in the pool
	count pools where series <= 100

Example output:

	{"count": 2}