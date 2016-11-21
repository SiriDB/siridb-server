count users
===========

Syntax:

	count users [where ...]
	
Count users returns the number of users.

Examples:

	# Get number of users
	count users 
	
	# Get number of users not equal to 'iris'
	count users where name != 'iris'

Example output:

	{"users": 6}