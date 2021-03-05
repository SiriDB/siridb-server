# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/base64/base64.c

OBJS += \
./src/base64/base64.o

C_DEPS += \
./src/base64/base64.d


# Each subdirectory must supply rules for building sources it contributes
src/base64/%.o: ../src/base64/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	$(CC) -DNDEBUG -I../include -O3 -Wall -Wextra $(CPPFLAGS) $(CFLAGS) -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


