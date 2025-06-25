/*
NS-3 Co-simulation Script for ndnSIM
This script runs within NS-3 and communicates with the co-simulation platform
*/

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/wifi-module.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <iomanip>
#include <signal.h>  
#include <errno.h>   

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CoSimulationScript");

// Global flag for graceful shutdown
static volatile bool g_shutdown = false;

// Signal handler for SIGPIPE
void sigpipe_handler(int sig) {
    NS_LOG_WARN("SIGPIPE received - connection broken");
    g_shutdown = true;
}

class CoSimulationManager {
public:
    CoSimulationManager(int port) : m_port(port), m_running(true), m_socket(-1), m_connectionActive(false) {}
    
    void Initialize() {
        // Install signal handler
        signal(SIGPIPE, sigpipe_handler);
        
        ConnectToCoSimulator();
        SetupSimulation();
    }
    
    void Run() {
        NS_LOG_INFO("Starting co-simulation manager");
        
        // Start the communication thread
        std::thread commThread(&CoSimulationManager::CommunicationLoop, this);
        
        // Run NS-3 simulation
        Simulator::Run();
        
        m_running = false;
        if (commThread.joinable()) {
            commThread.join();
        }
        
        Simulator::Destroy();
        
        if (m_socket >= 0) {
            close(m_socket);
        }
    }

private:
    void ConnectToCoSimulator() {
        m_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (m_socket < 0) {
            NS_LOG_ERROR("Failed to create socket");
            return;
        }
        
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(m_port);
        server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
        
        // Wait for co-simulator to be ready
        sleep(2);
        
        if (connect(m_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            NS_LOG_ERROR("Failed to connect to co-simulation platform on port " << m_port);
            close(m_socket);
            m_socket = -1;
            return;
        }
        
        m_connectionActive = true;
        NS_LOG_INFO("Connected to co-simulation platform on port " << m_port);
    }
    
    void SetupSimulation() {
        // Create nodes (vehicles)
        NodeContainer nodes;
        nodes.Create(3);  // Create 3 vehicles
        
        // Setup mobility model
        MobilityHelper mobility;
        mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                     "MinX", DoubleValue(100.0),
                                     "MinY", DoubleValue(50.0),
                                     "DeltaX", DoubleValue(100.0),
                                     "DeltaY", DoubleValue(50.0),
                                     "GridWidth", UintegerValue(3),
                                     "LayoutType", StringValue("RowFirst"));
        
        mobility.SetMobilityModel("ns3::ConstantVelocityMobilityModel");
        mobility.Install(nodes);
        
        // Set initial velocities for each vehicle
        for (uint32_t i = 0; i < nodes.GetN(); ++i) {
            Ptr<ConstantVelocityMobilityModel> mob = nodes.Get(i)->GetObject<ConstantVelocityMobilityModel>();
            // Different speeds and slight variations in direction
            double speed = 15.0 + i * 3.0;  // 15, 18, 21 m/s
            double angle = i * 10.0;        // 0, 10, 20 degrees
            double radians = angle * M_PI / 180.0;
            
            Vector velocity(speed * cos(radians), speed * sin(radians), 0.0);
            mob->SetVelocity(velocity);
        }
        
        m_nodes = nodes;
        
        NS_LOG_INFO("Created " << nodes.GetN() << " vehicles with mobility");
        
        // Schedule periodic vehicle data updates
        Simulator::Schedule(Seconds(0.1), &CoSimulationManager::SendVehicleData, this);
    }
    
    void SendVehicleData() {
        // Check if we should shutdown
        if (g_shutdown || !m_running || !m_connectionActive || m_socket < 0) {
            NS_LOG_INFO("Stopping vehicle data transmission");
            return;
        }
        
        // Check if socket is still valid
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(m_socket, SOL_SOCKET, SO_ERROR, &error, &len) != 0 || error != 0) {
            NS_LOG_WARN("Socket error detected, stopping transmission");
            m_connectionActive = false;
            return;
        }
        
