/*
Custom NS-3 Co-simulation Adapter
Inspired by ns3-cosim but built independently for V2X-NDN-NFV platform
*/

#ifndef NS3_ADAPTER_H
#define NS3_ADAPTER_H

#include "synchronizer.h"
#include "message.h"
#include <string>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <memory>
#include <chrono>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

namespace cosim {

// Forward declarations
class ExternalSyncManager;
class SocketClient;
class MessageHandler;

// Custom Sync Manager (inspired by ns3-cosim ExternalSyncManager)
class ExternalSyncManager {
public:
    ExternalSyncManager();
    ~ExternalSyncManager();
    
    // Core sync operations
    bool Initialize(int port);
    void Shutdown();
    
    bool SyncToTime(double targetTime);
    bool WaitForExternalCommand();
    void NotifySyncComplete();
    
    // Configuration
    void SetSyncInterval(double interval) { syncInterval_ = interval; }
    double GetSyncInterval() const { return syncInterval_; }
    void SetTimeoutDuration(double timeout) { timeoutSeconds_ = timeout; }
    
    // Event callbacks
    void SetSyncEventCallback(std::function<void(double)> callback) { syncCallback_ = callback; }
    void SetMessageCallback(std::function<void(const std::string&)> callback) { messageCallback_ = callback; }
    
    // Status
    bool IsInitialized() const { return initialized_.load(); }
    bool IsSyncPending() const { return syncPending_.load(); }
    double GetCurrentTime() const { return currentTime_.load(); }
    
private:
    std::atomic<bool> initialized_;
    std::atomic<bool> syncPending_;
    std::atomic<bool> running_;
    std::atomic<double> currentTime_;
    std::atomic<double> targetTime_;
    
    double syncInterval_;
    double timeoutSeconds_;
    
    std::mutex syncMutex_;
    std::condition_variable syncCondition_;
    
    int serverSocket_;
    int clientSocket_;
    std::thread communicationThread_;
    
    std::function<void(double)> syncCallback_;
    std::function<void(const std::string&)> messageCallback_;
    
    void CommunicationLoop();
    bool SendSyncCommand(double time);
    bool ReceiveSyncResponse();
    void HandleIncomingMessage(const std::string& message);
};

// Custom Socket Client (inspired by ns3-cosim SocketClient)
class SocketClient {
public:
    SocketClient();
    ~SocketClient();
    
    // Connection management
    bool Initialize();
    void Shutdown();
    
    void SetServerAddress(const std::string& address) { serverAddress_ = address; }
    void SetServerPort(int port) { serverPort_ = port; }
    
    bool Connect();
    void Disconnect();
    bool IsConnected() const { return connected_.load(); }
    
    // Message operations
    bool SendMessage(const std::string& message);
    std::string ReceiveMessage();
    bool SendCommand(const std::string& command);
    std::string WaitForResponse(double timeoutSeconds = 5.0);
    
    // Async message handling
    void SetMessageCallback(std::function<void(const std::string&)> callback) { 
        messageCallback_ = callback; 
    }
    void StartAsyncReceive();
    void StopAsyncReceive();
    
private:
    std::string serverAddress_;
    int serverPort_;
    int socket_;
    
    std::atomic<bool> connected_;
    std::atomic<bool> receiving_;
    
    std::thread receiveThread_;
    std::function<void(const std::string&)> messageCallback_;
    
    std::queue<std::string> responseQueue_;
    std::mutex responseMutex_;
    std::condition_variable responseCondition_;
    
    void ReceiveLoop();
    bool SetupSocket();
    void ProcessMessage(const std::string& message);
};

// Message Handler for different message types
class MessageHandler {
public:
    MessageHandler();
    ~MessageHandler();
    
    // Message type registration
    void RegisterNDNHandler(std::function<void(const std::string&)> handler) { ndnHandler_ = handler; }
    void RegisterVehicleHandler(std::function<void(const std::string&)> handler) { vehicleHandler_ = handler; }
    void RegisterSyncHandler(std::function<void(const std::string&)> handler) { syncHandler_ = handler; }
    
    // Message processing
    void ProcessMessage(const std::string& message);
    
    // Message creation
    std::string CreateSyncMessage(double time);
    std::string CreateVehicleMessage(const VehicleInfo& vehicle);
    std::string CreateNDNMessage(const std::string& type, const std::string& data);
    
private:
    std::function<void(const std::string&)> ndnHandler_;
    std::function<void(const std::string&)> vehicleHandler_;
    std::function<void(const std::string&)> syncHandler_;
    
    bool IsNDNMessage(const std::string& message);
    bool IsVehicleMessage(const std::string& message);
    bool IsSyncMessage(const std::string& message);
};

// Main NS-3 Adapter using custom components
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
    
