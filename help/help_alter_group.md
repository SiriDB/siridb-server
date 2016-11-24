alter group
===========

Syntax:

	alter group `groupname` set <option>
	
Valid options are *expression* and *name*.

set expression
--------------
Change the regular expression for a group.

Example:

	# Create group `linux`
	create group `linux` for /linux.*/

	# Change expression so we will match case insensitive
	alter group `linux` set expression /linux.*/i
	
set name
--------
Change the name for a group.

>**Note**
>
>This statement expects a normal string using single or double quotes. 
>The reason is that 'set name' expects a string and not a group.

Example:

	# Rename group `linux` to `ubuntu`
	alter group `linux` set name 'ubuntu'
	