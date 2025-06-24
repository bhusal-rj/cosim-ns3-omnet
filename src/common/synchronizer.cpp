/*
Implementation of the Synchronizer class
Core logic for co-simulation coordination
*/

#include "synchronizer.h"
#include <iostream>
#include <algorithm>

namespace cosim {

Synchronizer::Synchronizer(const Config& config)
    : config_(config), currentTime_(0.0), running_(false), stepCount_(0) {
}

Synchronizer::~Synchronizer() {
    stop();
}

void Synchronizer::addSimulator(std::shared_ptr<SimulatorInterface> simulator) {
    simulators_.push_back(simulator);
    std::cout << "Added simulator to synchronizer" << std::endl;
}

bool Synchronizer::initialize() {
    std::cout << "Initializing synchronizer..." << std::endl;
    
    // Initialize all simulators
    for (auto& simulator : simulators_) {
        if (!simulator->initialize()) {
            std::cerr << "Failed to initialize simulator" << std::endl;
            return false;
        }
    }
    
    currentTime_ = 0.0;
    stepCount_ = 0;
    
    std::cout << "Synchronizer initialized successfully" << std::endl;
    return true;
}

bool Synchronizer::run() {
    if (simulators_.empty()) {
        std::cerr << "No simulators added to synchronizer" << std::endl;
        return false;
    }
    
    running_ = true;
    std::cout << "Starting co-simulation..." << std::endl;
    
    while (running_ && currentTime_ < config_.getSimulationTime()) {
        if (!synchronizeStep()) {
            std::cerr << "Synchronization failed at time " << currentTime_ << std::endl;
            running_ = false;
            return false;
        }
        
        exchangeVehicleData();
        
        currentTime_ += config_.getSyncInterval();
        stepCount_++;
        
        // Log progress every 10 steps
        if (stepCount_ % 10 == 0) {
            logProgress();
        }
    }
    
    std::cout << "Co-simulation completed successfully" << std::endl;
    return true;
}

void Synchronizer::stop() {
    if (running_) {
        running_ = false;
        std::cout << "Stopping co-simulation..." << std::endl;
        
        for (auto& simulator : simulators_) {
            simulator->shutdown();
        }
    }
}

bool Synchronizer::synchronizeStep() {
    // Step all simulators
    for (auto& simulator : simulators_) {
        if (!simulator->step(config_.getSyncInterval())) {
            return false;
        }
    }
    return true;
}

void Synchronizer::exchangeVehicleData() {
    if (simulators_.size() < 2) return;
    
    // Collect vehicle data from all simulators
    std::vector<VehicleInfo> allVehicles;
    
    for (auto& simulator : simulators_) {
        auto vehicles = simulator->getVehicleData();
        allVehicles.insert(allVehicles.end(), vehicles.begin(), vehicles.end());
    }
    
    // Update all simulators with combined vehicle data
    for (auto& simulator : simulators_) {
        simulator->updateVehicleData(allVehicles);
    }
}

void Synchronizer::logProgress() {
    std::cout << "Simulation time: " << currentTime_ << "s, Step: " << stepCount_ << std::endl;
}

} // namespace cosim