/*
V2X-NDN-NFV Co-simulation Pl    std::cout << "Usage: " << programName << " [options]\n"
              << "Options:\n"
              << "  --real-ns3              Use real NS-3/ndnSIM simulation\n"
              << "  --real-omnet            Use real OMNeT++ orchestrator\n"
              << "  --ns3-example <n>    NS-3 example to run (default: ndn-grid)\n"
              << "  --omnet-config <cfg>    OMNeT++ configuration (default: KathmanduV2X)\n"
              << "  --traffic <density>     Traffic density: light|normal|heavy (default: normal)\n"
              << "  --sim-time <seconds>    Simulation duration (default: 120)\n"
              << "  --sync-interval <ms>    Sync interval in ms (default: 100)\n"
              << "  --port <port>           Server port for leader-follower communication (default: auto)\n"
              << "  --kathmandu             Use Kathmandu intersection scenario\n"
              << "  --help                  Show this help\n" Entry Point
Implements Leader-Follower architecture with OMNeT++ as time master
Based on simulation methodology document
*/

#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include <thread>
#include <vector>
#include <arpa/inet.h>
#include <unistd.h>

// Core co-simulation components
#include "src/common/config.h"
#include "src/common/synchronizer.h"
#include "src/common/message.h"
#include "src/common/leader_follower_synchronizer.h"

// Simulator adapters
#include "src/adapters/ns3_adapter.h"
#include "src/adapters/omnet_orchestrator.h"

// Mock simulators for testing
#include "src/common/mock_simulators.h"

using namespace cosim;

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [options]\n"
              << "Options:\n"
              << "  --real-ns3              Use real NS-3/ndnSIM simulation\n"
              << "  --real-omnet            Use real OMNeT++ orchestrator\n"
              << "  --ns3-example <name>    NS-3 example to run (default: ndn-grid)\n"
              << "  --omnet-config <cfg>    OMNeT++ configuration (default: KathmanduV2X)\n"
              << "  --traffic <density>     Traffic density: light|normal|heavy (default: normal)\n"
              << "  --sim-time <seconds>    Simulation duration (default: 120)\n"
              << "  --sync-interval <ms>    Sync interval in ms (default: 100)\n"
              << "  --kathmandu             Use Kathmandu intersection scenario\n"
              << "  --help                  Show this help\n"
              << "\nAvailable NS-3 examples:\n"
              << "  ndn-grid, ndn-simple, ndn-tree-tracers, ndn-congestion-topo-plugin\n"
              << "\nTraffic scenarios (Kathmandu intersection):\n"
              << "  light: 2-10 vehicles, normal: 10-25 vehicles, heavy: 25-50 vehicles\n"
              << std::endl;
}

bool findAvailablePort(int& port) {
    for (int testPort = port; testPort < port + 20; testPort++) {
        int testSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (testSocket < 0) continue;
        
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(testPort);
        
        int result = bind(testSocket, (struct sockaddr*)&addr, sizeof(addr));
        close(testSocket);
        
        if (result == 0) {
            port = testPort;
            return true;
        }
    }
    return false;
}

