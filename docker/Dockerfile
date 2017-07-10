FROM alpine:latest
RUN apk update && \
		apk upgrade && \
		apk add gcc make libuv-dev musl-dev pcre-dev util-linux-dev linux-headers git && \
		cd /tmp && \
		git clone https://github.com/transceptor-technology/siridb-server.git && \
		cd siridb-server/Release && \
		make clean && \
		make

FROM alpine:latest
RUN apk update && \
		apk add pcre libuv libuuid && \
		mkdir -p /etc/siridb && \
		mkdir -p /var/lib/siridb && \
		mkdir -p /testme
COPY --from=0 /tmp/siridb-server/siridb.conf /etc/siridb/siridb.conf
COPY --from=0 /tmp/siridb-server/Release/siridb-server /usr/local/bin/
# Configuration
VOLUME ["/etc/siridb"] # config
# Data
VOLUME ["/var/lib/siridb/"]
# Client connections
EXPOSE 9000
# back-end connections
EXPOSE 9010
ENTRYPOINT ["/usr/local/bin/siridb-server"]
