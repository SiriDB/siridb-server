[![CI](https://github.com/SiriDB/siridb-server/workflows/CI/badge.svg)](https://github.com/SiriDB/siridb-server/actions)
[![Release Version](https://img.shields.io/github/release/SiriDB/siridb-server)](https://github.com/SiriDB/siridb-server/releases)

SiriDB Server
=============
SiriDB is a highly-scalable, robust and super fast time series database.

---------------------------------------
  * [Installation](#installation)
    * [Ubuntu](#ubuntu)
    * [Compile from source](#compile-from-source)
      * [Linux](#linux)
      * [OSX](#osx)
      * [Docker](#docker)
      * [Configuration](#configuration)
    * [Build Debian package](#build-debian-package)
    * [Run integration tests](#run-integration-tests)
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
For Ubuntu we have a deb package available which can be downloaded [here](https://github.com/SiriDB/siridb-server/releases/latest).

Note: SiriDB requires *libexpat1*, *libuv1*, *libpcre2-8-0* and *libcleri0* these libraries can be easily installed using apt:
```
apt install libexpat1 libuv1 libpcre2-8-0 libcleri0
```

>Library `libcleri0` is available from Ubuntu 18.04, for older versions a deb package can be found here:
>https://github.com/cesbit/libcleri/releases/latest

The .deb package installs a configuration file at `/etc/siridb/siridb.conf`. You might want to view or change this file before starting SiriDB.

### Compile from source
>From version 2.0.19 libcleri is not included as part of this source anymore
>and needs to be installed separately. libcleri can be found here:
>[https://github.com/cesbit/libcleri](https://github.com/cesbit/libcleri)
>or can be installed using `apt`.

#### Linux
Install the following requirements: (Ubuntu 18.04)
```
sudo apt install libcleri-dev
sudo apt install libpcre2-dev
sudo apt install libuv1-dev
sudo apt install libyajl-dev
sudo apt install uuid-dev
```

Compile (replace Release with Debug for a debug build):
```
cd ./Release
make clean
make test
make
```

Install
```
sudo make install
```

#### OSX
>Make sure [libcleri](https://github.com/cesbit/libcleri) is installed!

Install the following requirements:
```
brew install pcre2
brew install libuv
brew install yajl
brew install ossp-uuid
```
Compile (replace Release with Debug for a debug build):
```
cd ./Release
export CFLAGS="-I/usr/local/include"
export LDFLAGS="-L/usr/local/lib"
make clean
make test
make
```

Install
```
sudo make install
```

#### Docker

```bash
docker run \
    -d \
    -p 9000:9000 \
    -p 9080:9080 \
    -p 8080:8080 \
    -v ~/siridb-data:/var/lib/siridb \
    ghcr.io/siridb/siridb-server:latest    
```

#### Configuration
SiriDB accepts a configuration file or environment variable as configuration. By default SiriDB will search for the configuration file in `/etc/siridb/siridb.conf` but alternatively you can specify a custom path by using the `-c/--config` argument or use environment variable.

An example configuration file can be found here:
[https://github.com/SiriDB/siridb-server/blob/master/siridb.conf](https://github.com/SiriDB/siridb-server/blob/master/siridb.conf)

### Build Debian package:

Install required packages (*autopkgtest is required for running the tests*)
```
apt-get install devscripts lintian help2man autopkgtest
```

Create archive
```
git archive -o ../siridb-server_2.0.31.orig.tar.gz master
```

Run tests
```
autopkgtest -B -- null
```

Build package
```
debuild -us -uc
```

## Run integration tests
The simplest way to run the integration tests is to use [docker](https://docs.docker.com/install/).

Build integration test image
```
docker build -t siridb/itest -f itest/Dockerfile .
```

Run integration tests
```
docker run siridb/itest:latest
```

## Create or expand a database
[SiriDB Admin](https://github.com/SiriDB/siridb-admin) can be used for creating a new database or expanding an existing database with a new server. Documentation on how to install and use the admin tool can be found at the [siridb-admin](https://github.com/SiriDB/siridb-admin#readme) github project. Binaries are available for most platforms and can be downloaded from [here](https://github.com/SiriDB/siridb-admin/releases/latest). As an alternative it is possible to use a simple [HTTP API](https://docs.siridb.net/connect/http_api/) for creating or expanding a SiriDB database.

## Using SiriDB
SiriDB has several tools available to connect to a SiriDB database.

### SiriDB Connectors
The following native connectors are available:
 - [C/C++](https://github.com/SiriDB/libsiridb#readme)
 - [Python](https://github.com/SiriDB/siridb-connector#readme)
 - [Go](https://github.com/SiriDB/go-siridb-connector#readme)
 - [Node.js](https://github.com/SiriDB/siridb-nodejs-addon#readme)

When not using one of the above, you can still communicate to SiriDB using [SiriDB HTTP](#siridb-http).

### SiriDB HTTP
[SiriDB HTTP](https://github.com/SiriDB/siridb-http#readme) provides a HTTP API for SiriDB and has support for JSON, MsgPack, Qpack, CSV and Socket.io. SiriDB HTTP also has an optional web interface and SSL support.

### SiriDB Prompt
[SiriDB Prompt](https://github.com/SiriDB/siridb-prompt#readme) can be used as a command line SiriDB client with auto-completion support and can be used to load json or csv data into a SiriDB database. Click [here](https://github.com/SiriDB/siridb-prompt/blob/master/README.md) for more information about SiriDB Prompt.

### Grafana
[SiriDB Grafana Datasource](https://github.com/SiriDB/grafana-siridb-http-datasource#readme) is a plugin for Grafana. See the following blog article on how to configure and use this plugin: https://github.com/SiriDB/grafana-siridb-http-example.

## Query language
Documentation about the query language can be found at https://siridb.net/documentation.
