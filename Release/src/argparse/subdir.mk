# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/argparse/argparse.c

OBJS += \
./src/argparse/argparse.o

C_DEPS += \
./src/argparse/argparse.d


# Each subdirectory must supply rules for building sources it contributes
src/argparse/%.o: ../src/argparse/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	$(CC) -DNDEBUG -I../include -O3 -Wall -Wextra $(CPPFLAGS) $(CFLAGS) -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


