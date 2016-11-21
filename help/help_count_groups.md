count groups
============

Syntax:

	count groups [where ...]
	
Count groups returns the number of groups defined in *this* database. Servers 
in a SiriDB cluster can have cached groups for regular expression but since 
these cached groups can be different on each server they are always excluded 
from count groups. 

If you want to view the cached groups on a server, use 
`list groups where cached == true`.

Example:

	# Get number of groups
	count groups 
	
	# Get number of groups with more than 100 series
	count groups where series > 100  
	
	# Get number of indexed groups with more than 100 series
	count groups where series > 100 and indexed == true

Example output:

	{"groups": 23}