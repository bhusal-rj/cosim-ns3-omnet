#include "Orchestrator.h"
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

// Register the module with OMNeT++
Define_Module(Orchestrator);

void Orchestrator::initialize() {
    // Get parameters from omnetpp.ini
    port = par("port");

    // Create a self-message for periodically checking the socket
    socket_check_event = new cMessage("socketCheck");

    EV << "🚀 Orchestrator initializing on port " << port << std::endl;

    // The socket server must run in a separate thread to avoid blocking
    // the main OMNeT++ simulation kernel with the blocking `accept()` call.
    server_thread = std::thread(&Orchestrator::startTcpServer, this);
    server_thread.detach(); // Allow the thread to run independently

    // Schedule the first event to start checking for client data
    scheduleAt(simTime() + 1.0, socket_check_event);
}

void Orchestrator::handleMessage(cMessage *msg) {
    // We only expect our self-message.
    if (msg == socket_check_event) {
        // Only process data if a client has successfully connected
        if (client_connected) {
            handleClientData();

            // Example of two-way communication:
            // Periodically send a command back to the ns-3 client.
            if (simTime() > 5.0 && fmod(simTime().dbl(), 10.0) < 0.5) {
                 sendCommandToClient("TYPE:NFV_COMMAND|DATA:Migrate_VNF_to_Node_B\n");
            }
        }
        // Reschedule the check for the next interval
        scheduleAt(simTime() + 0.5, socket_check_event);
    } else {
        // Should not happen in this simple model
        EV_WARN << "Received unexpected message: " << msg->getName() << std::endl;
        delete msg;
    }
}

void Orchestrator::startTcpServer() {
    struct sockaddr_in server_addr;
    int opt = 1;
    int addrlen = sizeof(server_addr);

    // 1. Create the server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        EV_ERROR << "FATAL: Socket creation failed" << std::endl;
        return;
    }

    // 2. Set socket options to allow reusing the address
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        EV_ERROR << "FATAL: setsockopt failed" << std::endl;
        return;
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Listen on any network interface
    server_addr.sin_port = htons(port);

    // 3. Bind the socket to the specified port
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        EV_ERROR << "FATAL: Bind failed on port " << port << std::endl;
        return;
    }

    // 4. Listen for incoming connections
    if (listen(server_fd, 1) < 0) {
        EV_ERROR << "FATAL: Listen failed" << std::endl;
        return;
    }

    EV << "👂 Server listening... Waiting for ns-3 client to connect." << std::endl;

    // 5. Accept a connection. This is a BLOCKING call.
    // It will pause this thread until a client connects.
    if ((client_socket = accept(server_fd, (struct sockaddr *)&server_addr, (socklen_t*)&addrlen)) < 0) {
        EV_ERROR << "FATAL: Accept failed" << std::endl;
        return;
    }

    // Once accept() returns, a client is connected.
    EV << "✅ ns-3 client connected!" << std::endl;
    client_connected = true;
}

void Orchestrator::handleClientData() {
    char buffer[4096];
    // Use MSG_DONTWAIT for a non-blocking receive call.
    ssize_t bytes_received = ::recv(client_socket, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);

    if (bytes_received > 0) {
        // Message received successfully
        buffer[bytes_received] = '\0'; // Null-terminate the string
        EV << "📥 Received from ns-3: " << buffer;
    } else if (bytes_received == 0) {
        // Client disconnected gracefully
        EV << "ℹ️ ns-3 client disconnected." << std::endl;
        client_connected = false;
        close(client_socket);
        client_socket = -1;
    } else {
        // No data waiting (EWOULDBLOCK) or an error occurred.
        // This is normal for a non-blocking socket, so we don't log an error.
    }
}

void Orchestrator::sendCommandToClient(const std::string& command) {
    if (!client_connected) return;

    EV << "📤 Sending to ns-3: " << command;
    // Use ::send to call the global POSIX socket function, not the OMNeT++ one.
    ::send(client_socket, command.c_str(), command.length(), 0);
}

void Orchestrator::finish() {
    EV << "🏁 Simulation finished. Closing sockets." << std::endl;

    // Clean up the scheduled event
    cancelAndDelete(socket_check_event);

    // Close sockets
    if (client_socket >= 0) {
        close(client_socket);
    }
    if (server_fd >= 0) {
        close(server_fd);
    }
}
