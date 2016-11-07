SiriDB Server
=============

SiriDB (C-implementation)

Installation
------------

Compiled packages are available for Ubuntu (amd64 - 16.04 LTS xenial)

	wget https://storage.googleapis.com/siridb/server/deb/siridb-server_2.0.8_amd64.deb
	sudo dpkg -i siridb-server_2.0.8_amd64.deb
	
For creating a new or joining an existing database you need the manage tool:

	wget https://storage.googleapis.com/siridb/manage/deb/siridb-manage_2.0.1_amd64.deb
	sudo dpkg -i siridb-manage_2.0.1_amd64.deb
	
If you like to manage SiriDB from the terminal we have a prompt with auto-completion support available:

	wget https://storage.googleapis.com/siridb/prompt/deb/siridb-prompt_2.0.1_amd64.deb
	sudo dpkg -i siridb-prompt_2.0.1_amd64.deb


Compile
-------

(Ubuntu) Install the following packages:
 
	sudo apt install libuv1-dev
	sudo apt install uuid-dev
	sudo apt install libpcre3-dev


Build Deb Package
-----------------	

(Ubuntu) Install the following packages:

    sudo apt install libpcre3-dev
    sudo apt install libuv1-dev
    sudo apt install uuid-dev
    sudo apt install python3-setuptools
    sudo apt install devscripts
    sudo apt install dh-make
    sudo apt install debhelper
    sudo apt install pkg-config
    sudo apt install dh-autoreconf
    sudo apt install dh-exec


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
