################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/cfgparser/cfgparser.c

OBJS += \
./src/cfgparser/cfgparser.o

C_DEPS += \
./src/cfgparser/cfgparser.d


# Each subdirectory must supply rules for building sources it contributes
src/cfgparser/%.o: ../src/cfgparser/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -DDEBUG=1 -I../include -O0 -g3 -Wall -Wextra $(CPPFLAGS) $(CFLAGS) -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


