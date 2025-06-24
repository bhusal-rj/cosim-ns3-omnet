/*
Main entry point for the co-simulation platform
Test the complete synchronizer with mock simulators
*/

#include "message.h"
#include "config.h"
#include "synchronizer.h"
#include "mock_simulators.h"
#include <iostream>
#include <memory>

int main() {
    std::cout << "Co-simulation platform starting..." << std::endl;
    
    // Create configuration
    cosim::Config config;
    config.setSimulationTime(5.0);  // Run for 5 seconds
    config.setSyncInterval(0.5);    // Sync every 0.5 seconds
    
    std::cout << "Configuration:" << std::endl;
    std::cout << "  Simulation time: " << config.getSimulationTime() << " seconds" << std::endl;
    std::cout << "  Sync interval: " << config.getSyncInterval() << " seconds" << std::endl;
    
    // Create synchronizer
    cosim::Synchronizer synchronizer(config);
    
    // Create mock simulators
    auto ns3Simulator = std::make_shared<cosim::MockNS3Simulator>();
    auto omnetSimulator = std::make_shared<cosim::MockOMNeTSimulator>();
    
    // Add simulators to synchronizer
    synchronizer.addSimulator(ns3Simulator);
    synchronizer.addSimulator(omnetSimulator);
    
    // Initialize and run simulation
    if (synchronizer.initialize()) {
        std::cout << "\n=== Starting Co-Simulation ===" << std::endl;
        synchronizer.run();
        std::cout << "=== Co-Simulation Complete ===" << std::endl;
    } else {
        std::cerr << "Failed to initialize synchronizer" << std::endl;
        return 1;
    }
    
    return 0;
}