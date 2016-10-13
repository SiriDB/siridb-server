count series
============

Syntax:

	count series [match_series] [where ...]
	
Count series in the SiriDB cluster. For more information about how to select 
series see `help list series`

Examples:

	# Get number of series
	count series 
	
	# Get number of series in group `group_server01`
	count series `group_server01`

	# Get number of series in pool 0
	count series where pool == 0

Example output:

	{"count": 1105946}