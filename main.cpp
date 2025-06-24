/*
Main entry point for the co-simulation platform
Test both message and config systems
*/

#include "message.h"
#include "config.h"
#include <iostream>

int main() {
    std::cout << "Co-simulation platform starting..." << std::endl;
    
    // Test creating a basic message
    cosim::Message msg(cosim::MessageType::SYNC_REQUEST, 1.0);
    std::cout << "Message created with timestamp: " << msg.getTimestamp() << std::endl;
    
    // Test creating and using config
    cosim::Config config;
    std::cout << "Simulation time: " << config.getSimulationTime() << " seconds" << std::endl;
    std::cout << "Sync interval: " << config.getSyncInterval() << " seconds" << std::endl;
    std::cout << "NS-3 port: " << config.getNS3Config().port << std::endl;
    std::cout << "OMNeT++ port: " << config.getOMNeTConfig().port << std::endl;
    
    return 0;
}