/*
Custom NS-3 Co-simulation Adapter Implementation
Independent implementation inspired by ns3-cosim for V2X-NDN-NFV platform
*/

#include "ns3_adapter.h"
#include <iostream>
#include <sstream>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <cstring>
#include <jsoncpp/json/json.h>

#include <chrono>

namespace cosim {

// =============================================================================
// ExternalSyncManager Implementation
// =============================================================================

ExternalSyncManager::ExternalSyncManager() 
    : initialized_(false), syncPending_(false), running_(false),
      currentTime_(0.0), targetTime_(0.0), syncInterval_(1.0), timeoutSeconds_(10.0),
      serverSocket_(-1), clientSocket_(-1) {
}

ExternalSyncManager::~ExternalSyncManager() {
    Shutdown();
}

bool ExternalSyncManager::Initialize(int port) {
    std::cout << "Initializing ExternalSyncManager on port " << port << std::endl;
    
    // Create server socket
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0) {
        std::cerr << "Failed to create server socket" << std::endl;
        return false;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Failed to set socket options" << std::endl;
        close(serverSocket_);
        return false;
    }
    
    // Bind socket
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);
    
    if (bind(serverSocket_, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Failed to bind socket to port " << port << std::endl;
        close(serverSocket_);
        return false;
    }
    
    // Listen for connections
    if (listen(serverSocket_, 1) < 0) {
        std::cerr << "Failed to listen on socket" << std::endl;
        close(serverSocket_);
        return false;
    }
    
    running_ = true;
    communicationThread_ = std::thread(&ExternalSyncManager::CommunicationLoop, this);
    initialized_ = true;
    
    std::cout << "ExternalSyncManager initialized successfully" << std::endl;
    return true;
}

void ExternalSyncManager::Shutdown() {
    if (!initialized_) return;
    
    running_ = false;
    
    if (communicationThread_.joinable()) {
        communicationThread_.join();
    }
    
    if (clientSocket_ >= 0) {
        close(clientSocket_);
        clientSocket_ = -1;
    }
    
    if (serverSocket_ >= 0) {
        close(serverSocket_);
        serverSocket_ = -1;
    }
    
    initialized_ = false;
    std::cout << "ExternalSyncManager shutdown complete" << std::endl;
}

bool ExternalSyncManager::SyncToTime(double targetTime) {
    if (!initialized_) {
        std::cerr << "ExternalSyncManager not initialized" << std::endl;
        return false;
    }
    
    std::unique_lock<std::mutex> lock(syncMutex_);
    targetTime_ = targetTime;
    syncPending_ = true;
    
    if (!SendSyncCommand(targetTime)) {
        return false;
    }
    
    // Wait for sync completion or timeout
    auto timeout = std::chrono::milliseconds(static_cast<long>(timeoutSeconds_ * 1000));
    bool success = syncCondition_.wait_for(lock, timeout, [this] {
        return !syncPending_.load() || !running_.load();
    });
    
    if (success && !syncPending_) {
        currentTime_ = targetTime;
        return true;
    }
    
    std::cerr << "Sync timeout or failure" << std::endl;
    return false;
}

