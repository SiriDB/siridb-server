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

Series can be matched using different methods. Groups can help to quickly get the required series even in a database with millions of unique series.


syntax:

	<series_name | regular_expression | group> [update_function <match_series>]

series name
-----------
A series name is a string containing the series name. An error is raised when the given series name is not found in SiriDB. (Note that this exception is not raised when the database is re-indexing. In case of re-indexing we just don't return the series like a search using regular expression)

An advantage of using series names in a SiriDB cluster is that we know in which pool the series exists. The given query will therefore only be send to the applicable pool(s). We don't know which pool has series when using a regular expression or group match so each pool then needs the query.

Example:

	list series 'series-001'

regular expression
------------------
Regular expressions can be used to select series. Note that each pool in a SiriDB cluster will look for matching series. If you plan to use a regular expression multiple times, you should consider creating a group for the expression.

Example:

	# list all series starting with "linux"
	list series /linux.*/

	# list all series starting with "linux" (case-insensitive)
	list series /linux.*/i

	# list all series not starting with "linux"
	list series /(?!linux).*/

	# list all series not containing "linux"
	list series /((?!linux).)*/

group
-----
Groups are basically cached regular expression and can be used together with normal
regular expressions. When you use a regular expression to match series in a group it's
best to first select the group and then the regular expression. This way the regular
expression only needs to validate series inside the group.

Examples:

	# list all series in group "linux"
	list series `linux`

	# list all series in group "linux" with "cpu" in the name
	# note that we first select the group so the regular expression only
	# needs to be validated on series in the group.
	list series `linux` & /.*cpu.*/

update functions
----------------
When selecting series you can combine *series-names*, *regular-expressions* and *groups*. Update functions tell SiriDB how to combine selection.
SiriDB knows four update functions:

* difference (alias: **-**)
* symmentric_difference (alias: **^**)
* union (aliases: **,** and **|**)
* intersection (alias: **&**)

examples
--------

	# list multiple series using union (we actually use the alias here)
	list series 'series-001', 'series-002', 'series-003'

	# list series in group "linux" except series which are also in group "cpu"
	list series `linux` - `cpu`

	# list series when member of group "linux" or group "cpu" but not both
	list series `linux` ^ `cpu`

	# list series that are both members of `linux` and `cpu` except when
	# a series name contains "test".
	list series `linux` & `cpu` - /.*test.*/

	# list series in group `linux` and view their length.
	list series name, length `linux`

	# list series in group `linux` which have their last data point more
	# than 100 days ago
	list series `linux` where end < now - 100d


	# sample output (list series)
	{
		"columns": ["name"],
		"series": [
			["series-001"],
			["series-002"]
		]
	}
