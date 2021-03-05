# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/siri/net/bserver.c \
../src/siri/net/clserver.c \
../src/siri/net/pkg.c \
../src/siri/net/promise.c \
../src/siri/net/promises.c \
../src/siri/net/protocol.c \
../src/siri/net/stream.c \
../src/siri/net/tcp.c \
../src/siri/net/pipe.c

OBJS += \
./src/siri/net/bserver.o \
./src/siri/net/clserver.o \
./src/siri/net/pkg.o \
./src/siri/net/promise.o \
./src/siri/net/promises.o \
./src/siri/net/protocol.o \
./src/siri/net/stream.o \
./src/siri/net/tcp.o \
./src/siri/net/pipe.o

C_DEPS += \
./src/siri/net/bserver.d \
./src/siri/net/clserver.d \
./src/siri/net/pkg.d \
./src/siri/net/promise.d \
./src/siri/net/promises.d \
./src/siri/net/protocol.d \
./src/siri/net/stream.d \
./src/siri/net/tcp.d \
./src/siri/net/pipe.d


# Each subdirectory must supply rules for building sources it contributes
src/siri/net/%.o: ../src/siri/net/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	$(CC) -DNDEBUG -I../include -O3 -Wall -Wextra $(CPPFLAGS) $(CFLAGS) -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '
