################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/siri/admin/account.c \
../src/siri/admin/client.c \
../src/siri/admin/request.c

OBJS += \
./src/siri/admin/account.o \
./src/siri/admin/client.o \
./src/siri/admin/request.o

C_DEPS += \
./src/siri/admin/account.d \
./src/siri/admin/client.d \
./src/siri/admin/request.d


# Each subdirectory must supply rules for building sources it contributes
src/siri/args/%.o: ../src/siri/args/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I../include -O3 -Wall -Wextra $(CPPFLAGS) $(CFLAGS) -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


