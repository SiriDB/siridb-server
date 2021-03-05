# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/imap/imap.c

OBJS += \
./src/imap/imap.o

C_DEPS += \
./src/imap/imap.d


# Each subdirectory must supply rules for building sources it contributes
src/imap/%.o: ../src/imap/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	$(CC) -DNDEBUG -I../include -O3 -Wall -Wextra $(CPPFLAGS) $(CFLAGS) -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


