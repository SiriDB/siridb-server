alter server
============


Syntax:

	alter server <server_uuid / server_name> set <option>
	
Valid options are *address*, *port*, *backup_mode* and *log_level*. We can use
both a servers name or uuid to change a server. To view the current servers 
names and uuids use the command: `list servers name, uuid`

set address/port
----------------
Usually its not required to change the servers address or port using this
command but instead you should change the address/port in the configuration
file. (default /etc/siridb/siridb.conf) When the server gets online it will 
contact all SiriDB servers and they will automatically update to the new
address/port in their local database. If however all servers in a cluster are
updated at once, we need to tell at least one SiriDB server where to find the 
other server(s). This should be the only situation when this command is 
required.

Example:

	# srv1 and srv2 both have changed to another address so
	# they are not able find each other. The command below
	# is executed on srv1 and tells where to find srv2.
	
	alter server 'srv2.old.domain:9010' set address 'srv2.new.domain'
	
	# After executing the above command, srv1 will connect to srv2
	# using the new domain name and announces its own new address so 
	# srv2 will update the address automatically and will connect 
	# to srv1 again.	

backup_mode
-----------
When a backup_mode is enabled on a SiriDB server, all files in the database 
directory will be closed. (both dbpath and buffer_path) This way you can make
a backup of SiriDB without having problems with open files.
	
	
log_level
--------
With the argument *--log-level* its possible to start with a certain log level.
The default log level is *info*. If you want the log level to change while
being online, this command can be used. It will *not* be saved when the server is 
restarted.

Valid loglevels are "debug", "info", "warning", "error" and "critical"

Example:

	# Change the log-level to "debug"
	alter server f851c6a4-820e-11e5-9661-080027f37001 set log_level debug
	

