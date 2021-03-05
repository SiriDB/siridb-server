# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/qpack/qpack.c

OBJS += \
./src/qpack/qpack.o

C_DEPS += \
./src/qpack/qpack.d


# Each subdirectory must supply rules for building sources it contributes
src/qpack/%.o: ../src/qpack/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	$(CC) -DNDEBUG -I../include -O3 -Wall -Wextra $(CPPFLAGS) $(CFLAGS) -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


