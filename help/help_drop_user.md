drop user
=========

Syntax:

	drop user `username`

Drops an existing user.

>**Warning**
>
>If accidentally all users with access are dropped, you need to recover the
>default user. See `help noaccess` on how to recover from a situation in which you
>don't have access to SiriDB.

Example:

	# Drop the default user "siri"
	drop user "siri"
