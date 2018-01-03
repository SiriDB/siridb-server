################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/imap32/imap32.c

OBJS += \
./src/imap32/imap32.o

C_DEPS += \
./src/imap32/imap32.d


# Each subdirectory must supply rules for building sources it contributes
src/imap32/%.o: ../src/imap32/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I../include -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


