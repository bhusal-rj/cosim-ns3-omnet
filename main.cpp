/*
Main entry point for the co-simulation platform
Test message, config, and vehicle data systems
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
    
    // Test vehicle message system
    cosim::VehicleMessage vehicleMsg(2.5);
    
    // Add some test vehicles
    cosim::VehicleInfo vehicle1;
    vehicle1.id = "vehicle_001";
    vehicle1.x = 100.0;
    vehicle1.y = 200.0;
    vehicle1.z = 0.0;
    vehicle1.speed = 15.5;
    vehicle1.heading = 90.0;
    vehicle1.timestamp = 2.5;
    
    cosim::VehicleInfo vehicle2;
    vehicle2.id = "vehicle_002";
    vehicle2.x = 150.0;
    vehicle2.y = 180.0;
    vehicle2.z = 0.0;
    vehicle2.speed = 12.3;
    vehicle2.heading = 45.0;
    vehicle2.timestamp = 2.5;
    
    vehicleMsg.addVehicle(vehicle1);
    vehicleMsg.addVehicle(vehicle2);
    
    std::cout << "\nVehicle message created with " << vehicleMsg.getVehicleCount() << " vehicles:" << std::endl;
    for (const auto& vehicle : vehicleMsg.getVehicles()) {
        std::cout << "  Vehicle " << vehicle.id << " at (" << vehicle.x << ", " << vehicle.y 
                  << ") speed: " << vehicle.speed << " m/s" << std::endl;
    }
    
    return 0;
}