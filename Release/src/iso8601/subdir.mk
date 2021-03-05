# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/iso8601/iso8601.c

OBJS += \
./src/iso8601/iso8601.o

C_DEPS += \
./src/iso8601/iso8601.d


# Each subdirectory must supply rules for building sources it contributes
src/iso8601/%.o: ../src/iso8601/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	$(CC) -DNDEBUG -I../include -O3 -Wall -Wextra $(CPPFLAGS) $(CFLAGS) -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


