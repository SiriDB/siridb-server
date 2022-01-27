FROM amd64/alpine:3.13
RUN apk update && \
    apk upgrade && \
    apk add gcc make libuv-dev musl-dev pcre2-dev yajl-dev util-linux-dev linux-headers git && \
    git clone https://github.com/cesbit/libcleri.git /tmp/libcleri && \
    cd /tmp/libcleri/Release && \
    make all && \
    make install && \
    git clone https://github.com/SiriDB/siridb-server.git /tmp/siridb-server && \
    cd /tmp/siridb-server/Release && \
    make clean && \
    make

FROM alpine:latest
RUN apk update && \
    apk add pcre2 libuv libuuid yajl && \
    mkdir -p /etc/siridb && \
    mkdir -p /var/lib/siridb
COPY --from=0 /tmp/siridb-server/Release/siridb-server /usr/local/bin/
COPY --from=0 /usr/lib/libcleri* /usr/lib/

# Data
VOLUME ["/var/lib/siridb/"]
# Client (Socket) connections
EXPOSE 9000
# Server (Socket) connections
EXPOSE 9010
# Client (HTTP) connections
EXPOSE 9080
# Status connection
EXPOSE 8080

# Overwrite default configuration parameters
ENV SIRIDB_BIND_SERVER_ADDRESS 0.0.0.0
ENV SIRIDB_BIND_CLIENT_ADDRESS 0.0.0.0
ENV SIRIDB_HTTP_API_PORT 9080
ENV SIRIDB_HTTP_STATUS_PORT 8080
ENV SIRIDB_ENABLE_SHARD_COMPRESSION 1
ENV SIRIDB_ENABLE_SHARD_AUTO_DURATION 1
ENV SIRIDB_BUFFER_SYNC_INTERVAL 500

ENTRYPOINT ["/usr/local/bin/siridb-server"]
