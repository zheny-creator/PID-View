CC = gcc
CFLAGS = -Iinclude -Wall -Wextra -O3 -flto

BUILD_DIR = build/release
SRCS = $(wildcard src/*.c)
HEADERS = include/PID_view.h
TARGET = $(BUILD_DIR)/pid-view

all: $(TARGET)

$(TARGET): $(SRCS) $(HEADERS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)
	strip --strip-all $(TARGET)
	@du -h $(TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -rf build/

.PHONY: all clean