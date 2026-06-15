CC      := clang
AR      ?= ar
STD     := -std=c11
WARN    := -Wall -Wextra
INCLUDE := -Iinclude

ifeq ($(DEBUG),1)
CFLAGS  := $(STD) $(WARN) $(INCLUDE) -O0 -g
else
CFLAGS  := $(STD) $(WARN) $(INCLUDE) -O2
endif

ENGINE_SRCS := $(wildcard src/engine/*.c)
ENGINE_OBJS := $(ENGINE_SRCS:.c=.o)

CLI_SRCS := $(wildcard src/cli/*.c)
CLI_OBJS := $(CLI_SRCS:.c=.o)

VIZ_SRCS := $(wildcard src/viz/*.c)
VIZ_OBJS := $(VIZ_SRCS:.c=.o)
VIZ_CFLAGS := $(if $(VIZ_SRCS),$(shell pkg-config --cflags raylib) $(if $(RAYGUI_INCLUDE),-I$(RAYGUI_INCLUDE)))

TARGETS := libtierra.a
ifneq ($(CLI_SRCS),)
TARGETS += tierra-cli
endif
ifneq ($(VIZ_SRCS),)
TARGETS += tierra-viz
endif

.PHONY: all debug clean
all: $(TARGETS)

debug:
	$(MAKE) clean
	$(MAKE) DEBUG=1 all

libtierra.a: $(ENGINE_OBJS)
	$(AR) rcs $@ $^

tierra-cli: $(CLI_OBJS) libtierra.a
	$(CC) $(CFLAGS) $(CLI_OBJS) -L. -ltierra -lm -o $@

tierra-viz: $(VIZ_OBJS) libtierra.a
	$(CC) $(CFLAGS) $(if $(RAYGUI_INCLUDE),-I$(RAYGUI_INCLUDE)) $(VIZ_OBJS) -L. -ltierra $$(pkg-config --libs raylib) -lm -o $@

src/viz/%.o: src/viz/%.c
	$(CC) $(CFLAGS) $(VIZ_CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(ENGINE_OBJS) $(CLI_OBJS) $(VIZ_OBJS) libtierra.a tierra-cli tierra-viz
