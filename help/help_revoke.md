revoke
======

Syntax:

	revoke <access> from user/network ...
	
Revokes access rights from a user or network. For available access rights refer to `help access`.

>**Warning**
>
>If accidentally all access rights for all users are gone, you need to recover 
>the default user. See `help noaccess` for how to recover from a situation
>not having access to SiriDB.

Example:

	# Revoke drop and create from user "siri"
	revoke drop, create from user "siri"
	
	# Revoke modify from nerwork "192.168.10.0/24"
	revoke modify from network "192.168.10.0/24"	
	
Output:

	{"success_msg": "Successful revoked ... permissions from ..."}