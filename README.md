SiriDB Server
=============

SiriDB (C-implementation)


Logging
-------
Journal is prefered over rsyslog. To setup persistant logging using journald:
`sudo mkdir -p /var/log/journal`

Modify `/etc/systemd/journald.conf` and enable the following:

	[Journal]
	Storage=persistent

	
Now its possible to follow logs:
`journalctl -u siridb-server -f`

Or view boots
`journalctl --list-boots`

For eaxmple, the last boot:
`journalctl -u siridb-server --boot=-1`