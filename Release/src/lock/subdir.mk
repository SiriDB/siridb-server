# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/lock/lock.c

OBJS += \
./src/lock/lock.o

C_DEPS += \
./src/lock/lock.d


# Each subdirectory must supply rules for building sources it contributes
src/lock/%.o: ../src/lock/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	$(CC) -DNDEBUG -I../include -O3 -Wall -Wextra $(CPPFLAGS) $(CFLAGS) -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


