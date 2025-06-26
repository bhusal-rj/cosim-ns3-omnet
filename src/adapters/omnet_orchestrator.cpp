/*
OMNeT++ NFV Orchestrator Implementation
Leader in V2X-NDN-NFV Co-simulation
*/

#include "omnet_orchestrator.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <random>
#include <fstream>
#include <iomanip>
#include <arpa/inet.h>
#include <jsoncpp/json/json.h>

namespace cosim {

OMNeTOrchestrator::OMNeTOrchestrator() 
    : currentTime_(0.0), running_(false), initialized_(false), leaderReady_(false),
      followerConnected_(false), serverSocket_(-1), followerSocket_(-1),
      trafficDensity_("normal"), scenarioType_("generic"), useKathmanduScenario_(false),
      serverPort_(9999), syncAckReceived_(false), metricsReceived_(false) {
    
    // Initialize VNF instances as per methodology
    vnfInstances_[VNFType::NDN_ROUTER] = {};
    vnfInstances_[VNFType::TRAFFIC_ANALYZER] = {};
    vnfInstances_[VNFType::SECURITY_VNF] = {};
    vnfInstances_[VNFType::CACHE_OPTIMIZER] = {};
    
    // Initialize performance metrics
    performanceMetrics_.totalDecisions = 0;
    performanceMetrics_.scalingEvents = 0;
    performanceMetrics_.migrationEvents = 0;
    performanceMetrics_.emergencyResponses = 0;
    performanceMetrics_.avgDecisionLatency = 0.0;
    performanceMetrics_.resourceUtilization = 0.0;
    performanceMetrics_.startTime = std::chrono::steady_clock::now();
    
    // Initialize Kathmandu intersection
    kathmanduIntersection_.x = 0.0;
    kathmanduIntersection_.y = 0.0;
    kathmanduIntersection_.approaches = {"North", "South", "East", "West"};
    kathmanduIntersection_.currentPhase = 0;
    kathmanduIntersection_.phaseTimer = 0.0;
}

OMNeTOrchestrator::~OMNeTOrchestrator() {
    shutdown();
}

bool OMNeTOrchestrator::initialize() {
    std::cout << "🔧 Initializing OMNeT++ NFV Orchestrator (Leader)..." << std::endl;
    
    // Setup as leader server
    if (!startAsLeader(serverPort_)) {
        std::cerr << "❌ Failed to start as leader on port " << serverPort_ << std::endl;
        return false;
    }
    
    // Initialize scenario-specific setup
    if (useKathmanduScenario_) {
        initializeKathmanduTopology();
        generateKathmanduTraffic();
    } else {
        // Generate generic vehicle traffic
        vehicles_.clear();
        int vehicleCount = (trafficDensity_ == "light") ? 5 : 
                          (trafficDensity_ == "heavy") ? 25 : 15;
        
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> posDist(-500.0, 500.0);
        std::uniform_real_distribution<> speedDist(10.0, 30.0);
        
        for (int i = 0; i < vehicleCount; ++i) {
            VehicleInfo vehicle;
            vehicle.id = "vehicle_" + std::to_string(i);
            vehicle.x = posDist(gen);
            vehicle.y = posDist(gen);
            vehicle.z = 0.0;
            vehicle.speed = speedDist(gen);
            vehicle.heading = std::uniform_real_distribution<>(0.0, 360.0)(gen);
            vehicle.timestamp = currentTime_;
            
            vehicles_.push_back(vehicle);
        }
    }
    
    // Deploy initial VNF instances
    deployVNF(VNFType::NDN_ROUTER, "RSU_1");
    deployVNF(VNFType::TRAFFIC_ANALYZER, "EDGE_1");
    deployVNF(VNFType::SECURITY_VNF, "RSU_1");
    deployVNF(VNFType::CACHE_OPTIMIZER, "EDGE_1");
    
    running_ = true;
    initialized_ = true;
    
    std::cout << "✅ OMNeT++ Orchestrator initialized with " << vehicles_.size() << " vehicles" << std::endl;
    std::cout << "📊 Initial VNF deployment completed" << std::endl;
    
    return true;
}

bool OMNeTOrchestrator::startAsLeader(int port) {
    serverPort_ = port;
    
    // Create server socket
    serverSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket_ < 0) {
        std::cerr << "❌ Failed to create leader server socket" << std::endl;
        return false;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(serverSocket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "❌ Failed to set socket options" << std::endl;
        close(serverSocket_);
        return false;
    }
    
    // Bind socket
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);
    
