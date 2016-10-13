timeit
======

Can be placed in front of any query and will return information about the time it took to process the query.

Syntax:

    timeit <any_query>

Example result:
	
	{
		"__timeit__": [
	    	{
	      		"time": 0.001156334212755393,
	      		"server": "server04.siridb.net:9010"
	    	},	    
	   		{
	      		"time": 0.001481771469116211,
	      		"server": "server01.siridb.net:9010"
	    	}
	  	]
	}
    
Here `__timeit__` is an array containing response data from each server involved in processing the query. The last server in this list is the server who has received the query. Since this server is responsible for sending the response it has to wait for all other servers to complete and therefore the query time for this server will be always the highest value of all servers in the list.