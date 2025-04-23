CC = gcc
CFLAGS = -Wall -Wextra -O2 -I./include
LDFLAGS = -lpcap -lncurses
SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin
SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRC))
TARGET = $(BIN_DIR)/trafix

# The 'all' target ensures bin/ is created before anything else
all: $(BIN_DIR) $(TARGET)

# Build the target executable
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $(TARGET) $(LDFLAGS)

# Compile the object files
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Ensure build and bin directories exist
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# Install

install: install-bin #install-doc#

install-bin:
	mkdir -p $(DESTDIR)/usr/bin
	cp bin/trafix $(DESTDIR)/usr/bin/

install-doc:
	mkdir -p $(DESTDIR)/usr/share/doc/trafix
	cp LICENSE README.md $(DESTDIR)/usr/share/doc/trafix

uninstall:
	rm -f $(DESTDIR)/usr/bin/trafix
	rm -rf $(DESTDIR)/usr/share/doc/trafix

# Clean the build and binary directories
clean:
	rm -f $(TARGET)
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: all clean install install-bin install-doc uninstall
