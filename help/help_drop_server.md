drop server
===========

Syntax:

	drop server <server_uuid / server_name>
	
Can be used to remove a server. We only allow dropping a server which has a
replica since scaling down in number of pools  is currently not supported. 
A server needs to be turned off before it can be dropped.

>**Note**
>
>When having two servers in a pool, let's call them **siri1** and **siri2** and for some
>reason **siri2** is broken and does not start. You might be in a situation where
>**siri1** is waiting for **siri2** to connect and start to synchronize data. Both
>servers are not working in this case but when dropping **siri2**, **siri1** removes
>the *'wait for synchronization'* status and starts accepting inserts and queries. 

Example:

	# Drop server 'siri2:9010'. We first need to turn off
	# this server and make sure the server has a replica.
	drop server 'siri2:9010'
	