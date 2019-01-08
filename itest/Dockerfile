FROM ubuntu:18.04 as builder
RUN apt-get update && \
    apt-get install -y \
        libcleri-dev \
        libuv1-dev \
        libpcre2-dev \
        uuid-dev \
        build-essential
COPY ./main.c ./main.c
COPY ./src/ ./src/
COPY ./include/ ./include/
COPY ./Release/ ./Release/
RUN cd ./Release && \
    make clean && \
    CFLAGS="-Werror -std=gnu89" make

FROM python
RUN apt-get update && \
    apt-get install -y \
        valgrind \
        libuv1 \
        libpcre2-8-0 && \
    wget https://github.com/SiriDB/siridb-admin/releases/download/1.2.0/siridb-admin_1.2.0_linux_amd64.bin -O /usr/local/bin/siridb-admin && \
    chmod +x /usr/local/bin/siridb-admin
COPY --from=builder ./Release/siridb-server /Release/siridb-server
COPY --from=builder /usr/lib/x86_64-linux-gnu/libcleri* /usr/lib/x86_64-linux-gnu/
COPY ./itest/ /itest/
WORKDIR /itest
RUN pip install -r requirements.txt
CMD [ "python", "run_all.py", "-m", "-b=Release" ]
