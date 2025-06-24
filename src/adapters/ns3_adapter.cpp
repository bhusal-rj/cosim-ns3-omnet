/*
Implementation of NS-3 adapter for real integration
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

namespace cosim {

NS3Adapter::NS3Adapter(const std::string& configFile)
    : ns3ConfigFile_(configFile), communicationPort_("9999"),
      currentTime_(0.0), running_(false), initialized_(false),
      socketFd_(-1), ns3ProcessId_(-1) {
    
    // Default NS-3 script path
    ns3ScriptPath_ = "./ns3-scripts/cosim-script.cc";
}

NS3Adapter::~NS3Adapter() {
    shutdown();
}

bool NS3Adapter::initialize() {
    std::cout << "Initializing NS-3 Adapter..." << std::endl;
    
    if (initialized_) {
        std::cout << "NS-3 Adapter already initialized" << std::endl;
        return true;
    }
    
    // Setup communication first
    if (!setupCommunication()) {
        std::cerr << "Failed to setup communication with NS-3" << std::endl;
        return false;
    }
    
    // Start NS-3 process
    if (!startNS3Process()) {
        std::cerr << "Failed to start NS-3 process" << std::endl;
        return false;
    }
    
    // Wait for NS-3 to initialize
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // Verify NS-3 is running
    if (!isNS3ProcessRunning()) {
        std::cerr << "NS-3 process failed to start properly" << std::endl;
        return false;
    }
    
    // Start communication thread
    communicationThread_ = std::thread(&NS3Adapter::communicationLoop, this);
    
    initialized_ = true;
    running_ = true;
    currentTime_ = 0.0;
    
    std::cout << "NS-3 Adapter initialized successfully" << std::endl;
    return true;
}

bool NS3Adapter::step(double timeStep) {
    if (!running_ || !initialized_) {
        return false;
    }
    
    // Send step command to NS-3
    std::ostringstream oss;
    oss << "STEP:" << timeStep;
    
    if (!sendCommand(oss.str())) {
        std::cerr << "Failed to send step command to NS-3" << std::endl;
        return false;
    }
    
    // Wait for response
    std::string response = receiveResponse();
    if (response.empty() || response.find("STEP_COMPLETE") == std::string::npos) {
        std::cerr << "Invalid response from NS-3: " << response << std::endl;
        return false;
    }
    
    // Fix: Use store() method for atomic variable
    currentTime_.store(currentTime_.load() + timeStep);
    return true;
}

void NS3Adapter::shutdown() {
    if (!running_) return;
    
    std::cout << "Shutting down NS-3 Adapter..." << std::endl;
    
    running_ = false;
    
    // Send shutdown command to NS-3
    sendCommand("SHUTDOWN");
    
    // Stop NS-3 process
    stopNS3Process();
    
    // Close communication
    if (socketFd_ >= 0) {
        close(socketFd_);
        socketFd_ = -1;
    }
    
    // Wait for communication thread to finish
    if (communicationThread_.joinable()) {
        communicationThread_.join();
    }
    
    initialized_ = false;
    std::cout << "NS-3 Adapter shutdown complete" << std::endl;
}

std::vector<VehicleInfo> NS3Adapter::getVehicleData() {
    std::lock_guard<std::mutex> lock(vehiclesMutex_);
    return vehicles_;
}

void NS3Adapter::updateVehicleData(const std::vector<VehicleInfo>& vehicles) {
    // Send vehicle updates to NS-3
    for (const auto& vehicle : vehicles) {
        if (vehicle.id.find("ns3_") == std::string::npos) {  // Don't send back our own data
            std::string updateCmd = "UPDATE_VEHICLE:" + formatVehicleUpdate(vehicle);
            sendCommand(updateCmd);
        }
    }
}

bool NS3Adapter::startNS3Process() {
    std::cout << "Starting NS-3 process..." << std::endl;
    
    // Fork to create NS-3 process
    ns3ProcessId_ = fork();
    
    if (ns3ProcessId_ == 0) {
        // Child process - execute NS-3
        std::string command = "cd /usr/local/ns-allinone-3.39/ns-3.39 && ";
        command += "./ns3 run '" + ns3ScriptPath_ + " --port=" + communicationPort_ + "'";
        
        execl("/bin/sh", "sh", "-c", command.c_str(), (char*)NULL);
        
        // If we reach here, exec failed
        std::cerr << "Failed to execute NS-3 command" << std::endl;
        exit(1);
    } else if (ns3ProcessId_ > 0) {
        // Parent process
        std::cout << "NS-3 process started with PID: " << ns3ProcessId_ << std::endl;
        return true;
    } else {
        // Fork failed
        std::cerr << "Failed to fork NS-3 process" << std::endl;
        return false;
    }
}

void NS3Adapter::stopNS3Process() {
    if (ns3ProcessId_ > 0) {
        std::cout << "Stopping NS-3 process..." << std::endl;
        
        // Send SIGTERM to NS-3 process
        kill(ns3ProcessId_, SIGTERM);
        
        // Wait for process to terminate
        int status;
        waitpid(ns3ProcessId_, &status, 0);
        
        ns3ProcessId_ = -1;
        std::cout << "NS-3 process stopped" << std::endl;
    }
}

bool NS3Adapter::isNS3ProcessRunning() {
    if (ns3ProcessId_ <= 0) return false;
    
    // Check if process is still running
    int status;
    pid_t result = waitpid(ns3ProcessId_, &status, WNOHANG);
    
    return (result == 0);  // 0 means process is still running
}

bool NS3Adapter::setupCommunication() {
    std::cout << "Setting up communication with NS-3..." << std::endl;
    
    // Create socket
    socketFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFd_ < 0) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(socketFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "Failed to set socket options" << std::endl;
        close(socketFd_);
        return false;
    }
    
    // Bind socket
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(std::stoi(communicationPort_));
    
    if (bind(socketFd_, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Failed to bind socket to port " << communicationPort_ << std::endl;
        close(socketFd_);
        return false;
    }
    
    // Listen for connections
    if (listen(socketFd_, 1) < 0) {
        std::cerr << "Failed to listen on socket" << std::endl;
        close(socketFd_);
        return false;
    }
    
    std::cout << "Communication setup complete on port " << communicationPort_ << std::endl;
    return true;
}

void NS3Adapter::communicationLoop() {
    std::cout << "Starting communication loop..." << std::endl;
    
    // Accept connection from NS-3
    struct sockaddr_in clientAddress;
    socklen_t clientLen = sizeof(clientAddress);
    int clientSocket = accept(socketFd_, (struct sockaddr*)&clientAddress, &clientLen);
    
    if (clientSocket < 0) {
        std::cerr << "Failed to accept NS-3 connection" << std::endl;
        return;
    }
    
    std::cout << "NS-3 connected successfully" << std::endl;
    
    // Communication loop
    char buffer[1024];
    while (running_) {
        memset(buffer, 0, sizeof(buffer));
        
        ssize_t bytesRead = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) {
            if (running_) {
                std::cerr << "Lost connection to NS-3" << std::endl;
            }
            break;
        }
        
        std::string message(buffer, bytesRead);
        
        // Process received message
        if (message.find("VEHICLE_DATA:") == 0) {
            // Parse vehicle data
            VehicleInfo vehicle = parseVehicleData(message.substr(13));
            
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
        }
    }
    
    close(clientSocket);
    std::cout << "Communication loop ended" << std::endl;
}

bool NS3Adapter::sendCommand(const std::string& command) {
    std::lock_guard<std::mutex> lock(queueMutex_);
    messageQueue_.push(command);
    return true;
}

std::string NS3Adapter::receiveResponse() {
    // For now, return a mock response
    // In real implementation, this would wait for actual NS-3 response
    return "STEP_COMPLETE";
}

VehicleInfo NS3Adapter::parseVehicleData(const std::string& data) {
    VehicleInfo vehicle;
    
    // Parse comma-separated vehicle data
    // Format: id,x,y,z,speed,heading,timestamp
    std::istringstream iss(data);
    std::string token;
    
    if (std::getline(iss, token, ',')) vehicle.id = token;
    if (std::getline(iss, token, ',')) vehicle.x = std::stod(token);
    if (std::getline(iss, token, ',')) vehicle.y = std::stod(token);
    if (std::getline(iss, token, ',')) vehicle.z = std::stod(token);
    if (std::getline(iss, token, ',')) vehicle.speed = std::stod(token);
    if (std::getline(iss, token, ',')) vehicle.heading = std::stod(token);
    if (std::getline(iss, token, ',')) vehicle.timestamp = std::stod(token);
    
    return vehicle;
}

std::string NS3Adapter::formatVehicleUpdate(const VehicleInfo& vehicle) {
    std::ostringstream oss;
    oss << vehicle.id << "," << vehicle.x << "," << vehicle.y << "," << vehicle.z
        << "," << vehicle.speed << "," << vehicle.heading << "," << vehicle.timestamp;
    return oss.str();
}

} // namespace cosim