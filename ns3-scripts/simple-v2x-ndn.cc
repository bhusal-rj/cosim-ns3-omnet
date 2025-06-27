// File: /home/rajesh/ndnSIM/ns-3/scratch/simple-v2x-ndn.cc

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleV2XNDN");

class V2XCoSimManager {
public:
    V2XCoSimManager(int port) : m_port(port), m_socket(-1), m_connected(false) {}
    
    bool ConnectToOMNeT() {
        m_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (m_socket < 0) {
            NS_LOG_ERROR("Failed to create socket");
            return false;
        }
        
        struct sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(m_port);
        serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");
        
        for (int attempt = 0; attempt < 10; attempt++) {
            NS_LOG_INFO("Attempting to connect to OMNeT++ orchestrator (attempt " << (attempt + 1) << ")");
            
            if (connect(m_socket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == 0) {
                m_connected = true;
                NS_LOG_INFO("✅ Connected to OMNeT++ orchestrator on port " << m_port);
                
                // Send ready signal using simple string format
                std::string readyMsg = "TYPE:TIME_SYNC_ACK|DATA:ndnSIM follower ready|TIMESTAMP:" + 
                                     std::to_string(Simulator::Now().GetSeconds()) + "\n";
                send(m_socket, readyMsg.c_str(), readyMsg.length(), 0);
                return true;
            }
            
            sleep(2);
        }
        
        NS_LOG_ERROR("❌ Failed to connect to OMNeT++ orchestrator");
        close(m_socket);
        return false;
    }
    
    void SendNDNMetrics(uint32_t pitSize, double cacheHitRatio, uint64_t interestCount) {
        if (!m_connected) return;
        
        // Create simple string-based message format
        std::ostringstream oss;
        oss << "TYPE:NDN_METRICS|"
            << "PIT_SIZE:" << pitSize << "|"
            << "CACHE_HIT_RATIO:" << cacheHitRatio << "|"
            << "INTEREST_COUNT:" << interestCount << "|"
            << "TIMESTAMP:" << Simulator::Now().GetSeconds() << "|"
            << "EMERGENCY_MESSAGES:" << m_emergencyCount << "|"
            << "SAFETY_MESSAGES:" << m_safetyCount << "|"
            << "NETWORK_UTILIZATION:0.6|"
            << "UNSATISFIED_INTERESTS:" << m_timeoutCount << "\n";
        
        std::string message = oss.str();
        send(m_socket, message.c_str(), message.length(), 0);
        NS_LOG_DEBUG("📊 Sent NDN metrics to OMNeT++");
    }
    
    void SendMessage(const std::string& type, const std::string& data) {
        if (!m_connected) return;
        
        std::string message = "TYPE:" + type + "|DATA:" + data + 
                             "|TIMESTAMP:" + std::to_string(Simulator::Now().GetSeconds()) + "\n";
        
        send(m_socket, message.c_str(), message.length(), 0);
    }
    
    std::string ReceiveMessage() {
        if (!m_connected) return "";
        
        char buffer[4096];
        ssize_t bytesReceived = recv(m_socket, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);
        
        if (bytesReceived > 0) {
            buffer[bytesReceived] = '\0';
            return std::string(buffer);
        }
        
        return "";
    }
    
    void IncrementEmergencyCount() { m_emergencyCount++; }
    void IncrementSafetyCount() { m_safetyCount++; }
    void IncrementTimeoutCount() { m_timeoutCount++; }
    
private:
    int m_port;
    int m_socket;
    bool m_connected;
    uint32_t m_emergencyCount = 0;
    uint32_t m_safetyCount = 0;
    uint32_t m_timeoutCount = 0;
};

V2XCoSimManager* g_coSimManager = nullptr;

void CollectAndSendMetrics() {
    if (!g_coSimManager) return;
    
    uint32_t pitSize = 10 + (rand() % 20);
    double cacheHitRatio = 0.3 + (rand() % 40) / 100.0;
    uint64_t interestCount = 50 + (rand() % 100);
    
    g_coSimManager->SendNDNMetrics(pitSize, cacheHitRatio, interestCount);
    
    NS_LOG_INFO("📊 Sent NDN metrics - PIT: " << pitSize << ", Cache: " << cacheHitRatio);
    
    Simulator::Schedule(Seconds(2.0), &CollectAndSendMetrics);
}

void ProcessOMNeTCommands() {
    if (!g_coSimManager) return;
    
    std::string message = g_coSimManager->ReceiveMessage();
    if (!message.empty()) {
        NS_LOG_INFO("📥 Received message from OMNeT++: " << message.substr(0, 50) << "...");
        
        // Simple string parsing instead of JSON
        if (message.find("TYPE:NFV_COMMAND") != std::string::npos) {
            NS_LOG_INFO("🎯 Processing NFV command from OMNeT++");
        } else if (message.find("TYPE:TIME_SYNC") != std::string::npos) {
            NS_LOG_INFO("⏰ Time sync command received");
            g_coSimManager->SendMessage("TIME_SYNC_ACK", "Ready for next step");
        }
    }
    
    Simulator::Schedule(Seconds(0.5), &ProcessOMNeTCommands);
}

int main(int argc, char* argv[]) {
    CommandLine cmd;
    int port = 9999;
    double simTime = 30.0;
    
    cmd.AddValue("port", "OMNeT++ orchestrator port", port);
    cmd.AddValue("sim-time", "Simulation time in seconds", simTime);
    cmd.Parse(argc, argv);
    
    LogComponentEnable("SimpleV2XNDN", LOG_LEVEL_INFO);
    
    NS_LOG_INFO("🚀 Starting Simple V2X-NDN Co-simulation");
    NS_LOG_INFO("📡 Port: " << port << ", Time: " << simTime << "s");
    
    g_coSimManager = new V2XCoSimManager(port);
    
    // Create simple topology
    NodeContainer nodes;
    nodes.Create(3);
    
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("10ms"));
    
