################################################################################
# Automatically-generated file. Do not edit!
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../src/cleri/children.c \
../src/cleri/choice.c \
../src/cleri/expecting.c \
../src/cleri/grammar.c \
../src/cleri/keyword.c \
../src/cleri/kwcache.c \
../src/cleri/list.c \
../src/cleri/node.c \
../src/cleri/object.c \
../src/cleri/olist.c \
../src/cleri/optional.c \
../src/cleri/parse.c \
../src/cleri/prio.c \
../src/cleri/regex.c \
../src/cleri/repeat.c \
../src/cleri/rule.c \
../src/cleri/sequence.c \
../src/cleri/this.c \
../src/cleri/token.c \
../src/cleri/tokens.c 

OBJS += \
./src/cleri/children.o \
./src/cleri/choice.o \
./src/cleri/expecting.o \
./src/cleri/grammar.o \
./src/cleri/keyword.o \
./src/cleri/kwcache.o \
./src/cleri/list.o \
./src/cleri/node.o \
./src/cleri/object.o \
./src/cleri/olist.o \
./src/cleri/optional.o \
./src/cleri/parse.o \
./src/cleri/prio.o \
./src/cleri/regex.o \
./src/cleri/repeat.o \
./src/cleri/rule.o \
./src/cleri/sequence.o \
./src/cleri/this.o \
./src/cleri/token.o \
./src/cleri/tokens.o 

C_DEPS += \
./src/cleri/children.d \
./src/cleri/choice.d \
./src/cleri/expecting.d \
./src/cleri/grammar.d \
./src/cleri/keyword.d \
./src/cleri/kwcache.d \
./src/cleri/list.d \
./src/cleri/node.d \
./src/cleri/object.d \
./src/cleri/olist.d \
./src/cleri/optional.d \
./src/cleri/parse.d \
./src/cleri/prio.d \
./src/cleri/regex.d \
./src/cleri/repeat.d \
./src/cleri/rule.d \
./src/cleri/sequence.d \
./src/cleri/this.d \
./src/cleri/token.d \
./src/cleri/tokens.d 


# Each subdirectory must supply rules for building sources it contributes
src/cleri/%.o: ../src/cleri/%.c
	@echo 'Building file: $<'
	@echo 'Invoking: GCC C Compiler'
	gcc -I../include -O3 -Wall -c -fmessage-length=0 -MMD -MP -MF"$(@:%.o=%.d)" -MT"$(@)" -o "$@" "$<"
	@echo 'Finished building: $<'
	@echo ' '


