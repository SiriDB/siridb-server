FROM alpine:latest
RUN apk update && \
    apk upgrade && \
    apk add gcc make libuv-dev musl-dev pcre2-dev util-linux-dev linux-headers git && \
    git clone https://github.com/transceptor-technology/libcleri.git /tmp/libcleri && \
    cd /tmp/libcleri/Release && \
    make all && \
    make install && \
    git clone https://github.com/transceptor-technology/siridb-server.git /tmp/siridb-server && \
    cd /tmp/siridb-server/Release && \
    make clean && \
    make

FROM alpine:latest
RUN apk update && \
    apk add pcre2 libuv libuuid && \
    mkdir -p /etc/siridb && \
    mkdir -p /var/lib/siridb
COPY --from=0 /tmp/siridb-server/siridb.conf /etc/siridb/siridb.conf
COPY --from=0 /tmp/siridb-server/Release/siridb-server /usr/local/bin/
COPY --from=0 /usr/lib/libcleri* /usr/lib/

# Configuration
VOLUME ["/etc/siridb"] # config
# Data
VOLUME ["/var/lib/siridb/"]
# Client connections
EXPOSE 9000
# back-end connections
EXPOSE 9010
ENTRYPOINT ["/usr/local/bin/siridb-server"]
