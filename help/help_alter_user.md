alter user
==========

Syntax:

	alter user 'username' set <option>
	
Valid options are *password* and *name*.

Change a users password.

Example:

	# Change the password for "iris" to "siri"
	alter user 'iris' set password 'siri'
