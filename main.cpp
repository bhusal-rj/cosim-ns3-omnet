/*
Main entry point for the co-simulation platform
Test with both mock simulators and real NS-3 integration
*/

#include "message.h"
#include "config.h"
#include "synchronizer.h"
#include "mock_simulators.h"
#include "ns3_adapter.h"
#include <iostream>
#include <memory>

int main(int argc, char* argv[]) {
    std::cout << "Co-simulation platform starting..." << std::endl;
    
    // Check command line arguments for simulation mode
    bool useRealNS3 = false;
    if (argc > 1 && std::string(argv[1]) == "--real-ns3") {
        useRealNS3 = true;
        std::cout << "Using real NS-3 integration" << std::endl;
    } else {
        std::cout << "Using mock simulators (use --real-ns3 for real NS-3)" << std::endl;
    }
    
    // Create configuration
    cosim::Config config;
    config.setSimulationTime(5.0);  // Run for 5 seconds
    config.setSyncInterval(0.5);    // Sync every 0.5 seconds
    
    std::cout << "\nConfiguration:" << std::endl;
    std::cout << "  Simulation time: " << config.getSimulationTime() << " seconds" << std::endl;
    std::cout << "  Sync interval: " << config.getSyncInterval() << " seconds" << std::endl;
    
    // Create synchronizer
    cosim::Synchronizer synchronizer(config);
    
    // Create simulators based on mode
    if (useRealNS3) {
        std::cout << "\n=== Setting up Real NS-3 Integration ===" << std::endl;
        
        // Create real NS-3 adapter
        auto ns3Simulator = std::make_shared<cosim::NS3Adapter>();
        
        // Configure NS-3 adapter (optional)
        // ns3Simulator->setNS3ScriptPath("./ns3-scripts/cosim-script.cc");
        // ns3Simulator->setNS3ConfigFile("./config/ns3-config.txt");
        
        // Create mock OMNeT++ (replace with real OMNeT++ adapter when available)
        auto omnetSimulator = std::make_shared<cosim::MockOMNeTSimulator>();
        
        // Add simulators to synchronizer
        synchronizer.addSimulator(ns3Simulator);
        synchronizer.addSimulator(omnetSimulator);
        
        std::cout << "Real NS-3 adapter and mock OMNeT++ simulator added" << std::endl;
        
    } else {
        std::cout << "\n=== Setting up Mock Simulators ===" << std::endl;
        
        // Create mock simulators for testing
        auto ns3Simulator = std::make_shared<cosim::MockNS3Simulator>();
        auto omnetSimulator = std::make_shared<cosim::MockOMNeTSimulator>();
        
        // Add simulators to synchronizer
        synchronizer.addSimulator(ns3Simulator);
        synchronizer.addSimulator(omnetSimulator);
        
        std::cout << "Mock NS-3 and OMNeT++ simulators added" << std::endl;
    }
    
    // Initialize and run simulation
    std::cout << "\n=== Initializing Simulators ===" << std::endl;
    if (synchronizer.initialize()) {
        std::cout << "\n=== Starting Co-Simulation ===" << std::endl;
        
        if (synchronizer.run()) {
            std::cout << "\n=== Co-Simulation Complete ===" << std::endl;
            std::cout << "Final simulation time: " << synchronizer.getCurrentTime() << " seconds" << std::endl;
        } else {
            std::cerr << "Co-simulation failed during execution" << std::endl;
            return 1;
        }
        
    } else {
        std::cerr << "Failed to initialize synchronizer" << std::endl;
        return 1;
    }
    
    return 0;
}