Aggregate functions
===================
SiriDB supports multiple build-in aggregation and filter functions. Using these functions can be useful to reduce network traffic. Note that multiple functions can be combined using the arrow `=>` sign. (see `help select` for more information on how to use and combine functions)

Most aggregation function accept an optional `ts` argument. When not providing the `ts` argument, SiriDB will usually return the last timestamp in the result. One exception is the `first()` function which will return the first timestamp instead.

For example:

    # Select the last time-stamp and the average over all values.
    select mean() from `my_series`

    # Select the first time-stamp and first value:
    select first() from `my_series`

List of supported aggregation and filter functions:

limit
-----
Syntax:

    limit(max_points, aggr_function)

Returns at most `max_points` and uses a given aggregation function to reduce
the number of points if needed.

Example:

    # Returns at most 100 points for 'my-series'. The original values are
    # returned in case hundred or less points are found. In case more points
    # are found a mean aggregation function is used.
    select limit(100, mean) from "my-series"

count
-----
Syntax:

	count([ts])

Returns an integer value.

Count can be used to calculate points over a period of time.

Example:

	# Get the number of points in 'series-001' over the past 24 hours.
	select count(now) from "series-001" between now - 1d and now

sum
---
Syntax:

	sum([ts])

Returns an integer or float value depending on the series data type.

Sum can be used when you want to know the sum of the values over a period of time.

Example:

    # Get the sum of the values collected over the last 24 hours per hour.
    select sum(1h) from "series-001" between now - 1d and now


max
---
Syntax:

	max([ts])

Returns an integer or float value depending on the series data type.

Max can be used to identify the highest value in the selected time window.

Example:

    # Get the maximum value in 'series-001' over the last week.
    select max(now) from "series-001" between now - 1w and now


min
---
Syntax:

	min([ts])

Returns an integer or float value depending on the series data type.

Min is the opposite of max, you identify the lowest value in the selected time window.

Example:

    # Get the minimum value per day from 'series-001' between two dates.
    select min(1d) from "series-001" between '2016-11-14' and '2016-11-21'


mean
----
Syntax:

	mean([ts])

Returns a float value.

Mean is used to calculate the average values per selected time window.

Example:

    # Get average value of 'series-001' up until now.
    select mean(now) from "series-001" before now


median
------
Syntax:

	median([ts])

Returns a float value.

The median is a robust measure of central location, and is less affected by the presence of outliers in your data. When the number of data points is odd, the middle data point is returned as float value. When the number of data points is even, the median is interpolated by taking the average of the two middle values.

median_high
-----------
Syntax:

	median_high([ts])

Returns an integer or float value depending on the series data type.

The high median is always a member of the data set. When the number of data points is odd, the middle value is returned. When it is even, the larger of the two middle values is returned.

median\_low
-----------
Syntax:

	median_low([ts])

Returns an integer or float value depending on the series data type.

The low median is always a member of the data set. When the number of data points is odd, the middle value is returned. When it is even, the smaller of the two middle values is returned.

variance
--------
Syntax:

	variance([ts])

Returns a float value.

Returns the sample variance of data, an iterable of at least two real-valued numbers. Variance, or second moment about the mean, is a measure of the variability (spread or dispersion) of data. A large variance indicates that the data is spread out; a small variance indicates it is clustered closely around the mean.

pvariance
---------
Syntax:

	pvariance([ts])

Returns a float value.

Returns the population variance of data, a non-empty iterable of real-valued numbers. Variance, or second moment about the mean, is a measure of the variability (spread or dispersion) of data. A large variance indicates that the data is spread out; a small variance indicates it is clustered closely around the mean.

stddev
------
Syntax:

    stddev([ts])

Returns a float value.

Returns the standard deviation which is the square root of its variance.

difference
----------
Syntax:

	difference([ts])

Returns an integer or float value depending on the series data type.

Difference without arguments is used to get the difference between values.
As an optional argument you can specify a time period. In this case the function returns the difference between the first value and the last value within the time window.

Example:

    # Select difference between values in series-001.
    select difference() from 'series-001'

derivative
----------
Syntax:

	derivative([ts [, ts]])

Returns a float value.

The derivative can be used to get the difference per time unit. When no optional arguments
are used we return the difference per one unit. For example, in a database with second
precision the return value will be the difference per second. Optionally another time unit can
be used. A second argument can be used to set a time period. This time period will be used to get the difference between the first and last value within the time window.

Example:

    # Select the difference per second for values in series-001.
    select derivative(1s) from 'series-001'

    # Select the difference per second between the first and last value
    # within each hour for values in 'series-001'
    select derivative(1s, 1h) from 'series-001'


filter
------
Syntax:

	filter(<operator> <value>)

Returns an integer, float or string value depending on the series data type.

Filter is used to filter the result by values.

Example:

    # Select all values from 'series-001' except where the value is 0
    select filter(!= 0) from 'series-001'

    # Select all positive values from 'series-001'
    select filter(> 0) from 'series-001'

    # Select all values containing 'error' and not 'unavailable
    select filter(~'error') => filter(!~'unavailable') from 'some-log-series'

    # Select all values starting with 'error' using a regular expression
    select filter(/error.*/) from 'some-log-series'

    # Filter out Not-a-number (nan) and Infinite values
    select filter(!=nan) => filter(!=inf) => filter(!=-inf) from 'some-float-series'


first
-----
Syntax:

    first([ts])

Returns the first value in each `ts` time window. (Or just the first value)

Example:

    # Select the first value from 'series-001'
    select first() from 'series-001'

    # Select the first value per day from 'series-001'
    select first(1d) from 'series-001'

    # Select the first value in 2018 from 'series-001'
    select first() from 'series-001' after '2018'


last
----
Syntax:

    last([ts])

Returns the last value in each `ts` time window. (Or just the last value)

Example:

    # Select the last value from 'series-001'
    select last() from 'series-001'

    # Select the last value per day from 'series-001'
    select last(1d) from 'series-001'

    # Select the last value in 2017 from 'series-001'
    select last() from 'series-001' before '2018'