caSiriDB Server
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
For Ubuntu we have a deb package available which can be downloaded [here](https://github.com/transceptor-technology/siridb-server/releases/latest).

Note: SiriDB requires *libexpat1* and *libuv1*, these libraries can be easily installed using apt:
```
apt install libexpat1 libuv1
```

### Compile from source
>From version 2.0.19 libcleri is not included as part of this source anymore
>and needs to be installed separately. libcleri can be found here:
>[https://github.com/transceptor-technology/libcleri](https://github.com/transceptor-technology/libcleri)

#### Linux
Install the following requirements: (Ubuntu)
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
Native connectors are available for [C](https://github.com/transceptor-technology/libsiridb), [Python](https://github.com/transceptor-technology/siridb-connector) and [Go](https://github.com/transceptor-technology/go-siridb-connector). When not using C, Python or Go you can still communicate to SiriDB using [SiriDB HTTP](#siridb-http).

### SiriDB HTTP
[SiriDB HTTP](https://github.com/transceptor-technology/siridb-http/blob/master/README.md) provides a HTTP API for SiriDB and has support for JSON, MsgPack, Qpack, CSV and Socket.io. SiriDB HTTP also has an optional web interface and SSL support.

### SiriDB Prompt
[SiriDB Prompt](https://github.com/transceptor-technology/siridb-prompt) can be used as a command line SiriDB client with auto-completion support and can be used to load json or csv data into a SiriDB database. Click [here](https://github.com/transceptor-technology/siridb-prompt/blob/master/README.md) for more information about SiriDB Prompt.

## Query language
Documentation about the query language can be found at http://siridb.net/docs.


# SiriDB by Example

## SiriDB installation

Our SiriDB setup consists of an Ubuntu server, for this example we used Ubuntu server 16.04.2
On this server we install the following SiriDB components:

* SiriDB server
* SiriDB admin tool
* SiriDB prompt
* SiriDB HTTP


### SiriDB Server
Let's start with the SiriDB server installation.
First we install the prerequisites:

```
apt install libexpat1 libuv1
```

Next step is downloading, installing and starting SiriDB server:

```
cd /tmp
wget https://github.com/transceptor-technology/siridb-server/releases/download/2.0.21/siridb-server_2.0.21_amd64.deb
dpkg -i ./siridb-server_2.0.21_amd64.deb
sudo systemctl start siridb-server.service
```

### SiriDB admin tool

SiriDB admin tool is used for managing SiriDB service accounts and databases. SiriDB-Admin can be used both by command-line arguments and a graphical web-interface.

The SiriDB admin tool is distributed for multiple platforms as a binary tool, as we install SiriDB on a single Ubuntu server in our example let's download the appropriate version and place it in `/usr/local/bin`:

```
wget https://github.com/transceptor-technology/siridb-admin/releases/download/1.1.2/siridb-admin_1.1.2_linux_amd64.bin
cp siridb-admin_1.1.2_linux_amd64.bin /usr/local/bin/siridb-admin
chmod +x /usr/local/bin/siridb-admin
```

### SiriDB prompt

SiriDB prompt is a command-line client for SiriDB, in this setup we use this client to load our example dataset.

Last component we need is the SiriDB prompt which we also install on our Ubuntu server.

```
wget https://github.com/transceptor-technology/siridb-prompt/releases/download/2.0.5/siridb-prompt_2.0.5_amd64.deb
dpkg -i ./siridb-prompt_2.0.5_amd64.deb
```

### SiriDB HTTP

SiriDB HTTP provides a HTTP API and optional web interface for SiriDB.

```
wget https://github.com/transceptor-technology/siridb-http/releases/download/2.0.1/siridb-http_2.0.1_linux_amd64.bin
mv siridb-http_2.0.1_linux_amd64.bin /usr/local/bin/siridb-http
chmod +x /usr/local/bin/siridb-http
```



## SiriDB create a database

Using the SiriDB we create our first database.

```
siridb-admin  \
  -u sa \
  -p siri \
  -s hourglass \
  new-database \
    --db-name MyTimeSeriesDatabase \
    --time-precision s
```

This command should respond with the following output:
```
successfully created database: MyTimeSeriesDatabase
```

Let's look at what just happened, we created a database named **MyTimeSeriesDatabase** on **hourglass** with a time precision of a second. We use seconds in this example as it fits the data set we are going to use.


## Insert data

Now that we have a running database we might as well put it to good use.
For our example we use the Google Finance data since this data is available for free for different stock markets. The complete list can be found [here](https://www.google.com/intl/en/googlefinance/disclaimer/).

In order to format the data, we create a small python script that downloads the data and output's it as CSV. The source of this script can be found [here](https://github.com/transceptor-technology/siridb-demo):

```
apt install python3-pip
pip3 install asyncio aiohttp argparse
wget https://raw.githubusercontent.com/transceptor-technology/siridb-demo/master/demo_google_finance/data_google_finance.py
```

Please be patient as this script might take a couple of seconds to complete.
The output is piped to a file in the */tmp* folder.

>**Note**
> You can learn more about the parameters of our google finance script by using the *--help* option at startup, e.g. *docker run -it --rm siridb/demo_google_finance --help*


Using the SiriDB prompt we can load the data we just fetched into our just created database named MyTimeSeriesDatabase

```
siridb-prompt \
	-u iris \
	-p siri \
	-d MyTimeSeriesDatabase \
	-s 127.0.0.1:9000
```

We use localhost in this example as everything is installed on a single host.

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
We created a basic HTTP server for SiriDB which we will use in our example to provide you access to SiriDB's powers.

SiriDB-HTTP requires a configuration file, luckily the tool can generate one for us using the following command:
```
sudo sh -c 'siridb-http > /etc/siridb/siridb-http.conf'
```

Now use your favorite editor to change the `user`, `password` & `dbname` fields in our just created configuration file.

```
user = iris
password = siri
dbname = MyTimeSeriesDatabase
```

After this it is time to start siridb-http
```
sudo siridb-http -c /etc/siridb/siridb-http.conf
```

Use your web browser to connect to http://127.0.0.1:8080, now you can access the SiriDB-HTTP prompt.

![SiriDB Prompt](http://siridb.net/img/introduction_gettingstarted_prompt.png)

Enter the following query to see SiriDB in action:
```
select mean(1d) => difference () from /GOOGLE-FINANCE-IBM-(OPEN|CLOSE)/
```

The following two graphs should appear

![SiriDB Prompt](http://siridb.net/img/introduction_gettingstarted_query.png)

Your all set now, have fun!
