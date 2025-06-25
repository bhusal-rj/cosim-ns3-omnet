/*
NS-3 Co-simulation Script that runs existing ndnSIM examples and extracts data
This script integrates with existing ndnSIM simulations and sends data to OMNeT++
*/

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/ndnSIM-module.h"

// Add specific include for ndn::App
#include "ns3/ndnSIM/apps/ndn-app.hpp"

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
using namespace ns3::ndn;

NS_LOG_COMPONENT_DEFINE ("CoSimulationScript");

// Global flag for graceful shutdown
static volatile bool g_shutdown = false;

// Signal handler for SIGPIPE
void sigpipe_handler(int sig) {
    NS_LOG_WARN("SIGPIPE received - connection broken");
    g_shutdown = true;
}

// Data extraction callback class
class NDNDataExtractor {
public:
    // Fix callback signatures - parameters should be in correct order for ndnSIM traces
    static void OnInterestReceived(shared_ptr<const Interest> interest, Ptr<App> app, shared_ptr<Face> face) {
        uint32_t nodeId = app->GetNode()->GetId();
        std::string name = interest->getName().toUri();
        double timestamp = Simulator::Now().GetSeconds();
        
        // Store interest data for co-simulation
        std::ostringstream data;
        data << "NDN_INTEREST," << nodeId << "," << name << "," << timestamp << "," 
             << interest->getInterestLifetime().count() << "\n";
        
        // Send to co-simulation platform
        SendToCoSimulation(data.str());
    }
    
    static void OnDataReceived(shared_ptr<const Data> data, Ptr<App> app, shared_ptr<Face> face) {
        uint32_t nodeId = app->GetNode()->GetId();
        std::string name = data->getName().toUri();
        double timestamp = Simulator::Now().GetSeconds();
        size_t contentSize = data->getContent().size();
        
        // Store data packet info for co-simulation
        std::ostringstream output;
        output << "NDN_DATA," << nodeId << "," << name << "," << timestamp << "," << contentSize << "\n";
        
        // Send to co-simulation platform
        SendToCoSimulation(output.str());
    }
    
    static void OnInterestTimedOut(shared_ptr<const Interest> interest, Ptr<App> app) {
        uint32_t nodeId = app->GetNode()->GetId();
        std::string name = interest->getName().toUri();
        double timestamp = Simulator::Now().GetSeconds();
        
        std::ostringstream data;
        data << "NDN_TIMEOUT," << nodeId << "," << name << "," << timestamp << "\n";
        
        SendToCoSimulation(data.str());
    }
    
    static void SendToCoSimulation(const std::string& data);
    static void SetSocket(int socket, bool* active);
    
private:
    static int s_socket;
    static bool* s_active;
};

int NDNDataExtractor::s_socket = -1;
bool* NDNDataExtractor::s_active = nullptr;

void NDNDataExtractor::SetSocket(int socket, bool* active) {
    s_socket = socket;
    s_active = active;
}

void NDNDataExtractor::SendToCoSimulation(const std::string& data) {
    if (s_socket >= 0 && s_active && *s_active) {
        ssize_t sent = send(s_socket, data.c_str(), data.length(), MSG_NOSIGNAL);
        if (sent < 0 && (errno == EPIPE || errno == ECONNRESET)) {
            *s_active = false;
        }
    }
}

class CoSimulationManager {
public:
    CoSimulationManager(int port, const std::string& example = "simple") 
        : m_port(port), m_running(true), m_socket(-1), m_connectionActive(false), m_exampleType(example) {}
    
    void Initialize() {
        signal(SIGPIPE, sigpipe_handler);
        ConnectToCoSimulator();
        SetupNDNSimulation();
    }
    
