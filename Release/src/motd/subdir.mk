################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/motd/motd.c

OBJS += \
./src/motd/motd.o

C_DEPS += \
./src/motd/motd.d


# Each subdirectory must supply rules for building sources it contributes
src/motd/%.o: ../src/motd/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I../include -O3 -Wall -Wextra $(CPPFLAGS) $(CFLAGS) -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


