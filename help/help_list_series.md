list series
===========

syntax

	list series [columns] [match_series] [where ...] [limit ...]

columns
-------
Valid columns are:

- name: Series name
- pool: Pool where the series is assigned to
- start: Time-stamp of the first value in the series
- end: Time-stamp of the last value in the series
- length: The number of points in a series
- type: The series type. ("integer" or "float")

When no columns are provided the default is used. (name)

match series
------------

Series can be matched using different methods. Groups and especially indexed groups can help to quickly get the required series even in a database with over a million unique series.


syntax:

	<series_name | regular_expression | group> [update_function <match_series>]
	
series name
-----------
A series name is a string containing the series name. An error is raised when the given series name is not found in SiriDB. (Note that this exception
is not raised when the database is re-indexing. In case of re-indexing we just don't return the series like a search using regular expression)

An advantage of using series names in a SiriDB cluster is that we know in which pool the series exists. The given query will therefore only be send
to the applicable pool(s). We don't know which pool has series when using a regular expression or group match so each pool then needs the query.

Example:

	list series 'series-001'

regular expression
------------------
Regular expressions can be used to select series. Note that each pool in a SiriDB cluster will look for matching series. Regular expressions
will be cached so will probably run faster when used more than once. They are automatically updated when new series are created or dropped.
With `show max_cache_expressions` you can see how many cached expressions are kept and this value can be changed using 
`alter database set max_cache_expressions n` (see `help alter database` for more information).
It's also possible to view which expressions a server has in cache but notice that only local cache is displayed and the result might be different 
on another server. This can be done with `list groups where cached == true`. Cached expressions are not kept after a restart.
 
Example:
	
	# list all series starting with "linux"
	list series /linux.*/

	# list all series starting with "linux" (case-insensitive)
	list series /linux.*/i
	
group
-----
Groups can basically like regular expressions but are available on all servers in a SiriDB cluster and kept even after a reboot.
It's also possible to create an index for a group. They costs of indexing a group is basically only memory but creating or dropping series
might also be a bit slower when series have a match with the group. When a group is indexed the update functions *difference* and *intersection*
are much faster with new regular expressions since they only have to look for a match in the index of the group.

Examples:

	# list all series in group "linux"
	list series `linux`
	
	# list all series in group "linux" with "cpu" in the name
	# note that this query can be very fast if group "linux" is indexed
	list series `linux` & /.*cpu.*/
	
update functions
----------------
When selecting series you can combine *series-names*, *regular-expressions* and *groups*. Update functions tell SiriDB how to combine selection.
SiriDB knows four update functions:
* difference (alias: **-**)
* symmentric_difference (alias: **^**)
* union (aliasses: **,** and **|**)
* intersection (alias: **&**)

examples
--------

	# list multiple series using union (we actually use the alias here)
	list series 'series-001', 'series-002', 'series-003'
	
	# list series in group "linux" except series which are also in group "cpu"
	list series `linux` - `cpu`
	
	# list series when member of group "linux" or group "cpu" but not both
	list series `linux` ^ `cpu`
	
	# list series which are both member of `linux` and `cpu` except when
	# a series name contains "test".
	list series `linux` & `cpu` - /.*test.*/
	
	# list series in group `linux` and view their length and last value
	list series name, length, last `linux`
	
	# list series in group `linux` which have their last data more than 100 days ago.
	list series `linux` where end < now - 100d


	# sample output
	{
		"columns": ["name"],
		"series": [
			["series-001"], 
			["series-002"]
		]
	}	
	
	
	
	