    if (bind(serverSocket_, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "❌ Failed to bind leader socket to port " << port << std::endl;
        close(serverSocket_);
        return false;
    }
    
    // Listen for follower connections
    if (listen(serverSocket_, 1) < 0) {
        std::cerr << "❌ Failed to listen on leader socket" << std::endl;
        close(serverSocket_);
        return false;
    }
    
    // Start leader thread
    leaderThread_ = std::thread(&OMNeTOrchestrator::leaderLoop, this);
    leaderReady_ = true;
    
    std::cout << "🌐 Leader server started on port " << port << std::endl;
    return true;
}

void OMNeTOrchestrator::leaderLoop() {
    std::cout << "🔄 Leader communication loop started..." << std::endl;
    
    while (running_) {
        if (followerSocket_ < 0) {
            // Wait for follower connection
            std::cout << "⏳ Waiting for ndnSIM follower to connect..." << std::endl;
            
            struct sockaddr_in followerAddr;
            socklen_t followerLen = sizeof(followerAddr);
            
            followerSocket_ = accept(serverSocket_, (struct sockaddr*)&followerAddr, &followerLen);
            if (followerSocket_ < 0) {
                if (running_) {
                    std::cerr << "❌ Failed to accept follower connection" << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                }
                continue;
            }
            
            followerConnected_ = true;
            std::cout << "✅ ndnSIM follower connected from " 
                      << inet_ntoa(followerAddr.sin_addr) << std::endl;
        }
        
        // Handle incoming messages from follower
        try {
            CoSimMessage message = receiveMessage(followerSocket_);
            
            switch (message.type) {
                case CoSimMessage::NDN_METRICS: {
                    // Parse NDN metrics from follower
                    NDNMetrics metrics = parseNDNMetrics(message.payload);
                    handleFollowerMetrics(metrics);
                    metricsReceived_ = true;
                    break;
                }
                
                case CoSimMessage::TIME_SYNC: {
                    // Follower acknowledging time sync
                    syncAckReceived_ = true;
                    syncCondition_.notify_one();
                    break;
                }
                
                default:
                    std::cout << "📨 Received message type: " << message.type << std::endl;
                    break;
            }
            
        } catch (const std::exception& e) {
            if (running_) {
                std::cerr << "⚠️ Communication error with follower: " << e.what() << std::endl;
                close(followerSocket_);
                followerSocket_ = -1;
                followerConnected_ = false;
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    if (followerSocket_ >= 0) {
        close(followerSocket_);
        followerSocket_ = -1;
    }
    
    std::cout << "🔚 Leader communication loop ended" << std::endl;
}

bool OMNeTOrchestrator::step(double timeStep) {
    if (!running_ || !initialized_) {
        return false;
    }
    
    double nextTime = currentTime_ + timeStep;
    
    // As leader, send time synchronization command to follower
    if (!sendTimeSyncCommand(nextTime)) {
        std::cerr << "❌ Failed to send time sync command" << std::endl;
        return false;
    }
    
    // Wait for follower acknowledgment
    if (!waitForFollowerAck()) {
        std::cerr << "❌ Timeout waiting for follower acknowledgment" << std::endl;
        return false;
    }
    
    // Update our simulation state
    if (useKathmanduScenario_) {
        simulateIntersectionBehavior(timeStep);
    } else {
        updateVehiclePositions(timeStep);
    }
    
    // Generate V2X messages and events
    generateV2XMessages();
    handleEmergencyScenarios();
    
    // Update performance metrics
    updatePerformanceMetrics();
    
    currentTime_ = nextTime;
    
    return true;
}

bool OMNeTOrchestrator::sendTimeSyncCommand(double nextTime) {
    if (followerSocket_ < 0) {
        std::cout << "⏳ No follower connected, skipping time sync" << std::endl;
        return true; // Allow operation without follower for testing
    }
    
    CoSimMessage message;
    message.type = CoSimMessage::TIME_SYNC;
    message.timestamp = nextTime;
    message.priority = 1; // High priority
    
    // Create time sync payload
    Json::Value payload;
    payload["command"] = "ADVANCE_TIME";
    payload["target_time"] = nextTime;
    payload["leader_time"] = currentTime_.load();
    
    Json::StreamWriterBuilder builder;
    message.payload = Json::writeString(builder, payload);
    
    syncAckReceived_ = false;
    return sendMessage(followerSocket_, message);
}

bool OMNeTOrchestrator::waitForFollowerAck() {
    if (followerSocket_ < 0) {
        return true; // No follower connected
    }
    
    std::unique_lock<std::mutex> lock(syncMutex_);
    auto timeout = std::chrono::seconds(5); // 5 second timeout
    
    return syncCondition_.wait_for(lock, timeout, [this] {
        return syncAckReceived_.load();
    });
}

void OMNeTOrchestrator::handleFollowerMetrics(const NDNMetrics& metrics) {
    std::lock_guard<std::mutex> lock(nfvStateMutex_);
    
    // Analyze metrics and make NFV decisions
    std::vector<NFVDecision> decisions = analyzeAndDecide(metrics);
    
    // Execute decisions
    if (!decisions.empty()) {
        executeNFVDecisions(decisions);
        
        // Send decisions to follower
        for (const auto& decision : decisions) {
            CoSimMessage message;
            message.type = CoSimMessage::NFV_COMMAND;
            message.timestamp = currentTime_;
            message.priority = decision.priority;
            message.payload = nfvDecisionToJson(decision);
            
            if (followerSocket_ >= 0) {
                sendMessage(followerSocket_, message);
            }
        }
    }
    
    // Log metrics
    std::cout << "📊 NDN Metrics - PIT: " << metrics.pitSize 
              << ", Cache Hit: " << std::fixed << std::setprecision(2) << metrics.cacheHitRatio
              << ", Latency: " << (metrics.avgLatency * 1000) << "ms" << std::endl;
}

std::vector<NFVDecision> OMNeTOrchestrator::analyzeAndDecide(const NDNMetrics& metrics) {
    std::vector<NFVDecision> decisions;
    auto now = std::chrono::steady_clock::now();
    
    // Check if NDN Router VNF needs scaling
    if (shouldScaleUp(metrics, VNFType::NDN_ROUTER)) {
        NFVDecision decision;
        decision.vnfType = VNFType::NDN_ROUTER;
        decision.action = "SCALE_UP";
        decision.targetInstances = calculateRequiredInstances(metrics, VNFType::NDN_ROUTER);
        decision.targetLocation = findOptimalLocation(metrics, VNFType::NDN_ROUTER);
        decision.reason = "PIT size exceeded threshold: " + std::to_string(metrics.pitSize);
        decision.timestamp = currentTime_;
        decision.priority = (metrics.emergencyMessages > 0) ? 1 : 2;
        
        decisions.push_back(decision);
        performanceMetrics_.scalingEvents++;
    }
    
    // Check cache efficiency for Cache Optimizer VNF
    if (metrics.cacheHitRatio < CACHE_EFFICIENCY_THRESHOLD) {
        NFVDecision decision;
        decision.vnfType = VNFType::CACHE_OPTIMIZER;
        decision.action = "OPTIMIZE";
        decision.targetInstances = 1;
        decision.targetLocation = "EDGE_1";
        decision.reason = "Cache hit ratio below threshold: " + std::to_string(metrics.cacheHitRatio);
        decision.timestamp = currentTime_;
        decision.priority = 3;
        
        decisions.push_back(decision);
    }
    
    // Check latency for potential migration
    if (metrics.avgLatency > LATENCY_THRESHOLD) {
        if (shouldMigrate(metrics, VNFType::NDN_ROUTER)) {
            NFVDecision decision;
            decision.vnfType = VNFType::NDN_ROUTER;
            decision.action = "MIGRATE";
            decision.targetInstances = 1;
            decision.sourceLocation = "RSU_1";
            decision.targetLocation = findOptimalLocation(metrics, VNFType::NDN_ROUTER);
            decision.reason = "High latency: " + std::to_string(metrics.avgLatency * 1000) + "ms";
            decision.timestamp = currentTime_;
            decision.priority = (metrics.avgLatency > EMERGENCY_LATENCY_THRESHOLD) ? 1 : 2;
            
            decisions.push_back(decision);
            performanceMetrics_.migrationEvents++;
        }
    }
    
    // Emergency response for safety messages
    if (metrics.emergencyMessages > 0) {
        NFVDecision decision;
        decision.vnfType = VNFType::SECURITY_VNF;
        decision.action = "SCALE_UP";
        decision.targetInstances = 2;
        decision.targetLocation = "RSU_1";
        decision.reason = "Emergency messages detected: " + std::to_string(metrics.emergencyMessages);
        decision.timestamp = currentTime_;
        decision.priority = 1; // Critical
        
        decisions.push_back(decision);
        performanceMetrics_.emergencyResponses++;
    }
    
    performanceMetrics_.totalDecisions += decisions.size();
    
    return decisions;
}

bool OMNeTOrchestrator::shouldScaleUp(const NDNMetrics& metrics, VNFType vnfType) {
    switch (vnfType) {
        case VNFType::NDN_ROUTER:
            return metrics.pitSize > PIT_SCALE_THRESHOLD;
        case VNFType::TRAFFIC_ANALYZER:
            return metrics.networkUtilization > 0.8;
        case VNFType::SECURITY_VNF:
            return metrics.emergencyMessages > 5;
        case VNFType::CACHE_OPTIMIZER:
            return metrics.cacheHitRatio < 0.3;
        default:
            return false;
    }
}

int OMNeTOrchestrator::calculateRequiredInstances(const NDNMetrics& metrics, VNFType vnfType) {
    switch (vnfType) {
        case VNFType::NDN_ROUTER:
            return std::min(5, static_cast<int>(metrics.pitSize / 50) + 1);
        case VNFType::TRAFFIC_ANALYZER:
            return std::min(3, static_cast<int>(metrics.networkUtilization * 4));
        case VNFType::SECURITY_VNF:
            return std::min(3, static_cast<int>(metrics.emergencyMessages / 3) + 1);
        default:
            return 1;
    }
}

std::string OMNeTOrchestrator::findOptimalLocation(const NDNMetrics& metrics, VNFType vnfType) {
    // Simple location optimization based on VNF type and metrics
    switch (vnfType) {
        case VNFType::NDN_ROUTER:
            return (metrics.avgLatency > 0.05) ? "EDGE_1" : "RSU_1";
        case VNFType::TRAFFIC_ANALYZER:
            return "EDGE_1"; // Always at edge for better processing
        case VNFType::SECURITY_VNF:
            return "RSU_1"; // Close to vehicles for faster response
        case VNFType::CACHE_OPTIMIZER:
            return (metrics.cacheHitRatio < 0.4) ? "EDGE_2" : "EDGE_1";
        default:
            return "RSU_1";
    }
}

void OMNeTOrchestrator::initializeKathmanduTopology() {
    std::cout << "🏙️  Initializing Kathmandu intersection topology..." << std::endl;
    
    // Kathmandu Ring Road intersection coordinates (simplified)
    kathmanduIntersection_.x = 0.0;
    kathmanduIntersection_.y = 0.0;
    kathmanduIntersection_.currentPhase = 0;
    kathmanduIntersection_.phaseTimer = 30.0; // 30 second phases
    
    // Clear existing vehicles
    vehicles_.clear();
}

void OMNeTOrchestrator::generateKathmanduTraffic() {
    std::cout << "🚗 Generating Kathmandu traffic (density: " << trafficDensity_ << ")" << std::endl;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    
    int vehicleCount = 0;
    if (trafficDensity_ == "light") {
        std::uniform_int_distribution<> dist(2, 10);
        vehicleCount = dist(gen);
    } else if (trafficDensity_ == "normal") {
        std::uniform_int_distribution<> dist(10, 25);
        vehicleCount = dist(gen);
    } else if (trafficDensity_ == "heavy") {
        std::uniform_int_distribution<> dist(25, 50);
        vehicleCount = dist(gen);
    }
    
    for (int i = 0; i < vehicleCount; ++i) {
        VehicleInfo vehicle;
        vehicle.id = "ktm_vehicle_" + std::to_string(i);
        
        // Position vehicles on approach roads
        int approach = std::uniform_int_distribution<>(0, 3)(gen);
        switch (approach) {
            case 0: // North approach
                vehicle.x = std::uniform_real_distribution<>(-50.0, 50.0)(gen);
                vehicle.y = std::uniform_real_distribution<>(100.0, 300.0)(gen);
                vehicle.heading = 180.0; // Southbound
                break;
            case 1: // South approach
                vehicle.x = std::uniform_real_distribution<>(-50.0, 50.0)(gen);
                vehicle.y = std::uniform_real_distribution<>(-300.0, -100.0)(gen);
                vehicle.heading = 0.0; // Northbound
                break;
            case 2: // East approach
                vehicle.x = std::uniform_real_distribution<>(100.0, 300.0)(gen);
                vehicle.y = std::uniform_real_distribution<>(-50.0, 50.0)(gen);
                vehicle.heading = 270.0; // Westbound
                break;
            case 3: // West approach
                vehicle.x = std::uniform_real_distribution<>(-300.0, -100.0)(gen);
                vehicle.y = std::uniform_real_distribution<>(-50.0, 50.0)(gen);
                vehicle.heading = 90.0; // Eastbound
                break;
        }
        
        vehicle.z = 0.0;
        vehicle.speed = std::uniform_real_distribution<>(5.0, 15.0)(gen); // Kathmandu traffic speeds
        vehicle.timestamp = currentTime_;
        
        vehicles_.push_back(vehicle);
    }
    
    std::cout << "✅ Generated " << vehicleCount << " vehicles for Kathmandu scenario" << std::endl;
}

// Parse NDN metrics from JSON string
NDNMetrics OMNeTOrchestrator::parseNDNMetrics(const std::string& jsonStr) {
    NDNMetrics metrics;
    Json::Value json;
    Json::CharReaderBuilder builder;
    std::string errors;
    
    std::istringstream stream(jsonStr);
    if (!Json::parseFromStream(builder, stream, &json, &errors)) {
        std::cerr << "❌ Failed to parse NDN metrics JSON: " << errors << std::endl;
        return metrics; // Return default metrics
    }
    
    // Parse metrics fields
    metrics.pitSize = json.get("pit_size", 0).asUInt();
    metrics.fibEntries = json.get("fib_entries", 0).asUInt();
    metrics.cacheHitRatio = json.get("cache_hit_ratio", 0.0).asDouble();
    metrics.interestCount = json.get("interest_count", 0).asUInt64();
    metrics.dataCount = json.get("data_count", 0).asUInt64();
    metrics.avgLatency = json.get("avg_latency", 0.0).asDouble();
    metrics.unsatisfiedInterests = json.get("unsatisfied_interests", 0).asUInt();
    metrics.timestamp = json.get("timestamp", 0.0).asDouble();
    metrics.emergencyMessages = json.get("emergency_messages", 0).asUInt();
    metrics.safetyMessages = json.get("safety_messages", 0).asUInt();
    metrics.networkUtilization = json.get("network_utilization", 0.0).asDouble();
    
    return metrics;
}

// ============================================================================
// Missing implementations for OMNeT++ Orchestrator
// ============================================================================

void OMNeTOrchestrator::shutdown() {
    std::cout << "🔌 Shutting down OMNeT++ orchestrator..." << std::endl;
    running_ = false;
    
    // Close sockets
    if (followerSocket_ >= 0) {
        close(followerSocket_);
        followerSocket_ = -1;
    }
    if (serverSocket_ >= 0) {
        close(serverSocket_);
        serverSocket_ = -1;
    }
    
    std::cout << "✅ OMNeT++ orchestrator shutdown complete" << std::endl;
}

bool OMNeTOrchestrator::deployVNF(VNFType type, const std::string& location) {
    std::cout << "🚀 Deploying " << vnfTypeToString(type) << " VNF at " << location << std::endl;
    
    VNFInstance instance;
    instance.instanceId = vnfTypeToString(type) + "_" + std::to_string(vnfInstances_[type].size());
    instance.type = type;
    instance.location = location;
    instance.isActive = true;
    instance.cpuUsage = 0.3;  // Initial load
    instance.memoryUsage = 0.2;
    instance.createdAt = std::chrono::steady_clock::now();
    
    vnfInstances_[type].push_back(instance);
    std::cout << "✅ VNF " << instance.instanceId << " deployed successfully" << std::endl;
    
    return true;
}

CoSimMessage OMNeTOrchestrator::receiveMessage(int socket) {
    CoSimMessage message;
    char buffer[4096];
    
    ssize_t bytesReceived = recv(socket, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);
    
    if (bytesReceived > 0) {
        buffer[bytesReceived] = '\0';
        std::string data(buffer);
        
        // Parse JSON message
        Json::Value json;
        Json::CharReaderBuilder builder;
        std::string errors;
        std::istringstream stream(data);
        
        if (Json::parseFromStream(builder, stream, &json, &errors)) {
            std::string type = json.get("type", "").asString();
            
            if (type == "NDN_METRICS") {
                message.type = CoSimMessage::NDN_METRICS;
                message.payload = data;
                message.timestamp = json.get("timestamp", 0.0).asDouble();
            } else if (type == "TIME_SYNC_ACK") {
                message.type = CoSimMessage::TIME_SYNC;
                message.timestamp = json.get("timestamp", 0.0).asDouble();
            }
        }
    }
    
    return message;
}

bool OMNeTOrchestrator::sendMessage(int socket, const CoSimMessage& message) {
    // Convert message to string format
    std::string data = message.payload;
    
    ssize_t bytesSent = send(socket, data.c_str(), data.length(), 0);
    if (bytesSent < 0) {
        std::cerr << "❌ Failed to send message to follower" << std::endl;
        return false;
    }
    return true;
}

void OMNeTOrchestrator::simulateIntersectionBehavior(double timeStep) {
    // Update traffic light phases for Kathmandu intersection
    kathmanduIntersection_.phaseTimer += timeStep;
    
    if (kathmanduIntersection_.phaseTimer >= 30.0) { // 30-second phases
        kathmanduIntersection_.currentPhase = (kathmanduIntersection_.currentPhase + 1) % 4;
        kathmanduIntersection_.phaseTimer = 0.0;
        
        std::cout << "🚦 Kathmandu intersection phase changed to " 
                  << kathmanduIntersection_.currentPhase << std::endl;
    }
}

void OMNeTOrchestrator::updateVehiclePositions(double timeStep) {
    // Update vehicle positions based on movement models
    for (auto& vehicle : vehicles_) {
        // Simple linear movement
        vehicle.x += vehicle.vx * timeStep;
        vehicle.y += vehicle.vy * timeStep;
        vehicle.timestamp = currentTime_.load();
        
        // Keep vehicles within intersection bounds
        if (vehicle.x < -500 || vehicle.x > 500 || vehicle.y < -500 || vehicle.y > 500) {
            // Reset vehicle position (simulate new vehicle entering)
            vehicle.x = -500 + (std::rand() % 100);
            vehicle.y = -500 + (std::rand() % 100);
        }
    }
}

void OMNeTOrchestrator::generateV2XMessages() {
    // Generate safety and awareness messages
    for (const auto& vehicle : vehicles_) {
        // Simulate emergency braking scenario
        if (vehicle.speed > 50.0 && std::rand() % 1000 < 5) { // 0.5% chance
            std::cout << "🚨 Emergency braking message generated by " << vehicle.id << std::endl;
            performanceMetrics_.emergencyResponses++;
        }
    }
}

void OMNeTOrchestrator::handleEmergencyScenarios() {
    // Check for emergency scenarios based on vehicle positions and speeds
    for (size_t i = 0; i < vehicles_.size(); ++i) {
        for (size_t j = i + 1; j < vehicles_.size(); ++j) {
            double distance = std::sqrt(
                std::pow(vehicles_[i].x - vehicles_[j].x, 2) + 
                std::pow(vehicles_[i].y - vehicles_[j].y, 2)
            );
            
            if (distance < 50.0 && vehicles_[i].speed > 30.0 && vehicles_[j].speed > 30.0) {
                std::cout << "⚠️ Potential collision detected between " 
                          << vehicles_[i].id << " and " << vehicles_[j].id << std::endl;
            }
        }
    }
}

void OMNeTOrchestrator::executeNFVDecisions(const std::vector<NFVDecision>& decisions) {
    for (const auto& decision : decisions) {
        std::cout << "🎯 Executing NFV decision: " << decision.action 
                  << " for " << vnfTypeToString(decision.vnfType) << std::endl;
        
        if (decision.action == "SCALE_UP") {
            for (int i = 0; i < decision.targetInstances; ++i) {
                deployVNF(decision.vnfType, decision.targetLocation);
            }
            performanceMetrics_.scalingEvents++;
            
        } else if (decision.action == "SCALE_DOWN") {
            // Remove VNF instances
            auto& instances = vnfInstances_[decision.vnfType];
            if (!instances.empty()) {
                instances.pop_back();
                std::cout << "⬇️ Scaled down " << vnfTypeToString(decision.vnfType) << std::endl;
            }
            
        } else if (decision.action == "MIGRATE") {
            // Simulate VNF migration
            auto& instances = vnfInstances_[decision.vnfType];
            if (!instances.empty()) {
                instances[0].location = decision.targetLocation;
                std::cout << "📦 Migrated " << vnfTypeToString(decision.vnfType) 
                          << " to " << decision.targetLocation << std::endl;
                performanceMetrics_.migrationEvents++;
            }
        }
        
        logDecisionMaking(decision);
    }
}

bool OMNeTOrchestrator::shouldMigrate(const NDNMetrics& metrics, VNFType type) {
    // Decision logic for VNF migration based on metrics
    if (metrics.avgLatency > LATENCY_THRESHOLD && type == VNFType::NDN_ROUTER) {
        return true;
    }
    
    if (metrics.cacheHitRatio < CACHE_EFFICIENCY_THRESHOLD && type == VNFType::CACHE_OPTIMIZER) {
        return true;
    }
    
    return false;
}

void OMNeTOrchestrator::updatePerformanceMetrics() {
    performanceMetrics_.totalDecisions++;
    
    // Calculate resource utilization
    double totalCpu = 0.0;
    int totalInstances = 0;
    
    for (const auto& [type, instances] : vnfInstances_) {
        for (const auto& instance : instances) {
            totalCpu += instance.cpuUsage;
            totalInstances++;
        }
    }
    
    if (totalInstances > 0) {
        performanceMetrics_.resourceUtilization = totalCpu / totalInstances;
    }
}

void OMNeTOrchestrator::logDecisionMaking(const NFVDecision& decision) {
    std::cout << "📊 NFV Decision Log: " << decision.action 
              << " for " << vnfTypeToString(decision.vnfType)
              << " at " << decision.timestamp << "s" << std::endl;
}

std::vector<VehicleInfo> OMNeTOrchestrator::getVehicleData() {
    return vehicles_;
}

void OMNeTOrchestrator::updateVehicleData(const std::vector<VehicleInfo>& vehicleData) {
    vehicles_ = vehicleData;
}

} // namespace cosim

// ============================================================================
// Utility functions for VNF management
// ============================================================================

std::string cosim::vnfTypeToString(cosim::VNFType type) {
    switch (type) {
        case cosim::VNFType::NDN_ROUTER: return "NDNRouter";
        case cosim::VNFType::TRAFFIC_ANALYZER: return "TrafficAnalyzer";
        case cosim::VNFType::SECURITY_VNF: return "SecurityVNF";
        case cosim::VNFType::CACHE_OPTIMIZER: return "CacheOptimizer";
        default: return "Unknown";
    }
}

cosim::VNFType cosim::stringToVNFType(const std::string& str) {
    if (str == "NDNRouter") return cosim::VNFType::NDN_ROUTER;
    if (str == "TrafficAnalyzer") return cosim::VNFType::TRAFFIC_ANALYZER;
    if (str == "SecurityVNF") return cosim::VNFType::SECURITY_VNF;
    if (str == "CacheOptimizer") return cosim::VNFType::CACHE_OPTIMIZER;
    return cosim::VNFType::NDN_ROUTER; // default
}

std::string cosim::nfvDecisionToJson(const cosim::NFVDecision& decision) {
    Json::Value json;
    json["vnf_type"] = vnfTypeToString(decision.vnfType);
    json["action"] = decision.action;
    json["target_instances"] = decision.targetInstances;
    json["source_location"] = decision.sourceLocation;
    json["target_location"] = decision.targetLocation;
    json["reason"] = decision.reason;
    json["timestamp"] = decision.timestamp;
    json["priority"] = decision.priority;
    
    Json::StreamWriterBuilder builder;
    return Json::writeString(builder, json);
}

cosim::NFVDecision cosim::jsonToNFVDecision(const std::string& jsonStr) {
    cosim::NFVDecision decision;
    Json::Value json;
    Json::CharReaderBuilder builder;
    std::string errors;
    
    std::istringstream stream(jsonStr);
    if (Json::parseFromStream(builder, stream, &json, &errors)) {
        decision.vnfType = stringToVNFType(json.get("vnf_type", "NDNRouter").asString());
        decision.action = json.get("action", "SCALE_UP").asString();
        decision.targetInstances = json.get("target_instances", 1).asInt();
        decision.sourceLocation = json.get("source_location", "").asString();
        decision.targetLocation = json.get("target_location", "").asString();
        decision.reason = json.get("reason", "").asString();
        decision.timestamp = json.get("timestamp", 0.0).asDouble();
        decision.priority = json.get("priority", 1).asInt();
    }
    
    return decision;
}
