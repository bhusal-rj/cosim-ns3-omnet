cmake_minimum_required(VERSION 3.10)
project(cosimulation)

# Add NIST ns3-cosim dependency
find_path(NS3_COSIM_INCLUDE_DIR 
    NAMES ns3-cosim/model/external-sync-manager.h
    PATHS ../contrib/ns3-cosim/model
)

find_library(NS3_COSIM_LIBRARY
    NAMES ns3-cosim
    PATHS ../contrib/ns3-cosim/build
)

# Include directories
include_directories(${NS3_COSIM_INCLUDE_DIR})
include_directories(include)

# Add executable
add_executable(cosimulation
    src/main.cpp
    src/adapters/ns3_adapter.cpp
    src/common/config.cpp
    src/common/synchronizer.cpp
    src/common/message.cpp
    src/common/mock_simulators.cpp
)

# Link libraries
target_link_libraries(cosimulation 
    ${NS3_COSIM_LIBRARY}
    pthread
)