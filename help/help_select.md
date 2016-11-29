select
======

Syntax:

	select <points/functions> from <match_series [<where>]> [<time_range>] [<merge_data>]

points
======
Select original points.

Example:

	# Select points from "series-001"
	select * from "series-001"
	
functions
----------
It is possible to select multiple aggregate functions in one query. This has some
advantages over performing multiple queries since the database in this case only 
needs to search for the series and points once. To find the requested aggregate 
in the result we must add a prefix and/or suffix to the series name to make the
name unique. Not that a prefix and/or suffix is only required when querying 
multiple aggregates. 

Example:

	# Select both the min and max grouped by 5 minutes from "series-001"
	select min(5m) prefix "min-", max (5m) prefix "max-" from "series-001"

For more help on aggregate functions see `help functions`.
	
Combine functions
-----------------
Aggregate function can be used together by parsing the result of one function
to the next. It's also possible to use the same function twice which can be 
useful with for example difference.

Example:

	# Select the median grouped by 1 minute and return the difference for that result
	select median (1m) => difference () from "series-001" 
	
match_series
------------
see `help list series` for how to match series.

time_range
----------
An optional time range can be given to select only a part of the series data.
If no time range is provided SiriDB returns all the data available. As a time
range we can use *before*, *after* or *between .. and ..*

When using *after* you basically set a *start* time, with *before* a *end* time 
and when using *between .. and ..* you set both a *start* and *end* time.
Points having the exact the start time are *included* in the result, points 
with the exact end time are *excluded* from the result.

>**Note**
>
>Its safe to use *now* multiple times in query. SiriDB only calculates *now* one 
>time while processing a query. This way you can be sure that *now* has the
>same value.

Examples:

	# Select all points from "series-001" in the 24h
	select * from "series-001" after now - 1d
	
	# Select all points from "series-001" today
	select * from "series-001" between now - (now % 1d) and now - (now % 1d) + 1d
	
	# Select all points from "series-001" before November, 2015
	select * from "series-001" before "2015-11"
	
merge_data
----------
When selecting points from multiple series you can merge the data together in 
result. Most often you also want to provide an aggregate function with the
merge so series get really merged into one series. Even with merge its still 
possible to use aggregate functions on the series before they are merged.

>**Note**
>
>Sometimes it does not matter for the end result if you use an aggregate 
>function on the series or only while merging. If you have however multiple
>pools it can be an advantage to aggregate the series and the merge. This is
>an advantage because each pool can do some aggregate work and only send the
>aggregated result to the server processing the query.
>
>For example:
>
>`select * from /series.*/ merge as "series" using sum(1h)`
>
>will have the exact same result as
>
>`select sum(1h) from /series.*/ merge as "series" using sum(1h)`
>
>but the last one will be faster, assuming you are using a SiriDB cluster and
>/series.*/ contains multiple series spread out over multiple pools.

Examples:

>**Note**
>
>In the examples below we assume there are no points in the future. If you have
>points in the future and want only points from 7 days ago up till now you can
>use between now - 7d and now. Since we assume our series have no points in the
>future we use after now - 7d. 

	# We want the average value per 1 hour over the last 7 days over s01
	# and s02. The series should weight equal to each other but s01 has
	# a point each 2 seconds while s02 only has a point each 5 seconds.
	# We solve this by first getting the mean value for each series
	# by 1 hour before merging the series.
	#
	# Note we use mean(1) while merging. This means we group by 1 second or 
	# millisecond depending on the time precision. We can do this because the 
	# series are already grouped by 1h and therefore have re-indexed timestamps 
	# at precisely each hour.
	select mean(1h) from "s01", "s02" after now - 7d merge as "merged_s" using mean(1)
	
	# We want the number of points s01 and s02 have over the last 7 days.
	# Note: when having no timestamps after now this will result in one
	#       value with timestamp *now*
	select count(now) from "s01", "s02" after now - 7d merge as "merged_s" using sum(1)
	
	# We have s01 and s02 representing counter data. We want to sum the 
	# values per 4 hours over January, 2015 and show this as one series.
	select sum(4h) from "s01", "s01" between "2015-01" and "2015-02" merge as "merged_s" using sum(1)