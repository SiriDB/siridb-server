alter database
==============

Syntax:

	alter database set <option>
	
Valid options are *drop_threshold* and *timezone*.

drop_threshold
--------------
This value is used to protect you from accidentally dropping data from SiriDB.
The threshold is a value between 0 and 1 (100%). The threshold value is only
checked against the pool receiving your query. The default threshold value is
1 (100%) but it might be a good idea to change this to a lower value.

>**Note**
>
>Currently the drop_threshold is only used for dropping series since this is
>the only drop statement which allows you to drop multiple series. In 
>future we plan adding drop shards and maybe drop users/networks and then 
>this threshold will also be used for these statements.
 
Example:
	
	# Do not allow dropping more than 10% of SiriDB data
	alter database set drop_threshold 0.1
	
	# View the current threshold
	show drop_threshold
	
set timezone
------------
Change the timezone for the database. When using a date/time in a query SiriDB
needs to convert the given date to a timestamp. Default **Local** is used which
means SiriDB is naive about the time zone and acts as if it's a local time.

>**Warning**
>
>When using a SiriDB database over multiple time zones it's probably best to
>set the time zone to anything other than *NAIVE* since with *NAIVE* the server
>*receiving* the query will convert the date to a local time-stamp. This means that 
>sending the same query to a server in another time zone could respond with 
>a different result.
>
>It's however always possible in the query to specify
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