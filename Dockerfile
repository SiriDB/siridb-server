FROM ubuntu:xenial
MAINTAINER transceptor-technology "support@transceptor.technology"
# Install SiriDB server
ENV SIRIDB_VERSION 2.0.12
RUN \
  apt-get update && \
  apt-get install --yes --force-yes build-essential libuv1-dev uuid-dev libpcre3-dev wget && \
  wget -O /tmp/siridb-server.tar.gz https://github.com/transceptor-technology/siridb-server/archive/${SIRIDB_VERSION}.tar.gz && \
  mkdir -p /opt/src/siridb-server && \
  tar xvzf /tmp/siridb-server.tar.gz -C /opt/src/siridb-server --strip-components=1 && \
  cd /opt/src/siridb-server/Release && \
  make clean && \
  make && \
  mkdir -p /etc/siridb && \
  cp /opt/src/siridb-server/siridb.conf /etc/siridb/siridb.conf
VOLUME ["/etc/siridb"] # config
VOLUME ["/var/lib/siridb/"]
RUN locale-gen en_US.UTF-8
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en
ENV LC_ALL en_US.UTF-8
ENTRYPOINT ["/opt/src/siridb-server/Release/siridb-server"]