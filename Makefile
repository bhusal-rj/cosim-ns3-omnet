# V2X-NDN-NFV Co-simulation Makefile

# Compiler Settings
CXX = g++
CXXFLAGS = -std=c++17 -Wall -g -D_USE_MATH_DEFINES

# Directories
SRC_DIR = src
BUILD_DIR = build

# Include paths
INCLUDES = -I$(SRC_DIR)/common -I$(SRC_DIR)/adapters

# Libraries (added JSON support)
LIBS = -lm -lpthread -ljsoncpp

# Source files
COMMON_SOURCES = $(SRC_DIR)/common/message.cpp $(SRC_DIR)/common/config.cpp $(SRC_DIR)/common/synchronizer.cpp $(SRC_DIR)/common/mock_simulators.cpp $(SRC_DIR)/common/leader_follower_synchronizer.cpp
ADAPTER_SOURCES = $(SRC_DIR)/adapters/ns3_adapter.cpp $(SRC_DIR)/adapters/omnet_orchestrator.cpp
MAIN_SOURCE = main_v2x_nfv.cpp

SOURCES = $(COMMON_SOURCES) $(ADAPTER_SOURCES) $(MAIN_SOURCE)

# Object files
COMMON_OBJECTS = $(BUILD_DIR)/message.o $(BUILD_DIR)/config.o $(BUILD_DIR)/synchronizer.o $(BUILD_DIR)/mock_simulators.o $(BUILD_DIR)/leader_follower_synchronizer.o
ADAPTER_OBJECTS = $(BUILD_DIR)/ns3_adapter.o $(BUILD_DIR)/omnet_orchestrator.o
MAIN_OBJECT = $(BUILD_DIR)/main_v2x_nfv.o

OBJECTS = $(COMMON_OBJECTS) $(ADAPTER_OBJECTS) $(MAIN_OBJECT)

# Target executable
TARGET = v2x-ndn-nfv-cosim

# Default target
all: $(BUILD_DIR) $(TARGET)

# Create build directory
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)
	mkdir -p $(BUILD_DIR)/adapters

# Build target
$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LIBS)

# Compile common source files
$(BUILD_DIR)/message.o: $(SRC_DIR)/common/message.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/config.o: $(SRC_DIR)/common/config.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/synchronizer.o: $(SRC_DIR)/common/synchronizer.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/mock_simulators.o: $(SRC_DIR)/common/mock_simulators.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Compile adapter source files
$(BUILD_DIR)/ns3_adapter.o: $(SRC_DIR)/adapters/ns3_adapter.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/omnet_orchestrator.o: $(SRC_DIR)/adapters/omnet_orchestrator.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR)/leader_follower_synchronizer.o: $(SRC_DIR)/common/leader_follower_synchronizer.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Compile main.cpp
$(BUILD_DIR)/main_v2x_nfv.o: main_v2x_nfv.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

# Clean build files
clean:
	rm -rf $(BUILD_DIR)
	rm -f $(TARGET)

.PHONY: all clean