CC = gcc
CFLAGS = -Wall -Wextra -O2
LDFLAGS = -lpcap
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

# Clean the build and binary directories
clean:
	rm -f $(BIN_DIR)/$(TARGET)
	rm -rf $(BUILD_DIR)/*.o

.PHONY: all clean
