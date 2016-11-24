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
    
    # Get the total number of points in the database
    count series length
    
	# Get number of series in group `group_server01`
	count series `group_server01`

	# Get number of points for series in pool 0
	count series length where pool == 0

    # Get the number of series which started in the last week
    count series where start > now - 1w and start <= now	


Example output (series):

	{"series": 1105946}
	

Example output (series length):
	
    {"series_length": 77450345251}