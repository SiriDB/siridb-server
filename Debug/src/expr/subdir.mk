################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/expr/expr.c 

OBJS += \
./src/expr/expr.o 

C_DEPS += \
./src/expr/expr.d 


# Each subdirectory must supply rules for building sources it contributes
src/expr/%.o: ../src/expr/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -DDEBUG=1 -I../include -O0 -g3 -Wall $(CFLAGS) $(LDFLAGS) -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


