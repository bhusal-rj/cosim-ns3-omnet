/*
Implementation of the Config class
Provides default configuration values for the co-simulation
*/

#include "config.h"

namespace cosim {

Config::Config() 
    : simulationTime_(100.0), syncInterval_(0.1) {
    
    // Default NS-3 configuration
    ns3Config_.name = "NS3";
    ns3Config_.host = "localhost";
    ns3Config_.port = 9999;
    ns3Config_.stepSize = 0.1;
    ns3Config_.enabled = true;
    
    // Default OMNeT++ configuration
    omnetConfig_.name = "OMNeT++";
    omnetConfig_.host = "localhost";
    omnetConfig_.port = 9998;
    omnetConfig_.stepSize = 0.1;
    omnetConfig_.enabled = true;
}

} // namespace cosim