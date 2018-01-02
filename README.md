SiriDB Server
=============
SiriDB is a highly-scalable, robust and super fast time series database.

---------------------------------------
  * [Installation](#installation)
    * [Ubuntu](#ubuntu)
    * [Compile from source](#compile-from-source)
      * [Linux](#linux)
      * [OSX](#osx)
      * [Configuration](#configuration)
  * [Create or expand a database](#create-or-expand-a-database)
  * [Using SiriDB](#using-siridb)
    * [SiriDB Connectors](#siridb-connectors)
    * [SiriDB HTTP](#siridb-http)
    * [SiriDB Prompt](#siridb-prompt)
    * [Grafana](#grafana)
  * [API/Query language](#query-language)
  
---------------------------------------

## Installation
### Ubuntu
For Ubuntu we have a deb package available which can be downloaded [here](https://github.com/transceptor-technology/siridb-server/releases/latest).

Note: SiriDB requires *libexpat1*, *libuv1* and *libpcre2-8-0*, these libraries can be easily installed using apt:
```
apt install libexpat1 libuv1 libpcre2-8-0
```

The .deb package installs a configuration file at `/etc/siridb/siridb.conf`. You might want to view or change this file before starting SiriDB.

### Compile from source
>From version 2.0.19 libcleri is not included as part of this source anymore
>and needs to be installed separately. libcleri can be found here: 
>[https://github.com/transceptor-technology/libcleri](https://github.com/transceptor-technology/libcleri)

#### Linux
Install the following requirements: (Ubuntu)
```
sudo apt install libpcre2-dev
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
brew install pcre2
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
#### Configuration
SiriDB requires a configuration file to run. By default SiriDB will search for the configuration file in `/etc/siridb/siridb.conf` but alternatively you can specify a custom path by using the `-c/--config` argument.

```
$ siridb-server -c /my/path/siridb.conf
```

An example configuration file can be found here:
[https://github.com/transceptor-technology/siridb-server/blob/master/siridb.conf](#https://github.com/transceptor-technology/siridb-server/blob/master/siridb.conf)

## Create or expand a database
[SiriDB Admin](https://github.com/transceptor-technology/siridb-admin) is required for creating a new database or expanding an existing database with a new server. Documentation on how to install and use the admin tool can be found at the [siridb-admin](https://github.com/transceptor-technology/siridb-admin#readme) github project. Binaries are available for most platforms and can be downloaded from [here](https://github.com/transceptor-technology/siridb-admin/releases/latest).

## Using SiriDB
SiriDB has several tools available to connect to a SiriDB database. 

### SiriDB Connectors
The following native connectors are available:
 - [C/C++](https://github.com/transceptor-technology/libsiridb#readme)
 - [Python](https://github.com/transceptor-technology/siridb-connector#readme)
 - [Go](https://github.com/transceptor-technology/go-siridb-connector#readme)
 - [Node.js](https://github.com/transceptor-technology/siridb-nodejs-addon#readme)
 
When not using one of the above, you can still communicate to SiriDB using [SiriDB HTTP](#siridb-http).

### SiriDB HTTP
[SiriDB HTTP](https://github.com/transceptor-technology/siridb-http#readme) provides a HTTP API for SiriDB and has support for JSON, MsgPack, Qpack, CSV and Socket.io. SiriDB HTTP also has an optional web interface and SSL support.

### SiriDB Prompt
[SiriDB Prompt](https://github.com/transceptor-technology/siridb-prompt#readme) can be used as a command line SiriDB client with auto-completion support and can be used to load json or csv data into a SiriDB database. Click [here](https://github.com/transceptor-technology/siridb-prompt/blob/master/README.md) for more information about SiriDB Prompt.

### Grafana
[SiriDB Grafana Datasource](https://github.com/transceptor-technology/grafana-siridb-http-datasource#readme) is a plugin for Grafana. See the following blog article on how to configure and use this plugin: https://github.com/transceptor-technology/grafana-siridb-http-example.

## Query language
Documentation about the query language can be found at http://siridb.net/docs.
