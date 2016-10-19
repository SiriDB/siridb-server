################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/sdb/args.c \
../src/sdb/grammar.c 

OBJS += \
./src/sdb/args.o \
./src/sdb/grammar.o 

C_DEPS += \
./src/sdb/args.d \
./src/sdb/grammar.d 


# Each subdirectory must supply rules for building sources it contributes
src/sdb/%.o: ../src/sdb/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I../include -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