void ExternalSyncManager::CommunicationLoop() {
    std::cout << "Starting communication loop..." << std::endl;
    
    while (running_) {
        if (clientSocket_ < 0) {
            // Wait for client connection
            struct sockaddr_in clientAddr;
            socklen_t clientLen = sizeof(clientAddr);
            
            clientSocket_ = accept(serverSocket_, (struct sockaddr*)&clientAddr, &clientLen);
            if (clientSocket_ < 0) {
                if (running_) {
                    std::cerr << "Failed to accept client connection" << std::endl;
                }
                continue;
            }
            
            std::cout << "NS-3 client connected" << std::endl;
        }
        
        // Handle incoming messages
        char buffer[1024];
        ssize_t bytesRead = recv(clientSocket_, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);
        
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            std::string message(buffer);
            HandleIncomingMessage(message);
        } else if (bytesRead == 0) {
            // Client disconnected
            std::cout << "NS-3 client disconnected" << std::endl;
            close(clientSocket_);
            clientSocket_ = -1;
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

bool ExternalSyncManager::SendSyncCommand(double time) {
    if (clientSocket_ < 0) {
        std::cerr << "No client connection for sync command" << std::endl;
        return false;
    }
    
    std::ostringstream oss;
    oss << "SYNC " << time << "\n";
    std::string command = oss.str();
    
    ssize_t sent = send(clientSocket_, command.c_str(), command.length(), 0);
    return sent == static_cast<ssize_t>(command.length());
}

void ExternalSyncManager::HandleIncomingMessage(const std::string& message) {
    if (message.find("SYNC_COMPLETE") != std::string::npos) {
        std::lock_guard<std::mutex> lock(syncMutex_);
        syncPending_ = false;
        syncCondition_.notify_one();
        
        if (syncCallback_) {
            syncCallback_(targetTime_);
        }
    } else if (messageCallback_) {
        messageCallback_(message);
    }
}

// =============================================================================
// SocketClient Implementation  
// =============================================================================

SocketClient::SocketClient() 
    : serverAddress_("127.0.0.1"), serverPort_(9999), socket_(-1),
      connected_(false), receiving_(false) {
}

SocketClient::~SocketClient() {
    Shutdown();
}

bool SocketClient::Initialize() {
    return SetupSocket();
}

void SocketClient::Shutdown() {
    StopAsyncReceive();
    Disconnect();
}

bool SocketClient::Connect() {
    if (connected_) return true;
    
    socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_ < 0) {
        std::cerr << "Failed to create client socket" << std::endl;
        return false;
    }
    
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverPort_);
    
    if (inet_pton(AF_INET, serverAddress_.c_str(), &serverAddr.sin_addr) <= 0) {
        std::cerr << "Invalid server address: " << serverAddress_ << std::endl;
        close(socket_);
        return false;
    }
    
    if (connect(socket_, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Failed to connect to " << serverAddress_ << ":" << serverPort_ << std::endl;
        close(socket_);
        return false;
    }
    
    connected_ = true;
    std::cout << "Connected to server " << serverAddress_ << ":" << serverPort_ << std::endl;
    return true;
}

void SocketClient::Disconnect() {
    if (socket_ >= 0) {
        close(socket_);
        socket_ = -1;
    }
    connected_ = false;
}

bool SocketClient::SendMessage(const std::string& message) {
    if (!connected_) {
        std::cerr << "Not connected to server" << std::endl;
        return false;
    }
    
    ssize_t sent = send(socket_, message.c_str(), message.length(), 0);
    return sent == static_cast<ssize_t>(message.length());
}

std::string SocketClient::ReceiveMessage() {
    if (!connected_) return "";
    
    char buffer[1024];
    ssize_t bytesRead = recv(socket_, buffer, sizeof(buffer) - 1, 0);
    
    if (bytesRead > 0) {
        buffer[bytesRead] = '\0';
        return std::string(buffer);
    }
    
    return "";
}

void SocketClient::StartAsyncReceive() {
    if (receiving_) return;
    
    receiving_ = true;
    receiveThread_ = std::thread(&SocketClient::ReceiveLoop, this);
}

void SocketClient::StopAsyncReceive() {
    receiving_ = false;
    if (receiveThread_.joinable()) {
        receiveThread_.join();
    }
}

void SocketClient::ReceiveLoop() {
    while (receiving_ && connected_) {
        std::string message = ReceiveMessage();
        if (!message.empty()) {
            ProcessMessage(message);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void SocketClient::ProcessMessage(const std::string& message) {
    if (messageCallback_) {
        messageCallback_(message);
    }
    
    // Store as response for synchronous operations
    {
        std::lock_guard<std::mutex> lock(responseMutex_);
        responseQueue_.push(message);
        responseCondition_.notify_one();
    }
}

bool SocketClient::SetupSocket() {
    // Basic setup, actual connection happens in Connect()
    return true;
}

// =============================================================================
// MessageHandler Implementation
// =============================================================================

MessageHandler::MessageHandler() {
}

MessageHandler::~MessageHandler() {
}

void MessageHandler::ProcessMessage(const std::string& message) {
    if (IsSyncMessage(message) && syncHandler_) {
        syncHandler_(message);
    } else if (IsNDNMessage(message) && ndnHandler_) {
        ndnHandler_(message);
    } else if (IsVehicleMessage(message) && vehicleHandler_) {
        vehicleHandler_(message);
    }
}

std::string MessageHandler::CreateSyncMessage(double time) {
    std::ostringstream oss;
    oss << "SYNC " << time;
    return oss.str();
}

std::string MessageHandler::CreateVehicleMessage(const VehicleInfo& vehicle) {
    std::ostringstream oss;
    oss << "VEHICLE " << vehicle.id << " " << vehicle.x << " " << vehicle.y << " " << vehicle.speed;
    return oss.str();
}

std::string MessageHandler::CreateNDNMessage(const std::string& type, const std::string& data) {
    std::ostringstream oss;
    oss << "NDN " << type << " " << data;
    return oss.str();
}

bool MessageHandler::IsSyncMessage(const std::string& message) {
    return message.find("SYNC") == 0;
}

bool MessageHandler::IsVehicleMessage(const std::string& message) {
    return message.find("VEHICLE") == 0;
}

bool MessageHandler::IsNDNMessage(const std::string& message) {
    return message.find("NDN") == 0;
}

// =============================================================================
// NS3Adapter Implementation
// =============================================================================

NS3Adapter::NS3Adapter(const std::string& configFile)
    : ns3ConfigFile_(configFile), communicationPort_("9999"), logLevel_("INFO"),
      ndnTracingEnabled_(false), vehicleTrackingEnabled_(true),
      currentTime_(0.0), running_(false), initialized_(false), ns3Ready_(false),
      ns3ProcessId_(-1) {
    
    // Default NS-3 script path
    ns3ScriptPath_ = "./ns3-scripts/cosim-script.cc";
    
    // Initialize custom components
    syncManager_ = std::make_unique<ExternalSyncManager>();
    socketClient_ = std::make_unique<SocketClient>();
    messageHandler_ = std::make_unique<MessageHandler>();
    
    // Reset statistics
    stats_.reset();
}

NS3Adapter::~NS3Adapter() {
    shutdown();
}

bool NS3Adapter::initialize() {
    std::cout << "Initializing Custom NS-3 Adapter..." << std::endl;
    
    // Setup message handlers
    messageHandler_->RegisterSyncHandler([this](const std::string& msg) {
        handleSyncMessage(msg);
    });
    
    messageHandler_->RegisterNDNHandler([this](const std::string& msg) {
        handleNDNMessage(msg);
    });
    
    messageHandler_->RegisterVehicleHandler([this](const std::string& msg) {
        handleVehicleMessage(msg);
    });
    
    // Initialize sync manager
    int port = std::stoi(communicationPort_);
    if (!syncManager_->Initialize(port)) {
        std::cerr << "Failed to initialize sync manager" << std::endl;
        return false;
    }
    
    // Set sync manager callbacks
    syncManager_->SetSyncEventCallback([this](double time) {
        std::cout << "Sync event at time: " << time << std::endl;
        updateStats("sync");
    });
    
    syncManager_->SetMessageCallback([this](const std::string& msg) {
        messageHandler_->ProcessMessage(msg);
        updateStats("message_received");
    });
    
    // Start NS-3 process
    if (!startNS3Process()) {
        std::cerr << "Failed to start NS-3 process" << std::endl;
        return false;
    }
    
    // Wait for NS-3 to be ready
    std::cout << "Waiting for NS-3 to connect..." << std::endl;
    auto start = std::chrono::steady_clock::now();
    while (!ns3Ready_ && 
           std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::steady_clock::now() - start).count() < 30) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        // Check if NS-3 process is still running
        if (!isNS3ProcessRunning()) {
            std::cerr << "NS-3 process terminated unexpectedly" << std::endl;
            return false;
        }
    }
    
    if (!ns3Ready_) {
        std::cerr << "Timeout waiting for NS-3 to connect" << std::endl;
        return false;
    }
    
    initialized_ = true;
    running_ = true;
    std::cout << "NS-3 Adapter initialized successfully" << std::endl;
    
    return true;
}

bool NS3Adapter::step(double timeStep) {
    if (!running_ || !initialized_) {
        return false;
    }
    
    double targetTime = currentTime_ + timeStep;
    
    // Synchronize to target time
    if (!syncManager_->SyncToTime(targetTime)) {
        std::cerr << "Failed to sync to time: " << targetTime << std::endl;
        return false;
    }
    
    currentTime_ = targetTime;
    updateStats("step");
    
    return true;
}

void NS3Adapter::shutdown() {
    if (!initialized_) return;
    
    std::cout << "Shutting down NS-3 Adapter..." << std::endl;
    
    running_ = false;
    
    // Stop NS-3 process
    stopNS3Process();
    
    // Shutdown components
    if (syncManager_) {
        syncManager_->Shutdown();
    }
    
    if (socketClient_) {
        socketClient_->Shutdown();
    }
    
    // Print final statistics
    printStats();
    
    initialized_ = false;
    std::cout << "NS-3 Adapter shutdown complete" << std::endl;
}

std::vector<VehicleInfo> NS3Adapter::getVehicleData() {
    std::lock_guard<std::mutex> lock(vehiclesMutex_);
    return vehicles_;
}

void NS3Adapter::updateVehicleData(const std::vector<VehicleInfo>& vehicles) {
    std::lock_guard<std::mutex> lock(vehiclesMutex_);
    vehicles_ = vehicles;
    
    // Send vehicle updates to NS-3 if connected
    if (vehicleTrackingEnabled_ && syncManager_->IsInitialized()) {
        for (const auto& vehicle : vehicles) {
            std::string message = messageHandler_->CreateVehicleMessage(vehicle);
            // Send through sync manager (would need additional method)
            updateStats("vehicle_update");
        }
    }
}

void NS3Adapter::setSyncInterval(double interval) {
    if (syncManager_) {
        syncManager_->SetSyncInterval(interval);
    }
}

void NS3Adapter::setTimeoutDuration(double timeout) {
    if (syncManager_) {
        syncManager_->SetTimeoutDuration(timeout);
    }
}

bool NS3Adapter::startNS3Process() {
    std::cout << "Starting NS-3 process..." << std::endl;
    
    // Build command to run NS-3 script
    std::vector<std::string> command;
    command.push_back("cd");
    command.push_back("/home/rajesh/ndnSIM/ns-3");
    command.push_back("&&");
    command.push_back("./waf");
    command.push_back("--run");
    
    // Use the cosim script with appropriate parameters
    std::ostringstream scriptArgs;
    scriptArgs << "\"cosim-script --port=" << communicationPort_;
    if (!ns3ConfigFile_.empty()) {
        scriptArgs << " --config=" << ns3ConfigFile_;
    }
    scriptArgs << "\"";
    command.push_back(scriptArgs.str());
    
    // Join command
    std::ostringstream fullCommand;
    for (size_t i = 0; i < command.size(); ++i) {
        if (i > 0) fullCommand << " ";
        fullCommand << command[i];
    }
    
    std::cout << "Executing: " << fullCommand.str() << std::endl;
    
    // Fork and exec
    ns3ProcessId_ = fork();
    if (ns3ProcessId_ == 0) {
        // Child process
        execl("/bin/bash", "bash", "-c", fullCommand.str().c_str(), nullptr);
        std::cerr << "Failed to exec NS-3 command" << std::endl;
        exit(1);
    } else if (ns3ProcessId_ > 0) {
        // Parent process
        std::cout << "NS-3 process started with PID: " << ns3ProcessId_ << std::endl;
        
        // Give NS-3 time to start
        std::this_thread::sleep_for(std::chrono::seconds(2));
        
        // Mark as ready (in real implementation, this would be set by connection callback)
        ns3Ready_ = true;
        
        return true;
    } else {
        std::cerr << "Failed to fork NS-3 process" << std::endl;
        return false;
    }
}

void NS3Adapter::stopNS3Process() {
    if (ns3ProcessId_ > 0) {
        std::cout << "Stopping NS-3 process (PID: " << ns3ProcessId_ << ")..." << std::endl;
        
        // Send SIGTERM first
        kill(ns3ProcessId_, SIGTERM);
        
        // Wait for graceful shutdown
        int status;
        pid_t result = waitpid(ns3ProcessId_, &status, WNOHANG);
        
        if (result == 0) {
            // Process still running, wait a bit more
            std::this_thread::sleep_for(std::chrono::seconds(2));
            result = waitpid(ns3ProcessId_, &status, WNOHANG);
            
            if (result == 0) {
                // Force kill
                std::cout << "Force killing NS-3 process..." << std::endl;
                kill(ns3ProcessId_, SIGKILL);
                waitpid(ns3ProcessId_, &status, 0);
            }
        }
        
        ns3ProcessId_ = -1;
        ns3Ready_ = false;
        std::cout << "NS-3 process stopped" << std::endl;
    }
}

bool NS3Adapter::isNS3ProcessRunning() {
    if (ns3ProcessId_ <= 0) return false;
    
    int status;
    pid_t result = waitpid(ns3ProcessId_, &status, WNOHANG);
    return result == 0; // Process still running
}

void NS3Adapter::handleSyncMessage(const std::string& message) {
    std::cout << "Handling sync message: " << message << std::endl;
    updateStats("sync_message");
}

void NS3Adapter::handleNDNMessage(const std::string& message) {
    std::cout << "Handling NDN message: " << message << std::endl;
    
    if (message.find("INTEREST") != std::string::npos) {
        updateStats("ndn_interest");
    } else if (message.find("DATA") != std::string::npos) {
        updateStats("ndn_data");
    }
}

void NS3Adapter::handleVehicleMessage(const std::string& message) {
    std::cout << "Handling vehicle message: " << message << std::endl;
    
    // Parse vehicle data and update internal storage
    VehicleInfo vehicle = parseVehicleData(message);
    if (!vehicle.id.empty()) {
        std::lock_guard<std::mutex> lock(vehiclesMutex_);
        
        // Update or add vehicle
        bool found = false;
        for (auto& v : vehicles_) {
            if (v.id == vehicle.id) {
                v = vehicle;
                found = true;
                break;
            }
        }
        
        if (!found) {
            vehicles_.push_back(vehicle);
        }
        
        updateStats("vehicle_message");
    }
}

VehicleInfo NS3Adapter::parseVehicleData(const std::string& data) {
    VehicleInfo vehicle;
    vehicle.id = ""; // Invalid by default
    
    std::istringstream iss(data);
    std::string prefix;
    
    if (iss >> prefix >> vehicle.id >> vehicle.x >> vehicle.y >> vehicle.speed) {
        vehicle.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        return vehicle;
    }
    
    vehicle.id = ""; // Reset to invalid
    return vehicle; // Invalid
}

std::string NS3Adapter::formatVehicleUpdate(const VehicleInfo& vehicle) {
    return messageHandler_->CreateVehicleMessage(vehicle);
}

void NS3Adapter::processNDNInterest(const std::string& message) {
    std::cout << "Processing NDN Interest: " << message << std::endl;
    updateStats("ndn_interest");
}

void NS3Adapter::processNDNData(const std::string& message) {
    std::cout << "Processing NDN Data: " << message << std::endl;
    updateStats("ndn_data");
}

void NS3Adapter::updateStats(const std::string& operation) {
    if (operation == "message_sent") {
        stats_.messagesSent++;
    } else if (operation == "message_received") {
        stats_.messagesReceived++;
    } else if (operation == "sync" || operation == "sync_message") {
        stats_.syncOperations++;
    } else if (operation == "timeout") {
        stats_.timeouts++;
    } else if (operation == "ndn_interest") {
        stats_.ndnInterests++;
    } else if (operation == "ndn_data") {
        stats_.ndnData++;
    }
}

void NS3Adapter::printStats() const {
    stats_.print();
}

NDNMetrics NS3Adapter::collectNDNMetrics() const {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    
    NDNMetrics metrics;
    metrics.timestamp = getCurrentTime();
    
    // Map internal statistics to NDNMetrics structure (match message.h fields)
    metrics.pitSize = ndnStats_.pendingInterests;
    metrics.cacheHitRatio = (ndnStats_.interests > 0) 
                           ? static_cast<double>(ndnStats_.cacheHits) / ndnStats_.interests : 0.0;
    metrics.avgLatency = (ndnStats_.totalLatency > 0 && ndnStats_.satisfiedInterests > 0) 
                        ? ndnStats_.totalLatency / ndnStats_.satisfiedInterests : 0.0;
    metrics.interestCount = ndnStats_.interests;
    metrics.dataCount = ndnStats_.dataPackets;
    metrics.unsatisfiedInterests = ndnStats_.timeouts;
    
    return metrics;
}

void NS3Adapter::sendMetricsToLeader() {
    if (leaderSocket_ < 0) return;
    
    NDNMetrics metrics = collectNDNMetrics();
    
    // Create JSON message
    Json::Value json;
    json["type"] = "NDN_METRICS";
    json["timestamp"] = metrics.timestamp;
    json["pit_size"] = static_cast<Json::UInt>(metrics.pitSize);
    json["avg_latency"] = metrics.avgLatency;
    json["unsatisfied_interests"] = static_cast<Json::UInt>(metrics.unsatisfiedInterests);
    json["interest_count"] = static_cast<Json::UInt64>(metrics.interestCount);
    json["data_count"] = static_cast<Json::UInt64>(metrics.dataCount);
    json["cache_hit_ratio"] = metrics.cacheHitRatio;
    json["fib_entries"] = static_cast<Json::UInt>(metrics.fibEntries);
    json["emergency_messages"] = static_cast<Json::UInt>(metrics.emergencyMessages);
    json["safety_messages"] = static_cast<Json::UInt>(metrics.safetyMessages);
    json["network_utilization"] = metrics.networkUtilization;
    
    Json::StreamWriterBuilder builder;
    std::string message = Json::writeString(builder, json) + "\n";
    
    send(leaderSocket_, message.c_str(), message.length(), MSG_NOSIGNAL);
}

void NS3Adapter::SimulationStats::print() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - startTime);
    
    std::cout << "\n=== NS-3 Adapter Statistics ===" << std::endl;
    std::cout << "Runtime: " << duration.count() << " seconds" << std::endl;
    std::cout << "Messages sent: " << messagesSent << std::endl;
    std::cout << "Messages received: " << messagesReceived << std::endl;
    std::cout << "Sync operations: " << syncOperations << std::endl;
    std::cout << "Timeouts: " << timeouts << std::endl;
    std::cout << "NDN Interests: " << ndnInterests << std::endl;
    std::cout << "NDN Data: " << ndnData << std::endl;
    std::cout << "=============================" << std::endl;
}

// Method to update NDN statistics from callbacks
void NS3Adapter::updateNDNStats(const std::string& event, double latency) {
    std::lock_guard<std::mutex> lock(metricsMutex_);
    
    if (event == "INTEREST_SENT") {
        ndnStats_.interests++;
        ndnStats_.pendingInterests++;
    } else if (event == "DATA_RECEIVED") {
        ndnStats_.dataPackets++;
        ndnStats_.satisfiedInterests++;
        ndnStats_.totalLatency += latency;
        if (ndnStats_.pendingInterests > 0) {
            ndnStats_.pendingInterests--;
        }
    } else if (event == "TIMEOUT") {
        ndnStats_.timeouts++;
        if (ndnStats_.pendingInterests > 0) {
            ndnStats_.pendingInterests--;
        }
    } else if (event == "CACHE_HIT") {
        ndnStats_.cacheHits++;
    }
}

} // namespace cosim