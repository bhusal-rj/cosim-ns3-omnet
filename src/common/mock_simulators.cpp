/*
Implementation of mock simulators for testing
*/

#include "mock_simulators.h"
#include <iostream>
#include <sstream>
#include <cmath>

namespace cosim {

// Mock NS-3 Simulator Implementation
MockNS3Simulator::MockNS3Simulator() 
    : currentTime_(0.0), running_(false), rng_(42), 
      speedDist_(10.0, 30.0), positionDist_(-5.0, 5.0) {
}

bool MockNS3Simulator::initialize() {
    std::cout << "Initializing Mock NS-3 Simulator..." << std::endl;
    
    // Create some initial vehicles for NS-3
    VehicleInfo vehicle1;
    vehicle1.id = "ns3_vehicle_001";
    vehicle1.x = 100.0;
    vehicle1.y = 50.0;
    vehicle1.z = 0.0;
    vehicle1.speed = 20.0;
    vehicle1.heading = 0.0;
    vehicle1.timestamp = 0.0;
    
    VehicleInfo vehicle2;
    vehicle2.id = "ns3_vehicle_002";
    vehicle2.x = 200.0;
    vehicle2.y = 100.0;
    vehicle2.z = 0.0;
    vehicle2.speed = 25.0;
    vehicle2.heading = 45.0;
    vehicle2.timestamp = 0.0;
    
    vehicles_.push_back(vehicle1);
    vehicles_.push_back(vehicle2);
    
    running_ = true;
    currentTime_ = 0.0;
    
    std::cout << "Mock NS-3 Simulator initialized with " << vehicles_.size() << " vehicles" << std::endl;
    return true;
}

bool MockNS3Simulator::step(double timeStep) {
    if (!running_) return false;
    
    currentTime_ += timeStep;
    
    // Update vehicle positions (simple movement simulation)
    for (auto& vehicle : vehicles_) {
        // Move vehicles based on speed and heading
        double radians = vehicle.heading * M_PI / 180.0;
        vehicle.x += vehicle.speed * timeStep * cos(radians);
        vehicle.y += vehicle.speed * timeStep * sin(radians);
        
        // Add some random variation
        vehicle.speed += positionDist_(rng_) * 0.1;
        vehicle.heading += positionDist_(rng_) * 2.0;
        
        // Keep speed within reasonable bounds
        if (vehicle.speed < 5.0) vehicle.speed = 5.0;
        if (vehicle.speed > 35.0) vehicle.speed = 35.0;
        
        vehicle.timestamp = currentTime_;
    }
    
    return true;
}

void MockNS3Simulator::shutdown() {
    running_ = false;
    std::cout << "Mock NS-3 Simulator shutdown" << std::endl;
}

std::vector<VehicleInfo> MockNS3Simulator::getVehicleData() {
    return vehicles_;
}

void MockNS3Simulator::updateVehicleData(const std::vector<VehicleInfo>& vehicles) {
    // In a real implementation, this would update the NS-3 simulation
    // For now, just log that we received data
    std::cout << "NS-3 received " << vehicles.size() << " vehicle updates" << std::endl;
}

// Mock OMNeT++ Simulator Implementation
MockOMNeTSimulator::MockOMNeTSimulator() 
    : currentTime_(0.0), running_(false), rng_(24), 
      speedDist_(15.0, 25.0), positionDist_(-3.0, 3.0) {
}

bool MockOMNeTSimulator::initialize() {
    std::cout << "Initializing Mock OMNeT++ Simulator..." << std::endl;
    
    // Create some initial vehicles for OMNeT++
    VehicleInfo vehicle1;
    vehicle1.id = "omnet_vehicle_001";
    vehicle1.x = 150.0;
    vehicle1.y = 75.0;
    vehicle1.z = 0.0;
    vehicle1.speed = 18.0;
    vehicle1.heading = 90.0;
    vehicle1.timestamp = 0.0;
    
    vehicles_.push_back(vehicle1);
    
    running_ = true;
    currentTime_ = 0.0;
    
    std::cout << "Mock OMNeT++ Simulator initialized with " << vehicles_.size() << " vehicles" << std::endl;
    return true;
}

bool MockOMNeTSimulator::step(double timeStep) {
    if (!running_) return false;
    
    currentTime_ += timeStep;
    
    // Update vehicle positions
    for (auto& vehicle : vehicles_) {
        double radians = vehicle.heading * M_PI / 180.0;
        vehicle.x += vehicle.speed * timeStep * cos(radians);
        vehicle.y += vehicle.speed * timeStep * sin(radians);
        
        // Add some variation
        vehicle.speed += positionDist_(rng_) * 0.05;
        vehicle.heading += positionDist_(rng_) * 1.0;
        
        // Keep within bounds
        if (vehicle.speed < 10.0) vehicle.speed = 10.0;
        if (vehicle.speed > 30.0) vehicle.speed = 30.0;
        
        vehicle.timestamp = currentTime_;
    }
    
    return true;
}

void MockOMNeTSimulator::shutdown() {
    running_ = false;
    std::cout << "Mock OMNeT++ Simulator shutdown" << std::endl;
}

std::vector<VehicleInfo> MockOMNeTSimulator::getVehicleData() {
    return vehicles_;
}

void MockOMNeTSimulator::updateVehicleData(const std::vector<VehicleInfo>& vehicles) {
    std::cout << "OMNeT++ received " << vehicles.size() << " vehicle updates" << std::endl;
}

} // namespace cosim