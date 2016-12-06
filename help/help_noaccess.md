Lost access to SiriDB
---------------------
Read/use this section when accidentally all access rights to a database are
gone. Follow the steps below to recover the default user.
(username: *iris*, password: *siri*)

Stop **all servers** in the SiriDB cluster.

	# Assume you use systemd to start SiriDB...
	> sudo systemctl stop siridb-server.service

>**Warning**
>
>It's really important to stop all servers in a SiriDB cluster to prevent 
>database inconsistency.

Remove the appropriate file on **all servers** in the SiriDB cluster.

	# Restore the default user
	> rm my_database_folder/users.dat

Start **all servers** in the SiriDB cluster.

	# Assume systemd is used to start SiriDB...
	> sudo systemctl start siridb-server