    double getCurrentTime() const override { return currentTime_.load(); }
    bool isRunning() const override { return running_.load(); }
    SimulatorType getType() const override { return SimulatorType::NS3; }
    
    // Configuration methods
    void setNS3ScriptPath(const std::string& scriptPath) { ns3ScriptPath_ = scriptPath; }
    void setNS3ConfigFile(const std::string& configFile) { ns3ConfigFile_ = configFile; }
    void setSyncInterval(double interval);
    void setTimeoutDuration(double timeout);
    void setCommunicationPort(const std::string& port) { communicationPort_ = port; }
    
    // Advanced features
    void enableNDNTracing(bool enable) { ndnTracingEnabled_ = enable; }
    void enableVehicleTracking(bool enable) { vehicleTrackingEnabled_ = enable; }
    void setLogLevel(const std::string& level) { logLevel_ = level; }
    
    // Follower-specific methods (ndnSIM as follower)
    bool connectToLeader(const std::string& address = "127.0.0.1", int port = 9999);
    bool sendMetricsToLeader(const NDNMetrics& metrics);
    void sendMetricsToLeader(); // Overloaded method without parameters
    bool sendAckToLeader();
    void handleLeaderCommand(const std::string& command);
    
    // Method to update NDN statistics from callbacks
    void updateNDNStats(const std::string& event, double latency = 0.0);
    
    // NS-3 example configuration
    void setNS3Example(const std::string& example) { ns3Example_ = example; }
    void setKathmanduScenario(bool enable) { useKathmanduScenario_ = enable; }
    
    // NDN Metrics collection
    NDNMetrics collectNDNMetrics() const;
    void enableMetricsCollection(bool enable) { metricsEnabled_ = enable; }
    
private:
    // Custom co-simulation components (inspired by ns3-cosim)
    std::unique_ptr<ExternalSyncManager> syncManager_;
    std::unique_ptr<SocketClient> socketClient_;
    std::unique_ptr<MessageHandler> messageHandler_;
    
    // NS-3 process management
    bool startNS3Process();
    void stopNS3Process();
    bool isNS3ProcessRunning();
    
    // Message handling callbacks
    void handleSyncMessage(const std::string& message);
    void handleNDNMessage(const std::string& message);
    void handleVehicleMessage(const std::string& message);
    
    // Data conversion and processing
    VehicleInfo parseVehicleData(const std::string& data);
    std::string formatVehicleUpdate(const VehicleInfo& vehicle);
    void processNDNInterest(const std::string& message);
    void processNDNData(const std::string& message);
    
    // Configuration
    std::string ns3ScriptPath_;
    std::string ns3ConfigFile_;
    std::string communicationPort_;
    std::string logLevel_;
    
    // Feature flags
    bool ndnTracingEnabled_;
    bool vehicleTrackingEnabled_;
    
    // State management
    std::atomic<double> currentTime_;
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;
    std::atomic<bool> ns3Ready_;
    
    // Vehicle data storage
    std::vector<VehicleInfo> vehicles_;
    std::mutex vehiclesMutex_;
    
    // Process management
    pid_t ns3ProcessId_;
    
    // Follower configuration
    std::string leaderAddress_;
    int leaderPort_;
    bool isFollower_;
    bool useKathmanduScenario_;
    std::string ns3Example_;
    bool metricsEnabled_;
    
    // NDN metrics collection
    mutable std::mutex metricsMutex_;
    NDNMetrics lastMetrics_;
    
    // NDN Statistics structure for internal tracking
    struct NDNStatistics {
        uint64_t pendingInterests = 0;
        uint64_t cacheHits = 0;
        uint64_t timeouts = 0;
        uint64_t interests = 0;
        uint64_t dataPackets = 0;
        uint64_t satisfiedInterests = 0;
        double totalLatency = 0.0;
    };
    
    NDNStatistics ndnStats_;
    int leaderSocket_ = -1;
    
    // Statistics and monitoring
    struct SimulationStats {
        uint64_t messagesSent;
        uint64_t messagesReceived;
        uint64_t syncOperations;
        uint64_t timeouts;
        uint64_t ndnInterests;
        uint64_t ndnData;
        std::chrono::steady_clock::time_point startTime;
        
        void reset() {
            messagesSent = messagesReceived = syncOperations = 0;
            timeouts = ndnInterests = ndnData = 0;
            startTime = std::chrono::steady_clock::now();
        }
        
        void print() const;
    } stats_;
    
    void updateStats(const std::string& operation);
    void printStats() const;
};

} // namespace cosim

#endif // NS3_ADAPTER_H