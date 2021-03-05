# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/siri/api.c \
../src/siri/async.c \
../src/siri/backup.c \
../src/siri/buffersync.c \
../src/siri/err.c \
../src/siri/evars.c \
../src/siri/health.c \
../src/siri/heartbeat.c \
../src/siri/optimize.c \
../src/siri/siri.c \
../src/siri/version.c

OBJS += \
./src/siri/api.o \
./src/siri/async.o \
./src/siri/backup.o \
./src/siri/buffersync.o \
./src/siri/err.o \
./src/siri/evars.o \
./src/siri/health.o \
./src/siri/heartbeat.o \
./src/siri/optimize.o \
./src/siri/siri.o \
./src/siri/version.o

C_DEPS += \
./src/siri/api.d \
./src/siri/async.d \
./src/siri/backup.d \
./src/siri/buffersync.d \
./src/siri/err.d \
./src/siri/evars.d \
./src/siri/health.d \
./src/siri/heartbeat.d \
./src/siri/optimize.d \
./src/siri/siri.d \
./src/siri/version.d


# Each subdirectory must supply rules for building sources it contributes
src/siri/%.o: ../src/siri/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	$(CC) -DNDEBUG -I../include -O3 -Wall -Wextra $(CPPFLAGS) $(CFLAGS) -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


