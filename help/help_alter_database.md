alter database
==============

Syntax:

	alter database set <option>

Valid options are *drop_threshold*, *timezone*, *select_points_limit* and *list_limit*.

drop_threshold
--------------
This value is used to protect you from accidentally dropping data from SiriDB.
The threshold is a value between 0 and 1 (0/100%). The threshold value is only
checked against the pool receiving your query. The default threshold value is
1 (100%) but it might be a good idea to change this to a lower value.

>**Note**
>
>Currently the drop_threshold is only used for dropping series and shards
>because these are the only queries where we allow to drop multiple
>entries at once.

Example:

	# Do not allow dropping more than 10% series or shards at once
	alter database set drop_threshold 0.1

	# View the current threshold
	show drop_threshold

set timezone
------------
Change the timezone for the database. When using a date/time in a query SiriDB
needs to convert the given date to a timestamp. Default **NAIVE** is used which
means SiriDB is naive about the time zone and acts as if it's a local time.

>**Warning**
>
>When using a SiriDB database over multiple time zones it's probably best to
>set the time zone to anything other than *NAIVE* since with *NAIVE* the server
>*receiving* the query will convert the date to a local time-stamp. This means that
>sending the same query to a server in another time zone could respond with
>a different result.
>
>However, it's always possible in the query to specify
>a UTC date by adding 'Z' to the date. For example: '2016-01-11 16:00Z' will
>use UTC as it's time zone, no matter what time zone the database has configured.

For a list of valid time zones see `help timezones`

Example:

	# Set the default time zone to UTC
	alter database set timezone 'UTC'

	# Set the default time zone to NAIVE
	alter database set timezone 'NAIVE'

	# Set the default time zone to Europe/Amsterdam
	alter database set timezone 'Europe/Amsterdam'  

select\_points_limit
-------------------
Change the maximum points which can be returned by a select query. The default value is set to one million points to prevent a single query for taking to much memory.

Example:

    # Set the select points limit to 5 million
    alter database set select_points_limit 5000000
    
list_limit
----------
Change the maximum value which can be used as a limit for a list statement. The default value is set to ten thousand to prevent queries which could take a large amount of memory. The value must be greater than or equal to 1000.

Example:

    # Set the list limit to 50 thousand.
    alter database set list_limit 50000
    