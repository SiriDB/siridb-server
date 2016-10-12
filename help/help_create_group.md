create group
============

Syntax:

	create group `groupname` for /regular_expression/ [set indexed true/false]
	
Groups should be between **backticks** to make them different from strings.
Since a group is basically a cached regular expressions we need to provide
the regular expression we want to use for the group. Optionally we can set
*indexed* to *true* which creates an indexed group. This is useful only if we
want to search with regular expressions inside the group. By default *indexed*
is false but its also possible to turn on/off indexing later. (see
`help alter group`)

If you want to drop an existing group see `help drop group`.

Example:

	# Create a goup linux
	create group `linux` for /linux.*/ set indexed true
	
	