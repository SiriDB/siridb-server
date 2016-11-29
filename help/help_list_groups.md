list groups
===========

syntax

	list groups [columns] [where ...] [limit ...]

columns
-------
Valid columns are:

- name: Group name
- series: Number of series in the group.
- expression: Show the expression used for this group.

When no columns are provided the default is used. (name) 

Examples:

	# View all groups
	list groups
	
	# View groups and the expression used
	list groups name, expression
	
	# sample output (list groups)
	{
		"columns": ["name"],
		"groups": [
			["linux"], 
			["windows"]
		]
	}	
	