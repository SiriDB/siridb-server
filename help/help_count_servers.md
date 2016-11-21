count servers
=============

Syntax:

	count servers [received_points] [where ...]
	
Count servers returns the number of servers in a SiriDB cluster.
Received points are the number of points received be a server since uptime.
After a restart the counters are reset to zero.


>**Info**
>
>Received points only shows the number of points after uptime. For the total
>number of points you can use `count series length`

Examples:

	# Get number of servers
	count servers 
	
	# Get number of servers in pool 0
	count servers where pool == 0
	
	# Get total received points since uptime
	count servers received_points

Example output:

	{"count": 6}