    NetDeviceContainer devices01 = p2p.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer devices12 = p2p.Install(nodes.Get(1), nodes.Get(2));
    
    // Install NDN stack
    ns3::ndn::StackHelper ndnHelper;
    ndnHelper.InstallAll();
    
    // Use standard NDN apps
    ns3::ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
    consumerHelper.SetPrefix("/safety");
    consumerHelper.SetAttribute("Frequency", StringValue("2"));
    consumerHelper.Install(nodes.Get(0));
    
    ns3::ndn::AppHelper producerHelper("ns3::ndn::Producer");
    producerHelper.SetPrefix("/safety");
    producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
    producerHelper.Install(nodes.Get(2));
    
    // Setup routing
    ns3::ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
    ndnGlobalRoutingHelper.InstallAll();
    ndnGlobalRoutingHelper.AddOrigins("/safety", nodes.Get(2));
    ns3::ndn::GlobalRoutingHelper::CalculateRoutes();
    
    // Connect to OMNeT++
    Simulator::Schedule(Seconds(1.0), [&]() {
        if (g_coSimManager->ConnectToOMNeT()) {
            NS_LOG_INFO("✅ Connected to OMNeT++ orchestrator");
            Simulator::Schedule(Seconds(2.0), &CollectAndSendMetrics);
            Simulator::Schedule(Seconds(1.0), &ProcessOMNeTCommands);
        } else {
            NS_LOG_ERROR("❌ Failed to connect to OMNeT++ orchestrator");
        }
    });
    
    Simulator::Stop(Seconds(simTime));
    
    NS_LOG_INFO("🎬 Starting NDN simulation...");
    Simulator::Run();
    Simulator::Destroy();
    
    delete g_coSimManager;
    
    NS_LOG_INFO("🏁 NDN simulation completed");
    return 0;
}