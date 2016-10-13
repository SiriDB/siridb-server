list groups
===========

syntax

	list groups [columns] [where ...] [limit ...]

columns
-------
Valid columns are:

- name: Group name
- series: Number of series in the group. This value included series in other 
  	pools, except when viewing a cached group since they only exist on the server
  	which processes the command.
- indexed: true/false depending if the group is indexed
- cached: true/false depending if this is a cached regular expression
- expression: Show the expression used for this group.

When no columns are provided the default is used. (name, series, indexed) 

Examples:

	# View all groups (cached groups are excluded)
	list groups
	
	# View all cached groups
	list groups where cached == true
	
	# View groups and the expression used
	list groups name, expression
	
	