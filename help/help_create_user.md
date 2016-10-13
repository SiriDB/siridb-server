create user
===========

Syntax:

	create user 'my-username' set password 'my-password'
	
Create a new user. This will create a new user without access to SiriDB.
For more information on how to grant access to a user see `help grant`.

Example:
	
	# This will create a new user 'iris' with password 'siri'
	create user 'iris' set password 'siri'

	# Grant "read" access to "iris"
	grant read to user "iris"
	