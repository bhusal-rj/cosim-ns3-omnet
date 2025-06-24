/*
Implementation of the Message class
Provides basic functionality for message handling
*/

#include "message.h"

namespace cosim {

Message::Message(MessageType type, double timestamp) 
    : type_(type), timestamp_(timestamp) {
    // Constructor implementation
}

VehicleMessage::VehicleMessage(double timestamp)
    : Message(MessageType::VEHICLE_UPDATE, timestamp) {
    // Constructor for vehicle messages
}

void VehicleMessage::addVehicle(const VehicleInfo& vehicle) {
    vehicles_.push_back(vehicle);
}

} // namespace cosim