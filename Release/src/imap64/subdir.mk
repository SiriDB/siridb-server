################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/imap64/imap64.c

OBJS += \
./src/imap64/imap64.o

C_DEPS += \
./src/imap64/imap64.d


# Each subdirectory must supply rules for building sources it contributes
src/imap64/%.o: ../src/imap64/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I../include -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


