count series
============

Syntax:

	count series [length] [match_series] [where ...]
	
Count series in the SiriDB cluster. For more information about how to select 
series see `help list series`

Count series length gives the total number of points for the selected series.

Examples:

	# Get number of series
	count series 
	
	# Get number of series in group `group_server01`
	count series `group_server01`

	# Get number of series in pool 0
	count series where pool == 0
	
	# Get the total number of points in the database
	count series length


Example output:

	{"series": 1105946}