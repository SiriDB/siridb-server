# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/xmath/xmath.c

OBJS += \
./src/xmath/xmath.o

C_DEPS += \
./src/xmath/xmath.d


# Each subdirectory must supply rules for building sources it contributes
src/xmath/%.o: ../src/xmath/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	$(CC) -DNDEBUG -I../include -O3 -Wall -Wextra $(CPPFLAGS) $(CFLAGS) -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


