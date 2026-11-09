LVGL_DIR := third_party/lvgl
BUILD_DIR := build

ARCH ?= $(shell uname -m)
ifneq ($(filter arm64 aarch64,$(ARCH)),)
override ARCH := arm64
endif

ifeq ($(ARCH),arm64)
ifeq ($(shell uname -m),aarch64)
CROSS_COMPILE ?=
else
CROSS_COMPILE ?= aarch64-linux-gnu-
endif
BIN_NAME := lvgl_clock_arm64
else
CROSS_COMPILE ?=
BIN_NAME := lvgl_clock
endif

CC := $(CROSS_COMPILE)gcc
AR := $(CROSS_COMPILE)ar
OBJ_DIR := $(BUILD_DIR)/obj-$(ARCH)
BIN := $(BUILD_DIR)/$(BIN_NAME)

GOALS := $(if $(MAKECMDGOALS),$(MAKECMDGOALS),all)

ifeq ($(ARCH),arm64)
ifneq ($(filter-out clean distclean test fetch,$(GOALS)),)
ifeq ($(shell command -v $(CC) >/dev/null 2>&1 && echo yes),)
$(error $(CC) not found; install gcc-aarch64-linux-gnu)
endif
endif
endif

CFLAGS ?= -O2 -Wall -Wextra
CPPFLAGS += -DLV_CONF_INCLUDE_SIMPLE -I. -I$(LVGL_DIR)
LDLIBS := -lm

LVGL_SRCS := $(shell find $(LVGL_DIR)/src -name '*.c' 2>/dev/null)
LVGL_OBJS := $(patsubst $(LVGL_DIR)/src/%.c,$(OBJ_DIR)/lvgl/%.o,$(LVGL_SRCS))
APP_OBJS := $(OBJ_DIR)/main.o $(OBJ_DIR)/clock_ui.o $(OBJ_DIR)/clock_math.o
LIBLVGL := $(OBJ_DIR)/liblvgl.a

.PHONY: all fetch liblvgl test clean distclean

all:
	@./fetch_lvgl.sh
	@$(MAKE) --no-print-directory $(BIN)

fetch:
	./fetch_lvgl.sh

liblvgl:
	@./fetch_lvgl.sh
	@$(MAKE) --no-print-directory $(LIBLVGL)

$(OBJ_DIR)/lvgl/%.o: $(LVGL_DIR)/src/%.c lv_conf.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(LIBLVGL): $(LVGL_OBJS)
	$(AR) rcs $@ $^

$(OBJ_DIR)/%.o: %.c clock_ui.h clock_math.h lv_conf.h
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BIN): $(APP_OBJS) $(LIBLVGL)
	$(CC) $(CFLAGS) -o $@ $(APP_OBJS) $(LIBLVGL) $(LDLIBS)

test:
	@mkdir -p $(BUILD_DIR)
	cc -O2 -Wall -Wextra -o $(BUILD_DIR)/test_clock_math clock_math.c test_clock_math.c -lm
	./$(BUILD_DIR)/test_clock_math

clean:
	rm -rf $(BUILD_DIR)

distclean: clean
	rm -rf third_party
