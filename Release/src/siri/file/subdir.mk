# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/siri/file/handler.c \
../src/siri/file/pointer.c

OBJS += \
./src/siri/file/handler.o \
./src/siri/file/pointer.o

C_DEPS += \
./src/siri/file/handler.d \
./src/siri/file/pointer.d


# Each subdirectory must supply rules for building sources it contributes
src/siri/file/%.o: ../src/siri/file/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	$(CC) -DNDEBUG -I../include -O3 -Wall -Wextra $(CPPFLAGS) $(CFLAGS) -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


