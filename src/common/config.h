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

    // V2X-specific configuration
    void setTrafficDensity(const std::string& density) { trafficDensity_ = density; }
    std::string getTrafficDensity() const { return trafficDensity_; }
    
    void setScenarioType(const std::string& scenario) { scenarioType_ = scenario; }
    std::string getScenarioType() const { return scenarioType_; }
    
    void setCoSimulationMode(const std::string& mode) { coSimMode_ = mode; }
    std::string getCoSimulationMode() const { return coSimMode_; }
    
    void loadDefaults() {
        simulationTime_ = 120.0; // 2 minutes for V2X scenarios
        syncInterval_ = 0.1;     // 100ms for fast V2X response
        trafficDensity_ = "normal";
        scenarioType_ = "intersection";
        coSimMode_ = "leader_follower";
    }

private:
    double simulationTime_;
    double syncInterval_;
    SimulatorConfig ns3Config_;
    SimulatorConfig omnetConfig_;
    // V2X-specific members
    std::string trafficDensity_;
    std::string scenarioType_;
    std::string coSimMode_;
};

} // namespace cosim

#endif // CONFIG_H