        for (uint32_t i = 0; i < m_nodes.GetN(); ++i) {
            Ptr<MobilityModel> mob = m_nodes.Get(i)->GetObject<MobilityModel>();
            Vector pos = mob->GetPosition();
            Vector vel = mob->GetVelocity();
            
            // Calculate heading from velocity
            double heading = atan2(vel.y, vel.x) * 180.0 / M_PI;
            if (heading < 0) heading += 360.0;
            
            std::ostringstream oss;
            oss << "VEHICLE_DATA:ns3_vehicle_" << std::setfill('0') << std::setw(3) << i << ","
                << pos.x << "," << pos.y << "," << pos.z << ","
                << vel.GetLength() << "," << heading << "," << Simulator::Now().GetSeconds() << "\n";
            
            std::string data = oss.str();
            
            // Use MSG_NOSIGNAL to prevent SIGPIPE and check for errors
            ssize_t sent = send(m_socket, data.c_str(), data.length(), MSG_NOSIGNAL);
            if (sent < 0) {
                if (errno == EPIPE || errno == ECONNRESET || errno == ENOTCONN) {
                    NS_LOG_WARN("Connection broken while sending data, stopping transmission");
                    m_connectionActive = false;
                    return;
                }
            }
        }
        
        // Schedule next update only if connection is still active
        if (m_running && m_connectionActive && !g_shutdown && Simulator::Now().GetSeconds() < 10.0) {
            Simulator::Schedule(Seconds(0.5), &CoSimulationManager::SendVehicleData, this);
        } else {
            NS_LOG_INFO("Ending vehicle data transmission");
        }
    }
    
    void CommunicationLoop() {
        char buffer[1024];
        
        while (m_running && !g_shutdown) {
            if (m_socket < 0 || !m_connectionActive) break;
            
            memset(buffer, 0, sizeof(buffer));
            ssize_t bytes = recv(m_socket, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);
            
            if (bytes > 0) {
                std::string message(buffer, bytes);
                ProcessCommand(message);
            } else if (bytes == 0) {
                // Connection closed by peer
                NS_LOG_INFO("Connection closed by co-simulation platform");
                m_connectionActive = false;
                break;
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
                // Real error occurred
                NS_LOG_WARN("Error in recv: " << strerror(errno));
                m_connectionActive = false;
                break;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        NS_LOG_INFO("Communication loop ended");
    }
    
    void ProcessCommand(const std::string& message) {
        NS_LOG_INFO("Received command: " << message.substr(0, 50) << "...");
        
        if (message.find("STEP:") == 0) {
            // Extract time step
            double timeStep = std::stod(message.substr(5));
            NS_LOG_INFO("Step command for " << timeStep << " seconds");
            
            // Send response immediately for now
            SendStepComplete();
            
        } else if (message.find("UPDATE_VEHICLE:") == 0) {
            // Process external vehicle updates
            std::string vehicleData = message.substr(15);
            NS_LOG_INFO("External vehicle update: " << vehicleData.substr(0, 30) << "...");
            
        } else if (message.find("SHUTDOWN") == 0) {
            NS_LOG_INFO("Shutdown command received");
            m_running = false;
            m_connectionActive = false;
            Simulator::Stop();
        }
    }
    
    void SendStepComplete() {
        if (m_socket >= 0 && m_connectionActive) {
            std::string response = "STEP_COMPLETE\n";
            ssize_t sent = send(m_socket, response.c_str(), response.length(), MSG_NOSIGNAL);
            if (sent < 0 && (errno == EPIPE || errno == ECONNRESET)) {
                NS_LOG_WARN("Connection broken while sending step complete");
                m_connectionActive = false;
            }
        }
    }
    
    int m_port;
    bool m_running;
    int m_socket;
    bool m_connectionActive;  // Add this flag
    NodeContainer m_nodes;
};

int main(int argc, char *argv[]) {
    CommandLine cmd;
    int port = 9999;
    cmd.AddValue("port", "Communication port", port);
    cmd.Parse(argc, argv);
    
    LogComponentEnable("CoSimulationScript", LOG_LEVEL_INFO);
    
    NS_LOG_INFO("Starting NS-3 Co-simulation Script on port " << port);
    
    CoSimulationManager manager(port);
    manager.Initialize();
    manager.Run();
    
    return 0;
}