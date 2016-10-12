Aggregate functions
===================
With SiriDB it's possible to request points for a time series and perform a function
on the data before returning the result. Although you could also request the original points
and perform your own functions on the data, using functions in SiriDB can save some network traffic.

For help on how to use functions see `help select`.

count
-----
Syntax:

	count (ts)
	
sum
---
Syntax:

	sum (ts)

max
---
Syntax:

	max (ts)

min
---
Syntax:

	min (ts)

mean
----
Syntax:

	mean (ts)
	
median
------
Syntax:

	median (ts)
	
median_high
-----------
Syntax:

	median_high (ts)
	
median\_low
-----------
Syntax:

	median_low (ts)
	
variance
--------
Syntax:

	variance (ts)
	
pvariance
---------
Syntax:

	pvariance (ts)

difference
----------
Syntax:

	difference ([ts])

derivative
----------
Syntax:

	derivative ([ts [, ts]])

filter
------
Syntax:

	filter (<operator> <value>)


