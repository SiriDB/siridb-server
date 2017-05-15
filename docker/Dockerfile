FROM ubuntu:xenial
MAINTAINER transceptor-technology "support@transceptor.technology"
# Install SiriDB server
ENV SIRIDB_VERSION 2.0.16
RUN \
  apt-get update && \
  apt-get install --yes --force-yes build-essential libuv1-dev uuid-dev libpcre3-dev wget && \
  wget -P /tmp/ https://github.com/transceptor-technology/siridb-server/releases/download/${SIRIDB_VERSION}/siridb-server_${SIRIDB_VERSION}_amd64.deb && \
  dpkg -i /tmp/siridb-server_${SIRIDB_VERSION}_amd64.deb
  # mkdir -p /etc/siridb && \
  # cp /opt/src/siridb-server/siridb.conf /etc/siridb/siridb.conf
VOLUME ["/etc/siridb"] # config
VOLUME ["/var/lib/siridb/"]

# Client connections
EXPOSE 9000
# back-end connections
EXPOSE 9010

RUN locale-gen en_US.UTF-8
ENV LANG en_US.UTF-8
ENV LANGUAGE en_US:en
ENV LC_ALL en_US.UTF-8
ENTRYPOINT ["/usr/sbin/siridb-server"]
