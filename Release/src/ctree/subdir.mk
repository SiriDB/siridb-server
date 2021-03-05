# Add inputs and outputs from these tool invocations to the build variables
C_SRCS += \
../src/ctree/ctree.c

OBJS += \
./src/ctree/ctree.o

C_DEPS += \
./src/ctree/ctree.d


# Each subdirectory must supply rules for building sources it contributes
src/ctree/%.o: ../src/ctree/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	$(CC) -DNDEBUG -I../include -O3 -Wall -Wextra $(CPPFLAGS) $(CFLAGS) -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


