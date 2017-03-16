################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/siri/admin/user.c \
../src/siri/admin/request.c

OBJS += \
./src/siri/admin/user.o \
./src/siri/admin/request.o

C_DEPS += \
./src/siri/admin/user.d \
./src/siri/admin/request.d


# Each subdirectory must supply rules for building sources it contributes
src/siri/admin/%.o: ../src/siri/admin/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -DDEBUG=1 -I../include -O0 -g3 -Wall $(CFLAGS) -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


