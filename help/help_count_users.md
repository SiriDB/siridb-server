count users
===========

Syntax:

	count users [where ...]
	
Count users returns the number of users.

Examples:

	# Get number of users
	count users 
	
	# Get number of user not equal to 'iris'
	count users where user != 'iris'

Example output:

	{"count": 6}