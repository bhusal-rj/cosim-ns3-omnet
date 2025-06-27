# CoSim - Network Simulator Co-simulation Library

CoSim is a lightweight co-simulation library that enables seamless integration between ns-3 and OMNeT++ network simulators. This library allows researchers and developers to leverage the strengths of both simulators in a single simulation environment, combining ns-3's detailed protocol implementations with OMNeT++'s modular architecture and visualization capabilities.

## Features

- **Bidirectional Communication**: Real-time data exchange between ns-3 and OMNeT++ simulation instances
- **Synchronized Execution**: Coordinated simulation time management across both simulators
- **Flexible Integration**: Support for various network topologies and protocol stacks
- **Easy Configuration**: Simple API for setting up co-simulation scenarios

## Use Cases

- Hybrid network simulations combining different network segments
- Cross-validation of simulation results between simulators
- Leveraging specialized features from both ns-3 and OMNeT++
- Research scenarios requiring the unique capabilities of each simulator

# Project Structure and Usage Guide

## Folder Overview

### 1. `omnet_orchestrator/`
- **Purpose:** Contains the OMNeT++ orchestrator component, which acts as the master (leader) in the co-simulation.
- **Contents:**
  - `src/`: C++ source code for the Orchestrator module.
  - `simulations/`: NED files (network definitions), `omnetpp.ini` (simulation configuration), and any scenario-specific files.
  - `Makefile`: Build instructions for OMNeT++.
- **Usage:** Build and run the OMNeT++ orchestrator from this directory.

---

### 2. `src/adapters/`
- **Purpose:** Contains the C++ adapters for both OMNeT++ and ns-3 sides.
- **Contents:**
  - `omnet_orchestrator.cpp`/`.h`: Implements the OMNeT++ orchestration logic and TCP communication.
  - `ns3_adapter.cpp`/`.h`: Implements the ns-3/ndnSIM side of the co-simulation, handling synchronization and data exchange.
- **Usage:** These files are compiled as part of the orchestrator and ns-3/ndnSIM builds.

---

### 3. `ns3-scripts/`
- **Purpose:** Contains scripts and launchers for running the ns-3/ndnSIM simulation as the follower in the co-simulation.
- **Contents:**
  - `real-ndnsim-launcher.cc`: Main entry point for ns-3/ndnSIM in co-simulation mode.
- **Usage:** Used when launching the ns-3/ndnSIM side, either manually or via provided shell scripts.

---

### 4. Root Scripts
- **Purpose:** Provide easy, automated ways to launch the full co-simulation.
- **Files:**
  - `launch-full-integration.sh`: Launches both OMNeT++ and ns-3/ndnSIM in a basic integration scenario.
  - `launch-realistic-cosim.sh`: Launches both simulators in a realistic urban scenario (e.g., Kathmandu intersection).
- **Usage:** Run these scripts from the project root to start the co-simulation.

---

### 5. Documentation
- **Files:**
  - `README.md`: Project overview, folder explanations, and usage instructions.
  - `IMPLEMENTATION_SUMMARY.md`: Technical details about the architecture and implementation.

---

## How to Build and Run

### **1. Build the OMNeT++ Orchestrator**
```sh
cd omnet_orchestrator
make clean
make
```

### **2. Run the Co-simulation (Recommended)**
From the project root:
```sh
./launch-realistic-cosim.sh
```
or
```sh
./launch-full-integration.sh
```
These scripts will:
- Start the OMNeT++ orchestrator (master/leader)
- Start the ns-3/ndnSIM follower and connect it to OMNeT++

### **3. Manual Run (Advanced)**
- **Start OMNeT++ orchestrator:**
  ```sh
  ./omnet_orchestrator/omnet_orchestrator -n src:. -f omnet_orchestrator/simulations/omnetpp.ini
  ```
- **Start ns-3/ndnSIM follower (from its directory):**
  ```sh
  ./waf --run "real-ndnsim-launcher --leader=127.0.0.1 --port=<PORT> --time=120"
  ```

---

## Summary Table

| Folder/File         | Purpose                                                      |
|---------------------|-------------------------------------------------------------|
| omnet_orchestrator/ | OMNeT++ orchestrator code, NED files, and build scripts     |
| src/adapters/       | C++ adapters for OMNeT++ and ns-3/ndnSIM communication      |
| ns3-scripts/        | ns-3/ndnSIM launcher and integration scripts                |
| launch-*.sh         | Scripts to launch the full co-simulation                    |
| README.md           | Project overview and usage                                  |
| IMPLEMENTATION_SUMMARY.md | Technical and architectural details                   |

---
