################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/siri/async.c \
../src/siri/backup.c \
../src/siri/err.c \
../src/siri/heartbeat.c \
../src/siri/optimize.c \
../src/siri/siri.c \
../src/siri/version.c

OBJS += \
./src/siri/async.o \
./src/siri/backup.o \
./src/siri/err.o \
./src/siri/heartbeat.o \
./src/siri/optimize.o \
./src/siri/siri.o \
./src/siri/version.o

C_DEPS += \
./src/siri/async.d \
./src/siri/backup.d \
./src/siri/err.d \
./src/siri/heartbeat.d \
./src/siri/optimize.d \
./src/siri/siri.d \
./src/siri/version.d


# Each subdirectory must supply rules for building sources it contributes
src/siri/%.o: ../src/siri/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I../include -O3 -Wall -Wextra $(CPPFLAGS) $(CFLAGS) -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


