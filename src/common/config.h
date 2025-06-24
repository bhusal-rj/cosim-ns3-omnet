/*
Configuration management for the co-simulation platform
Handles simulation parameters and settings
*/

#ifndef CONFIG_H
#define CONFIG_H

#include <string>

namespace cosim {

struct SimulatorConfig {
    std::string name;
    std::string host;
    int port;
    double stepSize;
    bool enabled;
};

class Config {
public:
    Config();
    ~Config() = default;
    
    // Simulation parameters
    double getSimulationTime() const { return simulationTime_; }
    double getSyncInterval() const { return syncInterval_; }
    
    // Simulator configurations
    const SimulatorConfig& getNS3Config() const { return ns3Config_; }
    const SimulatorConfig& getOMNeTConfig() const { return omnetConfig_; }
    
    // Setters
    void setSimulationTime(double time) { simulationTime_ = time; }
    void setSyncInterval(double interval) { syncInterval_ = interval; }

private:
    double simulationTime_;
    double syncInterval_;
    SimulatorConfig ns3Config_;
    SimulatorConfig omnetConfig_;
};

} // namespace cosim

#endif // CONFIG_H