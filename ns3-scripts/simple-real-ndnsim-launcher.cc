// Simple Real NDN Simulator Launcher - V2X Integration
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/mobility-module.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <iostream>
#include <sstream>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SimpleRealNDNSimLauncher");

class SimpleRealNDNSimLauncher {
private:
    int leaderSocket_;
    bool connected_;
    NodeContainer nodes_;
    
public:
    SimpleRealNDNSimLauncher() : leaderSocket_(-1), connected_(false) {}
    
    bool connectToLeader(const std::string& address, int port) {
        leaderSocket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (leaderSocket_ < 0) return false;
        
        struct sockaddr_in serverAddr;
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        inet_pton(AF_INET, address.c_str(), &serverAddr.sin_addr);
        
        // Retry connection
        for (int i = 0; i < 10; i++) {
            if (connect(leaderSocket_, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == 0) {
                connected_ = true;
                std::cout << "✅ Connected to OMNeT++ leader" << std::endl;
                return true;
            }
            sleep(2);
        }
        return false;
    }
    
    void setupSimpleScenario() {
        // Create simple intersection topology
        nodes_.Create(10); // Simple node setup
        
        // Install NDN stack
        ns3::ndn::StackHelper ndnHelper;
        ndnHelper.InstallAll();
        
        // Setup simple V2X applications
        ns3::ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
        consumerHelper.SetPrefix("/kathmandu/intersection");
        consumerHelper.SetAttribute("Frequency", StringValue("2"));
        
        // Install on some nodes
        for (uint32_t i = 0; i < 5; i++) {
            consumerHelper.Install(nodes_.Get(i));
        }
        
        // Producer on one node
        ns3::ndn::AppHelper producerHelper("ns3::ndn::Producer");
        producerHelper.SetPrefix("/kathmandu/intersection");
        producerHelper.Install(nodes_.Get(9));
        
        // Basic mobility
        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                 "Bounds", RectangleValue(Rectangle(-200, 200, -200, 200)));
        mobility.Install(nodes_);
        
        std::cout << "✅ Scenario setup complete" << std::endl;
    }
    
    void sendMetrics() {
        if (!connected_) return;
        
        // Send simplified text-based metrics
        std::ostringstream oss;
        oss << "NDN_METRICS,"
            << Simulator::Now().GetSeconds() << ","
            << static_cast<int>(nodes_.GetN()) << ","
            << 50 << ","  // Simulated PIT size
            << static_cast<int>(Simulator::Now().GetSeconds() * 10) << "\n";
        
        std::string message = oss.str();
        send(leaderSocket_, message.c_str(), message.length(), 0);
        
        // Schedule next metrics collection
        Simulator::Schedule(Seconds(0.5), &SimpleRealNDNSimLauncher::sendMetrics, this);
    }
    
    void run(double simTime) {
        setupSimpleScenario();
        
        // Start metrics collection
        Simulator::Schedule(Seconds(1.0), &SimpleRealNDNSimLauncher::sendMetrics, this);
        
        // Run simulation
        Simulator::Stop(Seconds(simTime));
        Simulator::Run();
        Simulator::Destroy();
        
        if (leaderSocket_ >= 0) {
            close(leaderSocket_);
        }
    }
};

int main(int argc, char* argv[]) {
    CommandLine cmd;
    std::string leaderAddress = "127.0.0.1";
    int leaderPort = 9999;
    double simTime = 120.0;
    
    cmd.AddValue("leader", "Leader address", leaderAddress);
    cmd.AddValue("port", "Leader port", leaderPort);
    cmd.AddValue("time", "Simulation time", simTime);
    cmd.Parse(argc, argv);
    
    SimpleRealNDNSimLauncher launcher;
    
    std::cout << "🔗 Connecting to OMNeT++ leader at " << leaderAddress << ":" << leaderPort << std::endl;
    
    if (launcher.connectToLeader(leaderAddress, leaderPort)) {
        std::cout << "✅ Connected to leader - starting ndnSIM simulation" << std::endl;
        launcher.run(simTime);
    } else {
        std::cout << "❌ Failed to connect to leader" << std::endl;
        return 1;
    }
    
    return 0;
}
