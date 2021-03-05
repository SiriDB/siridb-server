# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/procinfo/procinfo.c

OBJS += \
./src/procinfo/procinfo.o

C_DEPS += \
./src/procinfo/procinfo.d


# Each subdirectory must supply rules for building sources it contributes
src/procinfo/%.o: ../src/procinfo/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	$(CC) -DNDEBUG -I../include -O3 -Wall -Wextra $(CPPFLAGS) $(CFLAGS) -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


