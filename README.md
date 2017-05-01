SiriDB Server
=============
SiriDB is a highly-scalable, robust and super fast time series database.

---------------------------------------
  * [Installation](#installation)
    * [Ubuntu](#ubuntu)
    * [Compile from source](#compile-from-source)
      * [Linux](#linux)
      * [OSX](#osx)
  * [Create or expand a database](#create-or-expand-a-database)
  * [Using SiriDB](#using-siridb)
    * [SiriDB Connectors](#siridb-connectors)
    * [SiriDB HTTP](#siridb-http)
    * [SiriDB Prompt](#siridb-prompt)
  * [API/Query language](#query-language)
  * [Docker](#docker)
    * [Single SiriDB server setup](#single-siridb-server-setup)
      * [Get the SiriDB server up and running](#get-the-siridb-server-up-and-running)
      * [Create a database](#create-a-database)
      * [Insert data](#insert-data)
      * [SiriDB HTTP server](#siridb-http-server)
    * [Advanced setup](#advanced-setup)
      * [Infrastructure](#infrastructure)
      * [Verify the getting started setup](#verify-the-getting-started-setup)
      * [Create a replica](#create-a-replica)
      * [Add an additional pool](#add-an-additional-pool)
	
---------------------------------------

## Installation
### Ubuntu
For Ubnutu we have a deb package available which can be downloaded [here](https://github.com/transceptor-technology/siridb-server/releases/latest).

Note: SiriDB requires *libexpat1* and *libuv1*, these libraries can be easily installed using apt:
```
apt install wget libexpat1 libuv1
```

### Compile from source
#### Linux
Install the following requirements: (Ubnutu)
```
sudo apt install libpcre3-dev
sudo apt install libuv1-dev 
sudo apt install uuid-dev 
```

Compile (replace Release with Debug for a debug build):
```
cd ./Release
make clean
make
```	
#### OSX
Install the following requirements:
```
brew install pcre
brew install libuv
brew install ossp-uuid
```
Compile (replace Release with Debug for a debug build):
```    
cd ./Release
export CFLAGS="-I/usr/local/include"
export LDFLAGS="-L/usr/local/lib"
make clean
make
```
## Create or expand a database
[SiriDB Admin](https://github.com/transceptor-technology/siridb-admin) is required for creating a new database or expanding an existing database with a new server. Documentation on how to install and use the admin tool can be found at the [siridb-admin](https://github.com/transceptor-technology/siridb-admin/blob/master/README.md) github project. Binaries are available for most platforms and can be downloaded from [here](https://github.com/transceptor-technology/siridb-admin/releases/latest).

## Using SiriDB
SiriDB has several tools available to connect to a SiriDB database. 

### SiriDB Connectors
Native connectors are available for [Python](https://github.com/transceptor-technology/siridb-connector) and [Go](https://github.com/transceptor-technology/go-siridb-connector). When not using Python or Go you can still communicate to SiriDB using [SiriDB HTTP](#siridb-http).

### SiriDB HTTP
[SiriDB HTTP](https://github.com/transceptor-technology/siridb-http/blob/master/README.md) provides a HTTP API for SiriDB and has support for JSON, MsgPack, CSV and QPack. SiriDB HTTP also has an optional web interface and SSL support.

### SiriDB Prompt
[SiriDB Prompt](https://github.com/transceptor-technology/siridb-prompt) can be used as a command line SiriDB client with auto-completion support and can be used to load json or csv data into a SiriDB database. Click [here](https://github.com/transceptor-technology/siridb-prompt/blob/master/README.md) for more information about SiriDB Prompt.

## Query language
Documentation about the query language can be found at https://siridb.net/docs.

## Docker
All SiriDB components are available from Docker Hub:

* [siridb/server](https://hub.docker.com/r/siridb/server/)
* [siridb/manage](https://hub.docker.com/r/siridb/manage/)
* [siridb/prompt](https://hub.docker.com/r/siridb/prompt/)
* [siridb/http](https://hub.docker.com/r/siridb/http/)

When using docker there are a couple of things to consider:

1. Use **hostnames** for your containers as this makes it easier to maintain and troubleshoot your SiriDB cluster.
2. The default database location is */var/lib/siridb* use a **data volumes** or **data volume containers** to ensure data is persistent.
3. The **SiriDB manage** command which is used to create or join existing databases requires access to the IP stack and filesystem of the SiriDB server being managed and can thus best be used using the following command:

```
docker run -it \
	--rm \
	--network container:server-a \
	--volumes-from=server-a \
	siridb/manage
```

### Single SiriDB server setup
The following guide helps you to quickly setup and use SiriDB so you can try it out and kick its tires.
This guide uses docker to setup the environment.
If you haven't installed docker yet we'd like to direct you to the docker getting started [guide](https://docs.docker.com/engine/getstarted/).

If you need help getting started please don't hesitate to contact us.

#### Get the SiriDB server up and running
First let's fire up our first SiriDB server:

	# First we create a bridged network
	docker network create -d bridge siridb-net
	# Start SiriDB container
	docker run -d \
		--network=siridb-net \
		--name=server-a \
		--hostname=server-a \
		siridb/server

By using these two docker commands a bridged network for SiriDB has been created and the first server is started in a container. See our installation guide for more details.

#### Create a database
Next step, create a database using the SiriDB manage prompt, this prompt is available as a separate package or docker container. Since we're using docker for our getting started we use the container:
```
docker run -it \
	--rm \
	--network container:server-a --volumes-from=server-a \
	siridb/manage create-new \
		--dbname MyTimeSeriesDatabase \
		--time-precision s
```

This command should respond with the following output:
```
Created database 'MyTimeSeriesDatabase'
Database "MyTimeSeriesDatabase" created successfully
```

Let's look at what just happened, we created a database named **MyTimeSeriesDatabase** on **server-a** with a time precision of a second. We use seconds in this example as it fits the data set we are going to use.

#### Insert data
Now that we have a running database we might as well put it to good use.
For our example we use the Google Finance data since this data is available for free for different stock markets. The complete list can be found [here](https://www.google.com/intl/en/googlefinance/disclaimer/).

In order to format the data, we create a small python script that downloads the data and output's it as CSV. The source of this script can be found [here](https://github.com/transceptor-technology/siridb-demo/blob/master/demo_google_finance/data_google_finance.py). For this guide we created a docker container, as well as for this script.


In this example we use IBM's ticker data:
```
docker run -it \
	--rm \
	siridb/demo_google_finance \
		-t IBM \
		-i 300 \
		-d 50 > /tmp/ibm_ticker.csv
```
Please be patient as this script might take a couple of seconds to complete.
The output is piped to a file in the */tmp* folder.

>**Note**
> You can learn more about the parameters of our google finance script by using the *--help* option at startup, e.g. *docker run -it --rm siridb/demo_google_finance --help*

Using the docker prompt we can load the data we just fetched into our database, in order to access the data in our local */tmp* folder we need to mount this folder in our docker container.

```
docker run -it \
	--rm \
	--network=siridb-net \
	-v /tmp:/tmp \
	siridb/prompt \
		-u iris \
		-p siri \
		-d MyTimeSeriesDatabase \
		-s server-a:9000
```

The SiriDB prompt should appear now, using the SiriDB `import_csv` command the ticker data can be loaded into SiriDB.
```
import_csv /tmp/ibm_ticker.csv
```
Use the `list series` command to list the just inserted time series:
```
iris@MyTimeSeriesDatabase> list series
name
━━━━━━━━━━━━━━━━━━━━━━━━━
GOOGLE-FINANCE-IBM-CLOSE
GOOGLE-FINANCE-IBM-HIGH
GOOGLE-FINANCE-IBM-LOW
GOOGLE-FINANCE-IBM-OPEN
GOOGLE-FINANCE-IBM-VOLUME

iris@MyTimeSeriesDatabase>
```
Feel free to play around in the prompt, our autocomplete routine and built-in help might prove useful.

#### SiriDB HTTP server
We created a basic HTTP server for SiriDB.
```
docker run -it \
	--rm \
	--network=siridb-net \
	-p 8080:8080 siridb/http \
		-u iris \
		-p siri \
		-d MyTimeSeriesDatabase \
		-s server-a:9000
```
Use your web browser to connect to http://127.0.0.1:8080, now you can access the SiriDB-HTTP prompt.

![SiriDB Prompt](http://siridb.net/img/introduction_gettingstarted_prompt.png)

Enter the following query to see SiriDB in action:
```
select mean(1d) => difference () from /GOOGLE-FINANCE-IBM-(OPEN|CLOSE)/
```

The following two graphs should appear

![SiriDB Prompt](http://siridb.net/img/introduction_gettingstarted_query.png)

Congratulations, you have just setup SiriDB and loaded an example dataset.
The next chapter covers expanding this setup to a robust 4 node SiriDB cluster.

### Advanced setup
In this section we expand the previously created setup to a full blown 4 node SiriDB cluster.
Each of the servers are running in their own docker container.
* `server-a` and `server-b` are replica's and form `pool0`,
* `server-c` and `server-d` are also replica's and form `pool 1`.

This section assumes you've followed the Single server setup. Next we'll see SiriDB's scalability in action :-)

The diagram below shows the final SiriDB environment of this exercise:

		╔════════════════════════════════════════╗
		║ SiriDB Cluster                         ║
		║  ┏━━━━━━━━━━━━━━━┓  ┏━━━━━━━━━━━━━━━┓  ║
		║  ┃    pool 0     ┃  ┃    pool 1     ┃  ║
		║  ┃┌─────────────┐┃  ┃┌─────────────┐┃  ║
		║  ┃│   SiriDB    │┃  ┃│   SiriDB    │┃  ║
		║  ┃│  server A   │┃  ┃│  server C   │┃  ║
		║  ┃└─────────────┘┃  ┃└─────────────┘┃  ║
		║  ┃       ▲       ┃  ┃       ▲       ┃  ║
		║  ┃       │       ┃  ┃       │       ┃  ║
		║  ┃       ▼       ┃  ┃       ▼       ┃  ║
		║  ┃┌─────────────┐┃  ┃┌─────────────┐┃  ║
		║  ┃│   SiriDB    │┃  ┃│   SiriDB    │┃  ║
		║  ┃│  server B   │┃  ┃│  server D   │┃  ║
		║  ┃└─────────────┘┃  ┃└─────────────┘┃  ║
		║  ┗━━━━━━━━━━━━━━━┛  ┗━━━━━━━━━━━━━━━┛  ║
		╚════════════════════════════════════════╝

#### Infrastructure
Our advanced getting started infrastructure consists of the following docker components:

- a bridged network connecting all SiriDB containers.
- 4 SiriDB server containers named server-a up to server-d
- 1 SiriDB manage container
- 1 SiriDB prompt container

The diagram below shows what the final infrastructure will look like:

		╔════════════════════════════════════════════════════════════════╗
		║ Docker host                                                    ║
		║            ┌─────────┐ ┌─────────┐ ┌─────────┐ ┌─────────┐     ║
		║            │ SiriDB  │ │ SiriDB  │ │ SiriDB  │ │ SiriDB  │     ║
		║            │container│ │container│ │container│ │container│     ║
		║            │server-a │ │server-b │ │server-c │ │server-d │     ║
		║            └─────────┘ └─────────┘ └─────────┘ └─────────┘     ║
		║                 │           │           │           │          ║
		║───siridb-net────●─────●─────●───────────●─────●─────●─────     ║
		║                       │                       │                ║
		║                  ┌─────────┐             ┌─────────┐           ║
		║                  │ SiriDB  │             │ SiriDB  │           ║
		║                  │container│             │container│           ║
		║                  │ manage  │             │ prompt  │           ║
		║                  └─────────┘             └─────────┘           ║
		╚════════════════════════════════════════════════════════════════╝


#### Verify the getting started setup
We start of by using the SiriDB prompt to verify our installation.

	docker run -it --rm \
		--network=siridb-net \
		siridb/prompt \
			-u iris \
			-p siri \
			-d MyTimeSeriesDatabase \
			-s server-a:9000

Use the `list servers` command to show the current servers available, there should be only *server_a* if you have followed our guide completely.

	iris@MyTimeSeriesDatabase> list servers
	name          ┃ pool ┃ version ┃ online ┃ status
	━━━━━━━━━━━━━━╇━━━━━━╇━━━━━━━━━╇━━━━━━━━╇━━━━━━━━
	server-a:9010 │ 0    │ 2.0.11  │ True   │ running

#### Create a replica
As we care for data, the next step is to create a replica.

The first step is to start a second SiriDB server container, server-b to be precise.

	docker run -d \
		--network=siridb-net \
		--name=server-b\
		--hostname=server-b \
		siridb/server

The second step is that we create the actual replica using the siridb-manage command.
Note: it can take a couple of seconds for the server to boot up, if you try too soon you might receive the following error: *Unable to get local SiriDB info, please check if SiriDB is running and listening to 127.0.0.1:9000*

	docker run -it --rm \
		--network container:server-b \
		--volumes-from=server-b \
		siridb/manage create-replica \
			--dbname MyTimeSeriesDatabase \
			--remote-address server-a \
			--remote-port 9000 \
			--user iris \
			--password siri \
			--pool 0

#### Add an additional pool
As data grows over time SiriDB can easily be expanded by adding pools.
In the following steps we add a pool and make the pool high available by ensuring it is a replica.

>**Note**
> It is important to know that we can only continue expanding our cluster if all servers are in a running state!
> Use the SiriDB prompt and the list servers command to verify this.

If you have ensured that all servers are running we can continue by starting our third SiriDB server container (server-c)

	docker run -d \
		--network=siridb-net \
		--name=server-c \
		--hostname=server-c \
		siridb/server

The next step is to create the actual additional pool using the manage prompt and the **create-pool** command

	docker run -it --rm \
		--network container:server-c \
		--volumes-from=server-c \
	  siridb/manage create-pool \
			--dbname MyTimeSeriesDatabase \
			--remote-address server-a \
			--remote-port 9000 \
			--user iris \
			--password siri

To top it all off we create a replica of server-c.

	# Start the fourth and last SiriDB container (server-d)
	docker run -d \
		--network=siridb-net \
		--name=server-d \
		--hostname=server-d \
		siridb/server

	# Use the siridb/manage container to create a replica of our just created pool 1
	docker run -it --rm \
		--network container:server-d \
		--volumes-from=server-d \
		siridb/manage create-replica \
			--dbname MyTimeSeriesDatabase \
			--remote-address server-c \
			--remote-port 9000 \
			--user iris \
			--password siri \
			--pool 1

If all went according to plan a 'list servers' command shows the following output:

	iris@MyTimeSeriesDatabase> list servers
	name          ┃ pool ┃ version ┃ online ┃ status
	━━━━━━━━━━━━━━╇━━━━━━╇━━━━━━━━━╇━━━━━━━━╇━━━━━━━━
	server-a:9010 │ 0    │ 2.0.11  │ True   │ running
	server-b:9010 │ 0    │ 2.0.11  │ True   │ running
	server-c:9010 │ 1    │ 2.0.11  │ True   │ running
	server-d:9010 │ 1    │ 2.0.11  │ True   │ running

