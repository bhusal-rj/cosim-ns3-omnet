/*
Real NS-3 adapter for co-simulation
Connects to actual NS-3/ndnSIM simulation
*/

#ifndef NS3_ADAPTER_H
#define NS3_ADAPTER_H

#include "synchronizer.h"
#include <string>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>

namespace cosim {

class NS3Adapter : public SimulatorInterface {
public:
    NS3Adapter(const std::string& configFile = "");
    ~NS3Adapter() override;
    
    // SimulatorInterface implementation
    bool initialize() override;
    bool step(double timeStep) override;
    void shutdown() override;
    
    std::vector<VehicleInfo> getVehicleData() override;
    void updateVehicleData(const std::vector<VehicleInfo>& vehicles) override;
    
    double getCurrentTime() const override { return currentTime_; }
    bool isRunning() const override { return running_; }
    SimulatorType getType() const override { return SimulatorType::NS3; }
    
    // NS-3 specific methods
    void setNS3ScriptPath(const std::string& scriptPath) { ns3ScriptPath_ = scriptPath; }
    void setNS3ConfigFile(const std::string& configFile) { ns3ConfigFile_ = configFile; }
    
private:
    // NS-3 process management
    bool startNS3Process();
    void stopNS3Process();
    bool isNS3ProcessRunning();
    
    // Communication with NS-3
    bool setupCommunication();
    void communicationLoop();
    bool sendCommand(const std::string& command);
    std::string receiveResponse();
    
    // Data conversion
    VehicleInfo parseVehicleData(const std::string& data);
    std::string formatVehicleUpdate(const VehicleInfo& vehicle);
    
    // Configuration
    std::string ns3ScriptPath_;
    std::string ns3ConfigFile_;
    std::string communicationPort_;
    
    // State
    std::atomic<double> currentTime_;
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    
    // Communication
    int socketFd_;
    std::thread communicationThread_;
    std::queue<std::string> messageQueue_;
    std::mutex queueMutex_;
    
    // Vehicle data
    std::vector<VehicleInfo> vehicles_;
    std::mutex vehiclesMutex_;
    
    // Process management
    pid_t ns3ProcessId_;
};

} // namespace cosim

#endif // NS3_ADAPTER_H