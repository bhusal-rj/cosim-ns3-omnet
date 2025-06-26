/*
OMNeT++ NFV Orchestrator - Leader in Co-simulation
Implements NFV MANO functionality with NDN-aware decision making
Based on simulation methodology document
*/

#ifndef OMNET_ORCHESTRATOR_H
#define OMNET_ORCHESTRATOR_H

#include "../common/synchronizer.h"
#include "../common/message.h"
#include <string>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <map>
#include <vector>
#include <functional>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

namespace cosim {

// VNF Types as defined in methodology
enum class VNFType {
    NDN_ROUTER,
    TRAFFIC_ANALYZER, 
    SECURITY_VNF,
    CACHE_OPTIMIZER
};

// NFV Decision structure
struct NFVDecision {
    VNFType vnfType;
    std::string action; // SCALE_UP, SCALE_DOWN, MIGRATE, OPTIMIZE
    int targetInstances;
    std::string sourceLocation;
    std::string targetLocation;
    std::string reason;
    double timestamp;
    int priority; // 1=critical, 2=high, 3=normal
};

// VNF Instance state
struct VNFInstance {
    std::string instanceId;
    VNFType type;
    std::string location;
    double cpuUsage;
    double memoryUsage;
    double networkLoad;
    bool isActive;
    std::chrono::steady_clock::time_point createdAt;
};

// Co-simulation message types
struct CoSimMessage {
    enum Type {
        TIME_SYNC,
        NDN_METRICS,
        NFV_COMMAND,
        VEHICLE_UPDATE,
        EMERGENCY_EVENT
    } type;
    
    double timestamp;
    std::string payload;
    int priority;
};

// OMNeT++ Orchestrator (Leader in co-simulation)
class OMNeTOrchestrator : public SimulatorInterface {
public:
    OMNeTOrchestrator();
    ~OMNeTOrchestrator() override;
    
    // SimulatorInterface implementation
    bool initialize() override;
    bool step(double timeStep) override;
    void shutdown() override;
    
    std::vector<VehicleInfo> getVehicleData() override;
    void updateVehicleData(const std::vector<VehicleInfo>& vehicles) override;
    
    double getCurrentTime() const override { return currentTime_.load(); }
    bool isRunning() const override { return running_.load(); }
    SimulatorType getType() const override { return SimulatorType::OMNET; }
    
    // Leader-specific methods (Time Master)
    bool startAsLeader(int port = 9999);
    bool sendTimeSyncCommand(double nextTime);
    bool waitForFollowerAck();
    void handleFollowerMetrics(const NDNMetrics& metrics);
    
    // NFV Orchestration methods
    std::vector<NFVDecision> analyzeAndDecide(const NDNMetrics& metrics);
    void executeNFVDecisions(const std::vector<NFVDecision>& decisions);
    bool deployVNF(VNFType type, const std::string& location);
    bool scaleVNF(VNFType type, int targetInstances);
    bool migrateVNF(const std::string& instanceId, const std::string& targetLocation);
    void optimizeVNFPlacement();
    
    // Configuration
    void setTrafficDensity(const std::string& density) { trafficDensity_ = density; }
    void setScenarioType(const std::string& scenario) { scenarioType_ = scenario; }
    void setKathmanduScenario(bool enable) { useKathmanduScenario_ = enable; }
    
    // Monitoring and metrics
    void printNFVStatus() const;
    void exportMetrics(const std::string& filename) const;
    
private:
    // TCP Server for leader role
    bool setupLeaderServer();
    void leaderLoop();
    void handleFollowerConnection(int clientSocket);
    bool sendMessage(int socket, const CoSimMessage& message);
    CoSimMessage receiveMessage(int socket);
    
    // NFV Decision Logic (from methodology)
    bool shouldScaleUp(const NDNMetrics& metrics, VNFType vnfType);
    bool shouldScaleDown(const NDNMetrics& metrics, VNFType vnfType);
    bool shouldMigrate(const NDNMetrics& metrics, VNFType vnfType);
    std::string findOptimalLocation(const NDNMetrics& metrics, VNFType vnfType);
    int calculateRequiredInstances(const NDNMetrics& metrics, VNFType vnfType);
    
    // Kathmandu scenario specific
    void initializeKathmanduTopology();
    void generateKathmanduTraffic();
    void simulateIntersectionBehavior(double timeStep);
    
    // Vehicle simulation for OMNeT++
    void updateVehiclePositions(double timeStep);
    void generateV2XMessages();
    void handleEmergencyScenarios();
    
    // Performance monitoring
    void updatePerformanceMetrics();
    void logDecisionMaking(const NFVDecision& decision);
    
    // Message parsing
    NDNMetrics parseNDNMetrics(const std::string& jsonStr);
    
    // Thresholds from methodology
    static constexpr uint32_t PIT_SCALE_THRESHOLD = 100;
    static constexpr double LATENCY_THRESHOLD = 0.1; // 100ms
    static constexpr double CACHE_EFFICIENCY_THRESHOLD = 0.5;
    static constexpr double CPU_SCALE_UP_THRESHOLD = 0.8;
    static constexpr double CPU_SCALE_DOWN_THRESHOLD = 0.3;
    static constexpr double EMERGENCY_LATENCY_THRESHOLD = 0.05; // 50ms for safety
    
    // State management
    std::atomic<double> currentTime_;
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    std::atomic<bool> leaderReady_;
    std::atomic<bool> followerConnected_;
    
    // Leader communication
    int serverSocket_;
    int followerSocket_;
    std::thread leaderThread_;
    std::mutex communicationMutex_;
    
    // Vehicle simulation state
    std::vector<VehicleInfo> vehicles_;
    std::mutex vehiclesMutex_;
    
    // NFV State tracking
    std::map<VNFType, std::vector<VNFInstance>> vnfInstances_;
    std::map<std::string, std::string> vnfLocations_; // instanceId -> location
    std::mutex nfvStateMutex_;
    
    // Kathmandu scenario data
    struct KathmanduIntersection {
        double x, y; // Position
        std::vector<std::string> approaches; // N, S, E, W
        int currentPhase; // Traffic light phase
        double phaseTimer;
        std::queue<std::string> waitingVehicles;
    } kathmanduIntersection_;
    
    // Configuration
    std::string trafficDensity_;
    std::string scenarioType_;
    bool useKathmanduScenario_;
    int serverPort_;
    
    // Performance tracking
    struct PerformanceMetrics {
        uint64_t totalDecisions;
        uint64_t scalingEvents;
        uint64_t migrationEvents;
        uint64_t emergencyResponses;
        double avgDecisionLatency;
        double resourceUtilization;
        std::chrono::steady_clock::time_point startTime;
    } performanceMetrics_;
    
    // Message queues for co-simulation
    std::queue<CoSimMessage> outgoingMessages_;
    std::queue<CoSimMessage> incomingMessages_;
    std::mutex messageQueueMutex_;
    std::condition_variable messageCondition_;
    
    // Synchronization for leader-follower
    std::mutex syncMutex_;
    std::condition_variable syncCondition_;
    std::atomic<bool> syncAckReceived_;
    std::atomic<bool> metricsReceived_;
};

// Utility functions for VNF management
std::string vnfTypeToString(VNFType type);
VNFType stringToVNFType(const std::string& str);
std::string nfvDecisionToJson(const NFVDecision& decision);
NFVDecision jsonToNFVDecision(const std::string& json);

} // namespace cosim

#endif // OMNET_ORCHESTRATOR_H
