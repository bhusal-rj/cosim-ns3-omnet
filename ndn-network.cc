#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/wifi-module.h"
#include "ns3/ndnSIM-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/ndnSIM/utils/tracers/ndn-cs-tracer.hpp"

int main(int argc, char *argv[]) {
    std::cout << "NDN network simulation started." << std::endl;

    ns3::CommandLine cmd;
    cmd.Parse(argc, argv);

    // Create nodes
    ns3::NodeContainer vehicles;
    vehicles.Create(3);

    ns3::NodeContainer rsu;
    rsu.Create(1);

    ns3::NodeContainer allNodes;
    allNodes.Add(vehicles);
    allNodes.Add(rsu);

    // WiFi setup
    ns3::WifiHelper wifi;
    wifi.SetStandard(ns3::WIFI_STANDARD_80211n_5GHZ);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                "DataMode", ns3::StringValue("HtMcs7"),
                                "ControlMode", ns3::StringValue("HtMcs0"));

    ns3::YansWifiPhyHelper wifiPhy;
    wifiPhy.Set("TxPowerStart", ns3::DoubleValue(15.0));
    wifiPhy.Set("TxPowerEnd", ns3::DoubleValue(15.0));
    ns3::YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel", "Frequency", ns3::DoubleValue(5.9e9));
    wifiPhy.SetChannel(wifiChannel.Create());

    ns3::WifiMacHelper wifiMac;
    wifiMac.SetType("ns3::AdhocWifiMac", "Ssid", ns3::SsidValue(ns3::Ssid("V2X-IBSS-Network")));

    ns3::NetDeviceContainer wifiDevices = wifi.Install(wifiPhy, wifiMac, allNodes);

    // Mobility (for visualization)
    ns3::MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    ns3::Ptr<ns3::ListPositionAllocator> posAlloc = ns3::CreateObject<ns3::ListPositionAllocator>();
    posAlloc->Add(ns3::Vector(10, 20, 0)); // Vehicle 1
    posAlloc->Add(ns3::Vector(30, 20, 0)); // Vehicle 2
    posAlloc->Add(ns3::Vector(50, 20, 0)); // Vehicle 3
    posAlloc->Add(ns3::Vector(30, 50, 0)); // RSU
    mobility.SetPositionAllocator(posAlloc);
    mobility.Install(allNodes);

    // Install NDN stack
    ns3::ndn::StackHelper ndnHelper;
    // ndnHelper.InstallAll();

    //Caching in the NDN stack
    ndnHelper.setCsSize(100);
    ndnHelper.setPolicy("nfd::cs::lru");
    ndnHelper.Install(allNodes);

    ns3::ndn::CsTracer::InstallAll("cs-trace.txt", ns3::Seconds(1));

    // Routing helper
    ns3::ndn::GlobalRoutingHelper ndnGlobalRoutingHelper;
    // ndnGlobalRoutingHelper.InstallAll();
    ndnGlobalRoutingHelper.Install(allNodes);

    // RSU produces incident/accident data
    ns3::ndn::AppHelper rsuIncidentProducerHelper("ns3::ndn::Producer");
    rsuIncidentProducerHelper.SetPrefix("/rsu/incident");
    rsuIncidentProducerHelper.SetAttribute("PayloadSize", ns3::StringValue("1024"));
    rsuIncidentProducerHelper.Install(rsu);
    ndnGlobalRoutingHelper.AddOrigins("/rsu/incident", rsu.Get(0));

    // Each vehicle produces location coordinates
    for (uint32_t i = 0; i < vehicles.GetN(); ++i) {
        ns3::ndn::AppHelper vehicleLocationProducerHelper("ns3::ndn::Producer");
        std::string locationPrefix = "/vehicle/" + std::to_string(i) + "/location";
        vehicleLocationProducerHelper.SetPrefix(locationPrefix);
        vehicleLocationProducerHelper.SetAttribute("PayloadSize", ns3::StringValue("256"));
        vehicleLocationProducerHelper.Install(vehicles.Get(i));
        ndnGlobalRoutingHelper.AddOrigins(locationPrefix, vehicles.Get(i));
    }

    // Each vehicle produces incident/accident data
    for (uint32_t i = 0; i < vehicles.GetN(); ++i) {
        ns3::ndn::AppHelper vehicleIncidentProducerHelper("ns3::ndn::Producer");
        std::string incidentPrefix = "/vehicle/" + std::to_string(i) + "/incident";
        vehicleIncidentProducerHelper.SetPrefix(incidentPrefix);
        vehicleIncidentProducerHelper.SetAttribute("PayloadSize", ns3::StringValue("512"));
        vehicleIncidentProducerHelper.Install(vehicles.Get(i));
        ndnGlobalRoutingHelper.AddOrigins(incidentPrefix, vehicles.Get(i));
    }

    // Vehicles consume incident data from RSU
    ns3::ndn::AppHelper vehicleIncidentConsumerHelper("ns3::ndn::ConsumerCbr");
    vehicleIncidentConsumerHelper.SetPrefix("/rsu/incident");
    vehicleIncidentConsumerHelper.SetAttribute("Frequency", ns3::StringValue("10")); // Higher frequency
    vehicleIncidentConsumerHelper.Install(vehicles);

    // RSU consumes location data from vehicles
    for (uint32_t i = 0; i < vehicles.GetN(); ++i) {
        ns3::ndn::AppHelper rsuLocationConsumerHelper("ns3::ndn::ConsumerCbr");
        std::string locationPrefix = "/vehicle/" + std::to_string(i) + "/location";
        rsuLocationConsumerHelper.SetPrefix(locationPrefix);
        rsuLocationConsumerHelper.SetAttribute("Frequency", ns3::StringValue("5"));
        rsuLocationConsumerHelper.Install(rsu);
    }

    // RSU consumes incident data from vehicles
    for (uint32_t i = 0; i < vehicles.GetN(); ++i) {
        ns3::ndn::AppHelper rsuIncidentConsumerHelper("ns3::ndn::ConsumerCbr");
        std::string incidentPrefix = "/vehicle/" + std::to_string(i) + "/incident";
        rsuIncidentConsumerHelper.SetPrefix(incidentPrefix);
        rsuIncidentConsumerHelper.SetAttribute("Frequency", ns3::StringValue("1"));
        rsuIncidentConsumerHelper.Install(rsu);
    }

    // Calculate routes
    ns3::ndn::GlobalRoutingHelper::CalculateRoutes();

    // Enable packet tracing
    ns3::AsciiTraceHelper ascii;
    std::string traceFile = "ndn-packet-trace.txt";
    wifiPhy.EnableAsciiAll(ascii.CreateFileStream(traceFile));

    // Enable pcap tracing for detailed packet analysis
    wifiPhy.EnablePcap("ndn-network", wifiDevices);

    // NetAnim visualization
    ns3::AnimationInterface anim("ndn-network.xml");
    anim.SetConstantPosition(rsu.Get(0), 30, 50, 0);
    anim.SetConstantPosition(vehicles.Get(0), 10, 20, 0);
    anim.SetConstantPosition(vehicles.Get(1), 30, 20, 0);
    anim.SetConstantPosition(vehicles.Get(2), 50, 20, 0);
    anim.UpdateNodeDescription(rsu.Get(0), "RSU");
    anim.UpdateNodeDescription(vehicles.Get(0), "Vehicle1");
    anim.UpdateNodeDescription(vehicles.Get(1), "Vehicle2");
    anim.UpdateNodeDescription(vehicles.Get(2), "Vehicle3");
    anim.EnablePacketMetadata(true);
    anim.EnableWifiPhyCounters(ns3::Seconds(0), ns3::Seconds(20), ns3::Seconds(1));
    anim.EnableWifiMacCounters(ns3::Seconds(0), ns3::Seconds(20), ns3::Seconds(1));

    // Run simulation
    ns3::Simulator::Stop(ns3::Seconds(20.0));
    ns3::Simulator::Run();
    ns3::Simulator::Destroy();

    std::cout << "NDN network simulation completed." << std::endl;
    std::cout << "Packet trace files generated:" << std::endl;
    std::cout << "  - ASCII trace: " << traceFile << std::endl;
    std::cout << "  - PCAP files: ndn-network-*.pcap" << std::endl;
    std::cout << "  - Animation: ndn-network.xml" << std::endl;
    return 0;
}
