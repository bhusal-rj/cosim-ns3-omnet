/*
Main entry point for the co-simulation platform
Simple test to verify compilation
*/

#include "message.h"
#include <iostream>

int main() {
    std::cout << "Co-simulation platform starting..." << std::endl;
    
    // Test creating a basic message
    cosim::Message msg(cosim::MessageType::SYNC_REQUEST, 1.0);
    std::cout << "Message created with timestamp: " << msg.getTimestamp() << std::endl;
    
    return 0;
}