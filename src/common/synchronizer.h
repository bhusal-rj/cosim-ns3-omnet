/*
Core synchronization component for co-simulation
Handles time coordination and data exchange between simulators
*/

#ifndef SYNCHRONIZER_H
#define SYNCHRONIZER_H

#include "message.h"
#include "config.h"
#include <vector>
#include <memory>

namespace cosim {

enum class SimulatorType {
    NS3,
    OMNET
};

class SimulatorInterface {
public:
    virtual ~SimulatorInterface() = default;
    
    virtual bool initialize() = 0;
    virtual bool step(double timeStep) = 0;
    virtual void shutdown() = 0;
    
    virtual std::vector<VehicleInfo> getVehicleData() = 0;
    virtual void updateVehicleData(const std::vector<VehicleInfo>& vehicles) = 0;
    
    virtual double getCurrentTime() const = 0;
    virtual bool isRunning() const = 0;
    virtual SimulatorType getType() const = 0;
};

class Synchronizer {
public:
    Synchronizer(const Config& config);
    ~Synchronizer();
    
    // Simulator management
    void addSimulator(std::shared_ptr<SimulatorInterface> simulator);
    
    // Simulation control
    bool initialize();
    bool run();
    void stop();
    
    // Status
    double getCurrentTime() const { return currentTime_; }
    bool isRunning() const { return running_; }

private:
    bool synchronizeStep();
    void exchangeVehicleData();
    void logProgress();
    
    Config config_;
    std::vector<std::shared_ptr<SimulatorInterface>> simulators_;
    double currentTime_;
    bool running_;
    int stepCount_;
};

} // namespace cosim

#endif // SYNCHRONIZER_H