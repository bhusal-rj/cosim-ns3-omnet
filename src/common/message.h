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

// NDN Metrics structure from methodology
struct NDNMetrics {
    uint32_t pitSize = 0;
    uint32_t fibEntries = 0;
    double cacheHitRatio = 0.0;
    uint64_t interestCount = 0;
    uint64_t dataCount = 0;
    double avgLatency = 0.0;
    uint32_t unsatisfiedInterests = 0;
    double timestamp = 0.0;
    
    // Additional V2X metrics
    uint32_t emergencyMessages = 0;
    uint32_t safetyMessages = 0;
    double networkUtilization = 0.0;
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