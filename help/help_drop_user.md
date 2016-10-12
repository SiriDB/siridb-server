drop user
=========

Syntax:

	drop user `username`
	
Drops an existing user.

>**Warning**
>
>If accidentally all users with access are dropped, you need to recover the
>default user. See `help noaccess` for how to recover from a situation
>not having access to SiriDB.

Example:

	# Drop the default user "siri"
	drop user "siri"
	
