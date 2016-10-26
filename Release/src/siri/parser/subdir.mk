################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/siri/parser/listener.c \
../src/siri/parser/queries.c 

OBJS += \
./src/siri/parser/listener.o \
./src/siri/parser/queries.o 

C_DEPS += \
./src/siri/parser/listener.d \
./src/siri/parser/queries.d 


# Each subdirectory must supply rules for building sources it contributes
src/siri/parser/%.o: ../src/siri/parser/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I../include -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