int main(int argc, char* argv[]) {
    std::cout << "=== V2X-NDN-NFV Co-simulation Platform ===" << std::endl;
    std::cout << "Leader-Follower Architecture: OMNeT++ (Leader) ↔ ndnSIM (Follower)" << std::endl;
    
    // Configuration parameters
    bool useRealNS3 = false;
    bool useRealOMNeT = false;
    bool useKathmanduScenario = false;
    std::string ns3Example = "ndn-grid";
    std::string omnetConfig = "KathmanduV2X";
    std::string trafficDensity = "normal";
    double simulationTime = 120.0;  // 2 minutes as per methodology
    double syncInterval = 0.1;      // 100ms for V2X requirements
    int serverPort = 0;             // 0 means auto-allocate
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--real-ns3") {
            useRealNS3 = true;
            std::cout << "✓ Using real NS-3/ndnSIM integration" << std::endl;
            
        } else if (arg == "--real-omnet") {
            useRealOMNeT = true;
            std::cout << "✓ Using real OMNeT++ NFV orchestrator" << std::endl;
            
        } else if (arg == "--ns3-example" && i + 1 < argc) {
            ns3Example = argv[++i];
            std::cout << "✓ NS-3 example: " << ns3Example << std::endl;
            
        } else if (arg == "--omnet-config" && i + 1 < argc) {
            omnetConfig = argv[++i];
            std::cout << "✓ OMNeT++ config: " << omnetConfig << std::endl;
            
        } else if (arg == "--traffic" && i + 1 < argc) {
            trafficDensity = argv[++i];
            std::cout << "✓ Traffic density: " << trafficDensity << std::endl;
            
        } else if (arg == "--sim-time" && i + 1 < argc) {
            simulationTime = std::stod(argv[++i]);
            std::cout << "✓ Simulation time: " << simulationTime << "s" << std::endl;
            
        } else if (arg == "--sync-interval" && i + 1 < argc) {
            syncInterval = std::stod(argv[++i]) / 1000.0; // Convert ms to seconds
            std::cout << "✓ Sync interval: " << (syncInterval * 1000) << "ms" << std::endl;
            
        } else if (arg == "--port" && i + 1 < argc) {
            serverPort = std::stoi(argv[++i]);
            std::cout << "✓ Server port: " << serverPort << std::endl;
            
        } else if (arg == "--kathmandu") {
            useKathmanduScenario = true;
            std::cout << "✓ Using Kathmandu intersection scenario" << std::endl;
            
        } else if (arg == "--help") {
            printUsage(argv[0]);
            return 0;
            
        } else {
            std::cerr << "Unknown option: " << arg << std::endl;
            printUsage(argv[0]);
            return 1;
        }
    }
    
    try {
        // Create configuration
        Config config;
        config.setSimulationTime(simulationTime);
        config.setSyncInterval(syncInterval);
        config.setTrafficDensity(trafficDensity);
        config.setScenarioType(useKathmanduScenario ? "kathmandu_intersection" : "generic");
        
        std::cout << "\n=== Configuration Summary ===" << std::endl;
        std::cout << "Simulation time: " << simulationTime << " seconds" << std::endl;
        std::cout << "Sync interval: " << (syncInterval * 1000) << " ms" << std::endl;
        std::cout << "Traffic density: " << trafficDensity << std::endl;
        std::cout << "Scenario: " << (useKathmanduScenario ? "Kathmandu intersection" : "Generic") << std::endl;
        
        // Create simulators based on configuration
        std::unique_ptr<SimulatorInterface> orchestrator;  // OMNeT++ (Leader)
        std::unique_ptr<SimulatorInterface> ndnSimulator;  // NS-3 (Follower)
        
        // Dynamic port allocation
        int dynamicPort = (serverPort > 0) ? serverPort : 9999;
        if (serverPort == 0 && !findAvailablePort(dynamicPort)) {
            std::cerr << "❌ No available ports found" << std::endl;
            return 1;
        }
        std::cout << "✅ Using port: " << dynamicPort << std::endl;
        
        // Initialize OMNeT++ Orchestrator (Leader)
        if (useRealOMNeT) {
            std::cout << "\n=== Initializing OMNeT++ NFV Orchestrator (Leader) ===" << std::endl;
            auto omnetOrch = std::make_unique<OMNeTOrchestrator>();
            omnetOrch->setTrafficDensity(trafficDensity);
            omnetOrch->setScenarioType(useKathmanduScenario ? "kathmandu_intersection" : "generic");
            // Start as leader with dynamic port
            if (!omnetOrch->startAsLeader(dynamicPort)) {
                std::cerr << "❌ Failed to start OMNeT++ orchestrator as leader" << std::endl;
                return 1;
            }
            orchestrator = std::move(omnetOrch);
        } else {
            std::cout << "\n=== Using Mock OMNeT++ Orchestrator (Leader) ===" << std::endl;
            orchestrator = std::make_unique<MockOMNeTSimulator>();
        }
        
        // Initialize NS-3/ndnSIM (Follower)
        if (useRealNS3) {
            std::cout << "\n=== Initializing NS-3/ndnSIM (Follower) ===" << std::endl;
            auto ns3Adapter = std::make_unique<NS3Adapter>();
            ns3Adapter->setNS3Example(ns3Example);
            ns3Adapter->setKathmanduScenario(useKathmanduScenario);
            ns3Adapter->setSyncInterval(syncInterval);
            ndnSimulator = std::move(ns3Adapter);
        } else {
            std::cout << "\n=== Using Mock NS-3 Simulator (Follower) ===" << std::endl;
            ndnSimulator = std::make_unique<MockNS3Simulator>();
        }
        
        // Create co-simulation synchronizer
        LeaderFollowerSynchronizer synchronizer(config);
        synchronizer.setLeader(orchestrator.get());
        synchronizer.setFollower(ndnSimulator.get());
        
        std::cout << "\n=== Initializing Co-simulation Framework ===" << std::endl;
        
        // Initialize simulators
        if (!synchronizer.initialize()) {
            std::cerr << "❌ Failed to initialize co-simulation framework" << std::endl;
            return 1;
        }
        
        std::cout << "✅ Co-simulation framework initialized successfully" << std::endl;
        
        // Start co-simulation
        std::cout << "\n=== Starting V2X-NDN-NFV Co-simulation ===" << std::endl;
        std::cout << "Duration: " << simulationTime << "s, Sync interval: " << (syncInterval * 1000) << "ms" << std::endl;
        
        auto startTime = std::chrono::steady_clock::now();
        
        if (synchronizer.run()) {
            auto endTime = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);
            
            std::cout << "\n=== Co-simulation Completed Successfully ===" << std::endl;
            std::cout << "✅ Simulation time: " << synchronizer.getCurrentTime() << " seconds" << std::endl;
            std::cout << "✅ Wall clock time: " << duration.count() << " seconds" << std::endl;
            std::cout << "✅ Time ratio: " << (simulationTime / duration.count()) << "x real-time" << std::endl;
            
            // Print performance summary
            synchronizer.printPerformanceSummary();
            
        } else {
            std::cerr << "❌ Co-simulation failed during execution" << std::endl;
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error during simulation: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Unknown error occurred during simulation" << std::endl;
        return 1;
    }
    
    std::cout << "\n🎉 V2X-NDN-NFV Co-simulation platform completed successfully!" << std::endl;
    return 0;
}
