drop series
===========

Syntax:

	drop series [series_match] [where ...] [set ignore_threshold true/false]

Drops series from SiriDB. Optionally you can use a match and/or where statement
to filter the series you want to drop. For more information about how to match
series look at `help list series`.

SiriDB has a mechanism to protect you from accidentally dropping all (or many)
series. This is done with a *threshold* value. If the server receiving your drop
request finds more series to drop than the threshold, the request
is denied and you receive an *error_msg* about trying to delete more series than
the threshold value. The *drop_threshold* value will not be checked by other
servers in the cluster. You can view the current *drop_threshold* with `show drop_threshold` or look up `help alter database` for how to change this value. If you want to ignore the *drop_threshold* for one request you can
add *set ignore_threshold true*. The default drop threshold is set to 1 (100%)
which means you cannot drop *all* series but any other amount will pass. Any
value between 0 and 1 will work. For example a value of 0.5 means you cannot
drop more than 50% of the available series.

>**Warning**
>
>Before dropping series using a regular expression you should check the
>expression using `count series` and/or `list series` and see if your
>match has the expected result.

Examples:

	# Drop series "series-001"
	drop series "series-001"

	# Drop all series
	drop series set ignore_threshold true
