SiriDB Server
=============

SiriDB (C-implementation)

Installation
------------

Compiled packages are available for Ubuntu (amd64 - 16.04 LTS xenial)

	wget https://storage.googleapis.com/siridb/server/deb/siridb-server_2.0.0_amd64.deb
	sudo dpkg -i siridb-server_2.0.0_amd64.deb
	
For creating a new or joining an existing database you need the manage tool:

	wget https://storage.googleapis.com/siridb/manage/deb/siridb-manage_2.0.0_amd64.deb
	sudo dpkg -i siridb-manage_2.0.0_amd64.deb
	
If you like to manage SiriDB from the terminal we have a prompt with auto-completion support available:

	wget https://storage.googleapis.com/siridb/prompt/deb/siridb-prompt_2.0.0_amd64.deb
	sudo dpkg -i siridb-prompt_2.0.0_amd64.deb

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
