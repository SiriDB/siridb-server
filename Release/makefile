-include ../makefile.init

RM := rm -rf

# All of the sources participating in the build are defined here
-include sources.mk
-include src/base64/subdir.mk
-include src/xpath/subdir.mk
-include src/xmath/subdir.mk
-include src/timeit/subdir.mk
-include src/xstr/subdir.mk
-include src/vec/subdir.mk
-include src/siri/service/subdir.mk
-include src/siri/parser/subdir.mk
-include src/siri/net/subdir.mk
-include src/siri/help/subdir.mk
-include src/siri/grammar/subdir.mk
-include src/siri/file/subdir.mk
-include src/siri/db/subdir.mk
-include src/siri/cfg/subdir.mk
-include src/siri/args/subdir.mk
-include src/siri/subdir.mk
-include src/qpack/subdir.mk
-include src/qpjson/subdir.mk
-include src/procinfo/subdir.mk
-include src/owcrypt/subdir.mk
-include src/motd/subdir.mk
-include src/logger/subdir.mk
-include src/lock/subdir.mk
-include src/llist/subdir.mk
-include src/iso8601/subdir.mk
-include src/lib/subdir.mk
-include src/imap/subdir.mk
-include src/omap/subdir.mk
-include src/expr/subdir.mk
-include src/ctree/subdir.mk
-include src/cfgparser/subdir.mk
-include src/cexpr/subdir.mk
-include src/argparse/subdir.mk
-include subdir.mk
-include objects.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(strip $(C_DEPS)),)
-include $(C_DEPS)
endif
endif

-include ../makefile.defs

OS := $(shell uname)
ifeq ($(OS),Darwin)
CRYPT :=
UUID :=
INSTALL_PATH := /usr/local
else
CRYPT := -lcrypt
UUID := -luuid
INSTALL_PATH := /usr
endif

# Add inputs and outputs from these tool invocations to the build variables

# All Target
all: siridb-server

# Tool invocations
siridb-server: $(OBJS) $(USER_OBJS)
	@echo 'Building target: $@'
	@echo 'Invoking: GCC C Linker'
	$(CC)  -o "siridb-server" $(OBJS) $(USER_OBJS) $(LDFLAGS) $(LIBS) $(CRYPT) $(UUID)
	@echo 'Finished building target: $@'
	@echo ' '

# Other Targets
clean:
	-$(RM) $(EXECUTABLES)$(OBJS)$(C_DEPS) siridb-server
	-@echo ' '

.PHONY: all clean dependents
.SECONDARY:

-include ../makefile.targets

test:
	@cd ../test && ./test.sh
