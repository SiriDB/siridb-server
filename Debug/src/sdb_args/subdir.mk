################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/sdb_args/sdb_args.c 

OBJS += \
./src/sdb_args/sdb_args.o 

C_DEPS += \
./src/sdb_args/sdb_args.d 


# Each subdirectory must supply rules for building sources it contributes
src/sdb_args/%.o: ../src/sdb_args/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I../include -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


