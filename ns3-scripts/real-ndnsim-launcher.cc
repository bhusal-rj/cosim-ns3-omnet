// Create ns3-scripts/real-ndnsim-launcher.cc
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/mobility-module.h"

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <jsoncpp/json/json.h>

using namespace ns3;
using namespace ns3::ndn;

class RealNDNSimLauncher {
private:
    int leaderSocket_;
    bool connected_;
    NodeContainer nodes_;
    
public:
    RealNDNSimLauncher() : leaderSocket_(-1), connected_(false) {}
    
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
                NS_LOG_INFO("Connected to OMNeT++ leader");
                return true;
            }
            sleep(2);
        }
        return false;
    }
    
    void setupKathmanduScenario() {
        // Create intersection topology
        nodes_.Create(15); // 4 RSUs + 11 vehicles
        
        // Install NDN stack
        StackHelper ndnHelper;
        ndnHelper.InstallAll();
        
        // Setup V2X applications
        AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
        consumerHelper.SetPrefix("/kathmandu/intersection");
        consumerHelper.SetAttribute("Frequency", StringValue("2"));
        
        // Install on vehicle nodes
        for (uint32_t i = 4; i < nodes_.GetN(); i++) {
            consumerHelper.Install(nodes_.Get(i));
        }
        
        // Producer on RSU
        AppHelper producerHelper("ns3::ndn::Producer");
        producerHelper.SetPrefix("/kathmandu/intersection");
        producerHelper.Install(nodes_.Get(0)); // Central RSU
        
        // Setup mobility for vehicles
        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                 "Bounds", RectangleValue(Rectangle(-500, 500, -500, 500)));
        mobility.Install(NodeContainer(nodes_.Begin() + 4, nodes_.End()));
        
        // Fixed positions for RSUs
        Ptr<ListPositionAllocator> rsuPositions = CreateObject<ListPositionAllocator>();
        rsuPositions->Add(Vector(0, 0, 0));      // Central
        rsuPositions->Add(Vector(0, 200, 0));    // North
        rsuPositions->Add(Vector(200, 0, 0));    // East  
        rsuPositions->Add(Vector(0, -200, 0));   // South
        
        mobility.SetPositionAllocator(rsuPositions);
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        mobility.Install(NodeContainer(nodes_.Begin(), nodes_.Begin() + 4));
    }
    
    void sendMetrics() {
        if (!connected_) return;
        
        // Collect real NDN metrics
        Json::Value metrics;
        metrics["type"] = "NDN_METRICS";
        metrics["timestamp"] = Simulator::Now().GetSeconds();
        
        // Get actual PIT size from ndnSIM
        uint32_t totalPIT = 0;
        for (auto node = nodes_.Begin(); node != nodes_.End(); ++node) {
            Ptr<nfd::Forwarder> forwarder = (*node)->GetObject<nfd::Forwarder>();
            if (forwarder) {
                totalPIT += forwarder->getPit().size();
            }
        }
        
        metrics["pit_size"] = totalPIT;
        metrics["node_count"] = static_cast<int>(nodes_.GetN());
        
        Json::StreamWriterBuilder builder;
        std::string message = Json::writeString(builder, metrics) + "\n";
        
        send(leaderSocket_, message.c_str(), message.length(), 0);
        
        // Schedule next metrics collection
        Simulator::Schedule(Seconds(0.5), &RealNDNSimLauncher::sendMetrics, this);
    }
    
    void run(double simTime) {
        setupKathmanduScenario();
        
        // Start metrics collection
        Simulator::Schedule(Seconds(1.0), &RealNDNSimLauncher::sendMetrics, this);
        
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
    
    RealNDNSimLauncher launcher;
    
    if (launcher.connectToLeader(leaderAddress, leaderPort)) {
        NS_LOG_INFO("Starting real ndnSIM co-simulation");
        launcher.run(simTime);
    } else {
        NS_LOG_ERROR("Failed to connect to leader");
        return 1;
    }
    
    return 0;
}