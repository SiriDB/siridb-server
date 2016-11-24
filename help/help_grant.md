grant
=====

Syntax:

	grant <access> to user 'username'
	
Grants access rights to a user. For information about access rights
see `help access`.

Example:

	# Grant drop and create to user "iris"
	grant drop, create to user "iris"


Output:

	{"success_msg": "Successfully granted permissions to user ..."}
	