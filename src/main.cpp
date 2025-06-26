#include <iostream>
#include <memory>
#include <string>

// Forward declarations for simulator interfaces
namespace cosim {
    class SimulatorInterface;
    class NS3Adapter;
    class MockNS3Simulator;
    class MockOMNeTSimulator;
    class VehicleDataGenerator; // Forward declaration for vehicle data generator
}

// Find the main function and increase simulation time

int main(int argc, char* argv[]) {
    std::cout << "Co-simulation platform starting..." << std::endl;
    
    bool useRealNS3 = false;
    bool useRealOMNeT = false;  // Add OMNeT++ flag
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--real-ns3") {
            useRealNS3 = true;
            std::cout << "Using real NS-3 integration" << std::endl;
        } else if (std::string(argv[i]) == "--real-omnet") {
            useRealOMNeT = true;
            std::cout << "Using real OMNeT++ integration" << std::endl;
        } else if (std::string(argv[i]) == "--ndn-only") {
            // Run only NS-3/NDN simulation
            std::cout << "Running NDN-only simulation mode" << std::endl;
        } else if (std::string(argv[i]) == "--ndn-focus") {
            std::cout << "Running in NDN-focused mode" << std::endl;
            
            // Create enhanced NS-3 adapter
            ns3Simulator = std::make_unique<cosim::NS3Adapter>();
            
            // Use simplified vehicle generator instead of full OMNeT++
            omnetSimulator = std::make_unique<cosim::VehicleDataGenerator>();
            
            // Adjust sync interval for NDN simulation
            syncInterval = 0.1;  // Faster updates for NDN packet analysis
        }
    }
    
    // Configuration
    double simulationTime = 60.0;  // Increased from 5.0 to 60.0 seconds
    double syncInterval = 0.5;
    
    try {
        // Create simulators based on flags
        std::unique_ptr<cosim::SimulatorInterface> ns3Simulator;
        std::unique_ptr<cosim::SimulatorInterface> omnetSimulator;
        
        if (useRealNS3) {
            ns3Simulator = std::make_unique<cosim::NS3Adapter>();
        } else {
            ns3Simulator = std::make_unique<cosim::MockNS3Simulator>();
        }
        
        // For now, always use Mock OMNeT++ unless specifically requested
        if (useRealOMNeT) {
            // Future: Create real OMNeT++ adapter
            std::cout << "Real OMNeT++ not yet implemented, using mock" << std::endl;
            omnetSimulator = std::make_unique<cosim::MockOMNeTSimulator>();
        } else {
            omnetSimulator = std::make_unique<cosim::MockOMNeTSimulator>();
        }
        
        // ... rest of initialization and simulation loop
    } catch (const std::exception& e) {
        std::cerr << "Error during simulation: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}