    void Run() {
        NS_LOG_INFO("Starting NDN co-simulation manager");
        
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
        
        sleep(2);
        
        if (connect(m_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            NS_LOG_ERROR("Failed to connect to co-simulation platform on port " << m_port);
            close(m_socket);
            m_socket = -1;
            return;
        }
        
        m_connectionActive = true;
        NS_LOG_INFO("Connected to co-simulation platform on port " << m_port);
        
        // Set socket for data extractor
        NDNDataExtractor::SetSocket(m_socket, &m_connectionActive);
    }
    
    void SetupNDNSimulation() {
        // Choose which example to run based on command line parameter
        if (m_exampleType == "grid") {
            RunGridTopologyExample();
        } else {
            RunSimpleExample();
        }
        
        // Schedule periodic statistics collection
        Simulator::Schedule(Seconds(1.0), &CoSimulationManager::CollectStatistics, this);
        
        // Schedule simulation stop at 30 seconds
        Simulator::Schedule(Seconds(30.0), &CoSimulationManager::StopSimulation, this);
    }
    
    void StopSimulation() {
        NS_LOG_INFO("Simulation time limit reached - stopping");
        m_running = false;
        m_connectionActive = false;
        Simulator::Stop();
    }
    
    void RunSimpleExample() {
        // Create a simple 3-node topology
        NodeContainer nodes;
        nodes.Create(3);
        
        // Create point-to-point links
        PointToPointHelper p2p;
        p2p.SetDeviceAttribute("DataRate", StringValue("1Mbps"));
        p2p.SetChannelAttribute("Delay", StringValue("10ms"));
        
        // Connect nodes: 0 <-> 1 <-> 2
        NetDeviceContainer link1 = p2p.Install(nodes.Get(0), nodes.Get(1));
        NetDeviceContainer link2 = p2p.Install(nodes.Get(1), nodes.Get(2));
        
        // Install NDN stack on all nodes
        StackHelper ndnHelper;
        ndnHelper.SetDefaultRoutes(true);
        ndnHelper.InstallAll();
        
        // Choosing forwarding strategy
        StrategyChoiceHelper::InstallAll("/prefix", "/localhost/nfd/strategy/multicast");
        
        // Consumer
        AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
        consumerHelper.SetPrefix("/prefix");
        consumerHelper.SetAttribute("Frequency", StringValue("2")); // 2 interests per second
        
        ApplicationContainer consumerApp = consumerHelper.Install(nodes.Get(0));
        
        // Connect callbacks to extract data - fix callback signatures
        consumerApp.Get(0)->TraceConnectWithoutContext("ReceivedDatas", 
            MakeCallback(&NDNDataExtractor::OnDataReceived));
        consumerApp.Get(0)->TraceConnectWithoutContext("TimedOutInterests", 
            MakeCallback(&NDNDataExtractor::OnInterestTimedOut));
        
        // Producer
        AppHelper producerHelper("ns3::ndn::Producer");
        producerHelper.SetPrefix("/prefix");
        producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
        
        ApplicationContainer producerApp = producerHelper.Install(nodes.Get(2));
        
        // Connect callbacks
        producerApp.Get(0)->TraceConnectWithoutContext("ReceivedInterests", 
            MakeCallback(&NDNDataExtractor::OnInterestReceived));
        
        // Install global routing
        GlobalRoutingHelper ndnGlobalRoutingHelper;
        ndnGlobalRoutingHelper.InstallAll();
        
        ndnGlobalRoutingHelper.AddOrigins("/prefix", nodes.Get(2));
        GlobalRoutingHelper::CalculateRoutes();
        
        // Store nodes
        m_nodes = nodes;
        
        NS_LOG_INFO("Setup simple NDN topology with " << m_nodes.GetN() << " nodes");
        
        // Send initial simulation info to co-simulation
        std::ostringstream info;
        info << "NDN_SIM_STARTED:Nodes=" << m_nodes.GetN() 
             << ",Consumer=Node0,Producer=Node2,Duration=30s\n";
        NDNDataExtractor::SendToCoSimulation(info.str());
    }
    
    void RunGridTopologyExample() {
        NS_LOG_INFO("Setting up Grid Topology Example");
        
        // Check if topology file exists first
        std::string topoFile = "src/ndnSIM/examples/topologies/topo-grid-3x3.txt";
        std::ifstream file(topoFile);
        if (!file.good()) {
            NS_LOG_ERROR("Grid topology file not found: " << topoFile);
            NS_LOG_INFO("Falling back to simple example");
            RunSimpleExample();
            return;
        }
        file.close();
        
        // Create 3x3 grid topology
        AnnotatedTopologyReader topologyReader("", 25);
        topologyReader.SetFileName(topoFile);
        topologyReader.Read();
        
        // Install NDN stack
        StackHelper ndnHelper;
        ndnHelper.InstallAll();
        
        // Choosing forwarding strategy
        StrategyChoiceHelper::InstallAll("/prefix", "/localhost/nfd/strategy/multicast");
        
        // Install applications
        AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
        consumerHelper.SetPrefix("/prefix");
        consumerHelper.SetAttribute("Frequency", StringValue("5")); // 5 interests per second
        
        // Install consumers on leaf nodes (corner and edge nodes)
        ApplicationContainer consumerApps;
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                if (i == 0 || i == 2 || j == 0 || j == 2) { // leaf nodes
                    std::string nodeName = "Node" + std::to_string(i) + std::to_string(j);
                    Ptr<Node> node = Names::Find<Node>(nodeName);
                    if (node) {
                        ApplicationContainer app = consumerHelper.Install(node);
                        consumerApps.Add(app);
                        
                        // Connect callbacks to extract data
                        app.Get(0)->TraceConnectWithoutContext("ReceivedDatas", 
                            MakeCallback(&NDNDataExtractor::OnDataReceived));
                        app.Get(0)->TraceConnectWithoutContext("TimedOutInterests", 
                            MakeCallback(&NDNDataExtractor::OnInterestTimedOut));
                    }
                }
            }
        }
        
        // Producer on center node
        AppHelper producerHelper("ns3::ndn::Producer");
        producerHelper.SetPrefix("/prefix");
        producerHelper.SetAttribute("PayloadSize", StringValue("1024"));
        
