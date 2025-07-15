#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/mobility-module.h"
#include "ns3/ndnSIM-module.h"
#include <json/json.h>
#include <iostream>
#include <thread>
#include <atomic>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

using namespace ns3;

class RealNDNSimLauncher {
private:
    int leaderSocket_;
    bool connected_;
    NodeContainer nodes_;
    std::thread recvThread_;
    std::atomic<bool> stopRecv_{false};

    void receiveLoop() {
        char buffer[1024];
        while (!stopRecv_) {
            ssize_t n = recv(leaderSocket_, buffer, sizeof(buffer) - 1, 0);
            if (n > 0) {
                buffer[n] = '\0';
                std::string msg(buffer);
                std::cout << "📥 Received from OMNeT++: " << msg << std::endl;
                // TODO: Parse/process msg as needed
            } else if (n == 0) {
                std::cout << "🔌 OMNeT++ leader closed connection." << std::endl;
                break;
            } else if (errno != EINTR && errno != EAGAIN) {
                std::cerr << "❌ Socket recv error: " << strerror(errno) << std::endl;
                break;
            }
        }
    }

public:
    RealNDNSimLauncher() : leaderSocket_(-1), connected_(false) {}

    bool connectToLeader(const std::string& address, int port) {
        leaderSocket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (leaderSocket_ < 0) {
            std::cerr << "❌ Failed to create socket" << std::endl;
            return false;
        }
        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_port = htons(port);
        inet_pton(AF_INET, address.c_str(), &serverAddr.sin_addr);

        for (int i = 0; i < 10; i++) {
            if (connect(leaderSocket_, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == 0) {
                connected_ = true;
                stopRecv_ = false;
                recvThread_ = std::thread(&RealNDNSimLauncher::receiveLoop, this);
                return true;
            }
            sleep(2);
        }
        close(leaderSocket_);
        return false;
    }

    void setupKathmanduScenario() {
        nodes_.Create(15); // 4 RSUs + 11 vehicles

        ns3::ndn::StackHelper ndnHelper;
        ndnHelper.InstallAll();

        ns3::ndn::AppHelper consumerHelper("ns3::ndn::ConsumerCbr");
        consumerHelper.SetPrefix("/kathmandu/intersection");
        consumerHelper.SetAttribute("Frequency", ns3::StringValue("2"));

        for (uint32_t i = 4; i < nodes_.GetN(); i++) {
            consumerHelper.Install(nodes_.Get(i));
        }

        ns3::ndn::AppHelper producerHelper("ns3::ndn::Producer");
        producerHelper.SetPrefix("/kathmandu/intersection");
        producerHelper.Install(nodes_.Get(0)); // Central RSU

        MobilityHelper mobility;
        mobility.SetMobilityModel("ns3::RandomWalk2dMobilityModel",
                                 "Bounds", RectangleValue(Rectangle(-500, 500, -500, 500)));

        for (uint32_t i = 4; i < nodes_.GetN(); i++) {
            mobility.Install(nodes_.Get(i));
        }

        Ptr<ListPositionAllocator> rsuPositions = CreateObject<ListPositionAllocator>();
        rsuPositions->Add(Vector(0, 0, 0));      // Central
        rsuPositions->Add(Vector(0, 200, 0));    // North
        rsuPositions->Add(Vector(200, 0, 0));    // East  
        rsuPositions->Add(Vector(0, -200, 0));   // South

        mobility.SetPositionAllocator(rsuPositions);
        mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
        for (uint32_t i = 0; i < 4; i++) {
            mobility.Install(nodes_.Get(i));
        }
    }

    void sendMetrics() {
        if (!connected_) return;
        Json::Value metrics;
        metrics["active_faces"] = 20; // Simulated value
        metrics["interests_sent"] = static_cast<int>(Simulator::Now().GetSeconds() * 10);

        Json::StreamWriterBuilder builder;
        std::string message = Json::writeString(builder, metrics) + "\n";
        send(leaderSocket_, message.c_str(), message.length(), 0);

        Simulator::Schedule(Seconds(0.5), &RealNDNSimLauncher::sendMetrics, this);
    }

    void run(double simTime) {
        setupKathmanduScenario();
        Simulator::Schedule(Seconds(1.0), &RealNDNSimLauncher::sendMetrics, this);
        Simulator::Stop(Seconds(simTime));
        Simulator::Run();
        Simulator::Destroy();

        stopRecv_ = true;
        if (recvThread_.joinable()) recvThread_.join();

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

    std::cout << "🔗 Connecting to OMNeT++ leader at " << leaderAddress << ":" << leaderPort << std::endl;

    if (launcher.connectToLeader(leaderAddress, leaderPort)) {
        std::cout << "✅ Connected to leader - starting ndnSIM simulation" << std::endl;
        launcher.run(simTime);
    } else {
        std::cout << "❌ Failed to connect to leader" << std::endl;
        return 1;
    }
}