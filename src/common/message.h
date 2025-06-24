/*
Defines the basic message type for communication
Creates a base Message class that other messages will inherit from
Uses namespace cosim to avoid conflicts with other librariess
*/

#ifndef MESSAGE_H
#define MESSAGE_H

#include <string>

namespace cosim {

enum class MessageType {
    SYNC_REQUEST,
    SYNC_RESPONSE,
    DATA_EXCHANGE
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

} 

#endif 