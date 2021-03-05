# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/siri/db/access.c \
../src/siri/db/aggregate.c \
../src/siri/db/auth.c \
../src/siri/db/buffer.c \
../src/siri/db/db.c \
../src/siri/db/ffile.c \
../src/siri/db/fifo.c \
../src/siri/db/forward.c \
../src/siri/db/group.c \
../src/siri/db/groups.c \
../src/siri/db/initsync.c \
../src/siri/db/insert.c \
../src/siri/db/listener.c \
../src/siri/db/lookup.c \
../src/siri/db/median.c \
../src/siri/db/misc.c \
../src/siri/db/nodes.c \
../src/siri/db/pcache.c \
../src/siri/db/points.c \
../src/siri/db/pool.c \
../src/siri/db/pools.c \
../src/siri/db/presuf.c \
../src/siri/db/props.c \
../src/siri/db/queries.c \
../src/siri/db/query.c \
../src/siri/db/re.c \
../src/siri/db/reindex.c \
../src/siri/db/replicate.c \
../src/siri/db/series.c \
../src/siri/db/server.c \
../src/siri/db/servers.c \
../src/siri/db/shard.c \
../src/siri/db/shards.c \
../src/siri/db/sset.c \
../src/siri/db/tag.c \
../src/siri/db/tags.c \
../src/siri/db/tasks.c \
../src/siri/db/tee.c \
../src/siri/db/time.c \
../src/siri/db/user.c \
../src/siri/db/users.c \
../src/siri/db/variance.c \
../src/siri/db/walker.c

OBJS += \
./src/siri/db/access.o \
./src/siri/db/aggregate.o \
./src/siri/db/auth.o \
./src/siri/db/buffer.o \
./src/siri/db/db.o \
./src/siri/db/ffile.o \
./src/siri/db/fifo.o \
./src/siri/db/forward.o \
./src/siri/db/group.o \
./src/siri/db/groups.o \
./src/siri/db/initsync.o \
./src/siri/db/insert.o \
./src/siri/db/listener.o \
./src/siri/db/lookup.o \
./src/siri/db/median.o \
./src/siri/db/misc.o \
./src/siri/db/nodes.o \
./src/siri/db/pcache.o \
./src/siri/db/points.o \
./src/siri/db/pool.o \
./src/siri/db/pools.o \
./src/siri/db/presuf.o \
./src/siri/db/props.o \
./src/siri/db/queries.o \
./src/siri/db/query.o \
./src/siri/db/re.o \
./src/siri/db/reindex.o \
./src/siri/db/replicate.o \
./src/siri/db/series.o \
./src/siri/db/server.o \
./src/siri/db/servers.o \
./src/siri/db/shard.o \
./src/siri/db/shards.o \
./src/siri/db/sset.o \
./src/siri/db/tag.o \
./src/siri/db/tags.o \
./src/siri/db/tasks.o \
./src/siri/db/tee.o \
./src/siri/db/time.o \
./src/siri/db/user.o \
./src/siri/db/users.o \
./src/siri/db/variance.o \
./src/siri/db/walker.o

C_DEPS += \
./src/siri/db/access.d \
./src/siri/db/aggregate.d \
./src/siri/db/auth.d \
./src/siri/db/buffer.d \
./src/siri/db/db.d \
./src/siri/db/ffile.d \
./src/siri/db/fifo.d \
./src/siri/db/forward.d \
./src/siri/db/group.d \
./src/siri/db/groups.d \
./src/siri/db/initsync.d \
./src/siri/db/insert.d \
./src/siri/db/listener.d \
./src/siri/db/lookup.d \
./src/siri/db/median.d \
./src/siri/db/misc.d \
./src/siri/db/nodes.d \
./src/siri/db/pcache.d \
./src/siri/db/points.d \
./src/siri/db/pool.d \
./src/siri/db/pools.d \
./src/siri/db/presuf.d \
./src/siri/db/props.d \
./src/siri/db/queries.d \
./src/siri/db/query.d \
./src/siri/db/re.d \
./src/siri/db/reindex.d \
./src/siri/db/replicate.d \
./src/siri/db/series.d \
./src/siri/db/server.d \
./src/siri/db/servers.d \
./src/siri/db/shard.d \
./src/siri/db/shards.d \
./src/siri/db/sset.d \
./src/siri/db/tag.d \
./src/siri/db/tags.d \
./src/siri/db/tasks.d \
./src/siri/db/tee.d \
./src/siri/db/time.d \
./src/siri/db/user.d \
./src/siri/db/users.d \
./src/siri/db/variance.d \
./src/siri/db/walker.d


# Each subdirectory must supply rules for building sources it contributes
src/siri/db/%.o: ../src/siri/db/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	$(CC) -DNDEBUG -I../include -O3 -Wall -Wextra $(CPPFLAGS) $(CFLAGS) -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


