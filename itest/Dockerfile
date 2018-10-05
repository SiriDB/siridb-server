FROM ubuntu:18.04
COPY ./main.c ./main.c
COPY ./src/ ./src/
COPY ./include/ ./include/
COPY ./Release/ ./Release/
RUN apt-get update && \
    apt-get install -y \
        libcleri-dev \
        libuv1-dev \
        libpcre2-dev \
        uuid-dev \
        build-essential
RUN cd ./Release && \
    make clean && \
    make

FROM python
RUN apt-get update && \
    apt-get install -y valgrind
COPY --from=0 ./Release/siridb-server /Release/siridb-server
COPY ./itest/ /itest/
WORKDIR /itest
RUN pip install -r requirements.txt
CMD [ "python", "test_select.py" ]