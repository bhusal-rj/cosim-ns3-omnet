/*
Implementation of the Message class
Provides basic functionality for message handling
*/

#include "message.h"

namespace cosim {

Message::Message(MessageType type, double timestamp) 
    : type_(type), timestamp_(timestamp) {
   
}

} 