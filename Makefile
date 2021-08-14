
EXEC = uxn-debugger
BIN_DIR = $(PWD)/bin
BUILD_DIR = $(PWD)/build
SRC_DIRS = src

MICROUI_SRCS = microui/src/microui.c microui/demo/renderer.c
UXN_SRCS = uxn/src/uxn.c

SRCS = $(shell find $(SRC_DIRS) -name *.c) $(MICROUI_SRCS) $(UXN_SRCS)
OBJS = $(SRCS:%=$(BUILD_DIR)/%.o)
DEPS = $(OBJS:.o=.d)

INC_DIRS = microui/src microui/demo uxn/src
INC_FLAGS := $(addprefix -I,$(INC_DIRS))

CFLAGS = $(INC_FLAGS) -Wall -MMD -MP
LDFLAGS = `sdl2-config --libs` -lGL

CC = gcc

all: $(BIN_DIR)/$(EXEC)

$(BIN_DIR)/$(EXEC): $(OBJS)
	mkdir -p bin
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

$(BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

.PHONY: clean run

run: $(BIN_DIR)/$(EXEC)
	$(BIN_DIR)/$(EXEC) $(BIN_DIR)/console.rom

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)/$(EXEC)

-include $(DEPS)