        Ptr<Node> centerNode = Names::Find<Node>("Node11");
        if (centerNode) {
            ApplicationContainer producerApp = producerHelper.Install(centerNode);
            
            // Connect callbacks
            producerApp.Get(0)->TraceConnectWithoutContext("ReceivedInterests", 
                MakeCallback(&NDNDataExtractor::OnInterestReceived));
        }
        
        // Install global routing
        GlobalRoutingHelper ndnGlobalRoutingHelper;
        ndnGlobalRoutingHelper.InstallAll();
        
        ndnGlobalRoutingHelper.AddOrigins("/prefix", centerNode);
        GlobalRoutingHelper::CalculateRoutes();
        
        // Store all nodes
        m_nodes = NodeContainer::GetGlobal();
        
        NS_LOG_INFO("Setup Grid NDN topology with " << m_nodes.GetN() << " nodes");
        
        // Send initial simulation info to co-simulation
        std::ostringstream info;
        info << "NDN_SIM_STARTED:Topology=Grid3x3,Nodes=" << m_nodes.GetN() 
             << ",Consumers=" << consumerApps.GetN() << ",Producer=Node11,Duration=30s\n";
        NDNDataExtractor::SendToCoSimulation(info.str());
    }
    
    void CollectStatistics() {
        if (!m_connectionActive || g_shutdown) return;
        
        // Collect node statistics - simplified version
        std::ostringstream stats;
        stats << "NDN_NODE_STATS:Time=" << Simulator::Now().GetSeconds();
        
        for (uint32_t i = 0; i < m_nodes.GetN(); ++i) {
            Ptr<Node> node = m_nodes.Get(i);
            
            // Basic node information
            stats << ",Node" << i << "_Id=" << node->GetId();
            
            // Check if node has applications
            uint32_t nApps = node->GetNApplications();
            stats << ",Node" << i << "_Apps=" << nApps;
            
            // Get device information
            uint32_t nDevices = node->GetNDevices();
            stats << ",Node" << i << "_Devices=" << nDevices;
        }
        stats << "\n";
        
        std::string data = stats.str();
        ssize_t sent = send(m_socket, data.c_str(), data.length(), MSG_NOSIGNAL);
        if (sent < 0 && (errno == EPIPE || errno == ECONNRESET)) {
            m_connectionActive = false;
            return;
        }
        
        NS_LOG_INFO("Sent statistics: " << data.substr(0, 100) << "...");
        
        // Schedule next collection
        if (m_running && m_connectionActive && Simulator::Now().GetSeconds() < 30.0) {
            Simulator::Schedule(Seconds(2.0), &CoSimulationManager::CollectStatistics, this);
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
                NS_LOG_INFO("Connection closed by co-simulation platform");
                m_connectionActive = false;
                break;
            } else if (errno != EAGAIN && errno != EWOULDBLOCK) {
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
            double timeStep = std::stod(message.substr(5));
            NS_LOG_INFO("Step command for " << timeStep << " seconds");
            SendStepComplete();
            
        } else if (message.find("SHUTDOWN") == 0) {
            NS_LOG_INFO("Shutdown command received");
            m_running = false;
            m_connectionActive = false;
            Simulator::Stop();
        }
    }
    
    void SendStepComplete() {
        if (m_socket < 0 || !m_connectionActive) return;
        
        std::string response = "NDN_STEP_COMPLETE\n";
        ssize_t sent = send(m_socket, response.c_str(), response.length(), MSG_NOSIGNAL);
        if (sent < 0 && (errno == EPIPE || errno == ECONNRESET)) {
            m_connectionActive = false;
        }
        
        NS_LOG_INFO("Sent step complete notification");
    }

    int m_port;
    bool m_running;
    int m_socket;
    bool m_connectionActive;
    NodeContainer m_nodes;
    std::string m_exampleType;  // Add example type
};

int main(int argc, char *argv[]) {
    CommandLine cmd;
    int port = 9999;
    std::string example = "simple";  // Default to simple
    
    cmd.AddValue("port", "Communication port", port);
    cmd.AddValue("example", "NDN example type: simple or grid", example);
    cmd.Parse(argc, argv);
    
    // Validate example type
    if (example != "simple" && example != "grid") {
        std::cerr << "Error: Unknown example type '" << example << "'. Use 'simple' or 'grid'." << std::endl;
        return 1;
    }
    
    LogComponentEnable("CoSimulationScript", LOG_LEVEL_INFO);
    LogComponentEnable("ndn.Consumer", LOG_LEVEL_INFO);
    LogComponentEnable("ndn.Producer", LOG_LEVEL_INFO);
    
    NS_LOG_INFO("Starting NDN Co-simulation Script");
    NS_LOG_INFO("Port: " << port << ", Example: " << example);
    
    CoSimulationManager manager(port, example);
    manager.Initialize();
    manager.Run();
    
    return 0;
}