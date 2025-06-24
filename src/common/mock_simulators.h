/*
Mock simulators for testing the synchronization framework
Simulates NS-3 and OMNeT++ behavior without actual integration
*/

#ifndef MOCK_SIMULATORS_H
#define MOCK_SIMULATORS_H

#include "synchronizer.h"
#include <random>

namespace cosim {

class MockNS3Simulator : public SimulatorInterface {
public:
    MockNS3Simulator();
    ~MockNS3Simulator() override = default;
    
    bool initialize() override;
    bool step(double timeStep) override;
    void shutdown() override;
    
    std::vector<VehicleInfo> getVehicleData() override;
    void updateVehicleData(const std::vector<VehicleInfo>& vehicles) override;
    
    double getCurrentTime() const override { return currentTime_; }
    bool isRunning() const override { return running_; }
    SimulatorType getType() const override { return SimulatorType::NS3; }

private:
    double currentTime_;
    bool running_;
    std::vector<VehicleInfo> vehicles_;
    std::mt19937 rng_;
    std::uniform_real_distribution<double> speedDist_;
    std::uniform_real_distribution<double> positionDist_;
};

class MockOMNeTSimulator : public SimulatorInterface {
public:
    MockOMNeTSimulator();
    ~MockOMNeTSimulator() override = default;
    
    bool initialize() override;
    bool step(double timeStep) override;
    void shutdown() override;
    
    std::vector<VehicleInfo> getVehicleData() override;
    void updateVehicleData(const std::vector<VehicleInfo>& vehicles) override;
    
    double getCurrentTime() const override { return currentTime_; }
    bool isRunning() const override { return running_; }
    SimulatorType getType() const override { return SimulatorType::OMNET; }

private:
    double currentTime_;
    bool running_;
    std::vector<VehicleInfo> vehicles_;
    std::mt19937 rng_;
    std::uniform_real_distribution<double> speedDist_;
    std::uniform_real_distribution<double> positionDist_;
};

} // namespace cosim

#endif // MOCK_SIMULATORS_H