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

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CoSimulationScript");

class CoSimulationManager {
public:
    CoSimulationManager(int port) : m_port(port), m_running(true), m_socket(-1) {}
    
    void Initialize() {
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
        if (m_socket < 0) return;
        
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
            send(m_socket, data.c_str(), data.length(), 0);
        }
        
        // Schedule next update
        if (m_running && Simulator::Now().GetSeconds() < 10.0) {  // Run for 10 seconds max
            Simulator::Schedule(Seconds(0.5), &CoSimulationManager::SendVehicleData, this);
        }
    }
    
    void CommunicationLoop() {
        char buffer[1024];
        
        while (m_running) {
            if (m_socket < 0) break;
            
            memset(buffer, 0, sizeof(buffer));
            ssize_t bytes = recv(m_socket, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);
            
            if (bytes > 0) {
                std::string message(buffer, bytes);
                ProcessCommand(message);
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    void ProcessCommand(const std::string& message) {
        NS_LOG_INFO("Received command: " << message.substr(0, 50) << "...");  // Truncate long messages
        
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
            Simulator::Stop();
        }
    }
    
    void SendStepComplete() {
        if (m_socket >= 0) {
            std::string response = "STEP_COMPLETE\n";
            send(m_socket, response.c_str(), response.length(), 0);
        }
    }
    
    int m_port;
    bool m_running;
    int m_socket;
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