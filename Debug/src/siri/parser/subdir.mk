################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/siri/parser/listener.c \
../src/siri/parser/queries.c \
../src/siri/parser/walkers.c 

OBJS += \
./src/siri/parser/listener.o \
./src/siri/parser/queries.o \
./src/siri/parser/walkers.o 

C_DEPS += \
./src/siri/parser/listener.d \
./src/siri/parser/queries.d \
./src/siri/parser/walkers.d 


# Each subdirectory must supply rules for building sources it contributes
src/siri/parser/%.o: ../src/siri/parser/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -DDEBUG=1 -I"/home/joente/workspace/siridb-server/include" -O0 -g3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


