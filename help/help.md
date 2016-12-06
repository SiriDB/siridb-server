help
====

Displays this help page.

Alias: ?

Syntax:

    help [<option>] [...]

Available help options:

- `timeit`: see `help timeit` for more information.
- `show`: see `help show` for more information.
- `count`: see `help count` for more information.
- `list`: see `help list` for more information.
- `select`: see `help select` for more information.
- `create`: see `help create` for more information.
- `alter`: see `help alter` for more information.
- `drop`: see `help drop` for more information.
- `grant`: see `help grant` for more information.
- `revoke`: see `help revoke` for more information.

help notes
----------
Values between [ ] are optional.

Values between < > are not real keywords but explained later in the help file.

Three dots (...) means more.

string values
-------------
A string value should be between single-, or double-quotes. When you use a single quote to start the string and need a single quote inside the string you need to prefix the single quote with another single quote.

Examples:

	'This is a double quote " and a single quote '' in a single quoted string'
	"This is a double quote "" and a single quote ' in a double quoted string"

time expressions
----------------
Time expressions are translated to a time-stamp by SiriDB. When using a 'seconds' based precision database the value should be between 0 and 4294967295 and when 'milliseconds' are used, values between 0 and 1,844674407×10¹⁹ are allowed. In a time expressions it's allowed to use integer values, date strings and keyword `now` for the current time-stamp. Float values are not allowed since we expect time-stamps to be integer values. It's possible to use suffixes 'd', 'h', 'm' and 'ms' behind integer values to specify days, hours, minutes, seconds and \*milliseconds (be careful using milliseconds when having a 'seconds' precision database since the value will be floored to seconds *before* calculating the expression). 

The following operators can be used:

	+, -, *, / and %

Note: `/` is a division towards zero, for example: `10 / 3` will return time-stamp value 3

Examples:

	now - 10d  # Returns the time-stamp for exactly 10 days ago.
	'2015-04-01' + 1h * 24  # Returns the time-stamp of April 2, 2015.

date strings
------------
Date strings are strings between single- or double-quotes representing a time-stamp value at a given date/time.

Format:

	YYYY-MM-DD HH:MM:SSZ

Instead of a space between date and time it's also possible to use capital T.
We do not need to specify the whole date/time when another precision is enough.
These are all valid date strings:

	'2013-02-10 13:04:12Z'
	'2013-02-10 13:04:12+0200'
	'2013-2-10T13:04Z'
	'2013-2-10 13'
	'2013-02-10'
	"2013-02"
	"2013"
