# Co-simulation Makefile for NS-3 and OMNeT++

# Compiler Settings
CXX = g++
CXXFLAGS = -std=c++17 -Wall -g

# Directories
SRC_DIR = src
BUILD_DIR = build

# Include paths
INCLUDES = -I$(SRC_DIR)/common

# Source files
SOURCES = $(SRC_DIR)/common/message.cpp main.cpp

# Object files
OBJECTS = $(BUILD_DIR)/message.o $(BUILD_DIR)/main.o

# Target executable
TARGET = cosimulation

# Default target
all: $(BUILD_DIR) $(TARGET)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# Build target
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET)

# Compile message.cpp
$(BUILD_DIR)/message.o: $(SRC_DIR)/common/message.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Compile main.cpp
$(BUILD_DIR)/main.o: main.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Clean build files
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(TARGET)

.PHONY: all clean