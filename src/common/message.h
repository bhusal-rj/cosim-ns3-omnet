/*
Defines the basic message type for communication
Creates a base Message class that other messages will inherit from
Uses namespace cosim to avoid conflicts with other libraries
*/

#ifndef MESSAGE_H
#define MESSAGE_H

#include <string>
#include <vector>

namespace cosim {

enum class MessageType {
    SYNC_REQUEST,
    SYNC_RESPONSE,
    DATA_EXCHANGE,
    VEHICLE_UPDATE     // New message type
};

struct VehicleInfo {
    std::string id;
    double x, y, z;          // Position
    double vx, vy, vz;       // Velocity
    double speed;
    double heading;
    double timestamp;
};

class Message {
public:
    Message(MessageType type, double timestamp);
    virtual ~Message() = default;
    
    MessageType getType() const { return type_; }
    double getTimestamp() const { return timestamp_; }

protected:
    MessageType type_;
    double timestamp_;
};

class VehicleMessage : public Message {
public:
    VehicleMessage(double timestamp);
    
    void addVehicle(const VehicleInfo& vehicle);
    const std::vector<VehicleInfo>& getVehicles() const { return vehicles_; }
    size_t getVehicleCount() const { return vehicles_.size(); }

private:
    std::vector<VehicleInfo> vehicles_;
};

} // namespace cosim

#endif // MESSAGE_H