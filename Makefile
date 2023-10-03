CC = gcc
DEBUG = -g
CFLAGS_COMMON = -O2 -Wall -fPIC -c $(DEBUG)
CFLAGS = $(CFLAGS_COMMON)
CFLAGS_PLUGINS = $(CFLAGS_COMMON)
LFLAGS_COMMON = -O2 -Wall $(DEBUG)
LFLAGS = -lncurses -lz -ldl -lm $(LFLAGS_COMMON)
LFLAGS_PLUGINS = -shared $(LFLAGS_COMMON)

PROGRAM = chatsow

LOCAL = ~/.$(PROGRAM)/
SOURCE = source/
BUILD = build/
RELEASE = release/
PLUGINS = $(SOURCE)plugins/
BUILD_PLUGINS = $(BUILD)plugins/
RELEASE_PLUGINS = $(RELEASE)plugins/

THIS = Makefile

CFILES = $(wildcard $(SOURCE)*.c)
MODULES = $(patsubst $(SOURCE)%.c,%,$(CFILES))
OBJS = $(addprefix $(BUILD),$(addsuffix .o,$(MODULES)))
$(shell mkdir -p $(BUILD))

CFILES_PLUGINS = $(wildcard $(PLUGINS)*.c)
MODULES_PLUGINS = $(patsubst $(PLUGINS)%.c,%,$(CFILES_PLUGINS))
OBJS_PLUGINS = $(addprefix $(BUILD_PLUGINS),$(addsuffix .o,$(MODULES_PLUGINS)))
LIBS_PLUGINS = $(addprefix $(RELEASE_PLUGINS),$(addsuffix .so,$(MODULES_PLUGINS)))

$(shell mkdir -p $(BUILD))
$(shell mkdir -p $(BUILD_PLUGINS))

default: program plugins

program: $(RELEASE)$(PROGRAM)

plugins: $(LIBS_PLUGINS)

clean:
	rm -rf $(BUILD)

test: program plugins
	./$(RELEASE)$(PROGRAM)

loop:
	while true; do make test; done

updateloop:
	while true; do git pull && make test; done

$(RELEASE)$(PROGRAM): $(OBJS)
	$(CC) $^ $(LFLAGS) -o $@

define module_depender
$(shell touch $(BUILD)$(1).d)
$(shell makedepend -f $(BUILD)$(1).d -- $(CFLAGS) -- $(SOURCE)$(1).c
	2>/dev/null)
$(shell sed -i 's~^$(SOURCE)~$(BUILD)~g' $(BUILD)$(1).d)
-include $(BUILD)$(1).d
endef

define module_compiler
$(BUILD)$(1).o: $(SOURCE)$(1).c $(THIS)
	$(CC) $(CFLAGS) $$< -o $$@
endef

$(foreach module, $(MODULES), $(eval $(call module_depender,$(module))))
$(foreach module, $(MODULES), $(eval $(call module_compiler,$(module))))

define plugin_module_depender
$(shell touch $(BUILD_PLUGINS)$(1).d)
$(shell makedepend -f $(BUILD_PLUGINS)$(1).d -- $(CFLAGS_PLUGINS) -- $(PLUGINS)$(1).c
	2>/dev/null)
$(shell sed -i 's~^$(PLUGINS)~$(BUILD_PLUGINS)~g' $(BUILD_PLUGINS)$(1).d)
-include $(BUILD_PLUGINS)$(1).d
endef

define plugin_module_compiler
$(BUILD_PLUGINS)$(1).o: $(PLUGINS)$(1).c $(THIS)
	$(CC) $(CFLAGS_PLUGINS) $$< -o $$@
$(RELEASE_PLUGINS)$(1).so: $(BUILD_PLUGINS)$(1).o $(BUILD)import.o $(BUILD)utils.o $(BUILD)cs.o
	$(CC) $$^ $(LFLAGS_PLUGINS) -o $$@
endef

$(foreach module, $(MODULES_PLUGINS), $(eval $(call plugin_module_depender,$(module))))
$(foreach module, $(MODULES_PLUGINS), $(eval $(call plugin_module_compiler,$(module))))

.PHONY: default program plugins clean test loop updateloop
