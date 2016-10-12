grant
=====

Syntax:

	grant <access> to user/network ...
	
Grants access rights to a user or network. For information about access rights
see `help access`.

Example:

	# Grant drop and create to user "siri"
	grant drop, create to user "siri"
	
	# Grant modify to nerwork "192.168.10.0/24"
	grant modify to network "192.168.10.0/24"


Output:

	{"success_msg": "Successful granted ... permissions to ..."}
	