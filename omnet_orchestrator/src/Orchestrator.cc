#include "Orchestrator.h"
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include <json/json.h> // Include JSON library header

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
        EV << "🔍 Checking for client data... client_connected=" << client_connected << ", client_socket=" << client_socket << endl;
        
        // Only process data if a client has successfully connected
        if (client_connected) {
            handleClientData();

            // Example of two-way communication:
            // Periodically send a command back to the ns-3 client.
            if (simTime() > 5.0 && fmod(simTime().dbl(), 10.0) < 0.5) {
                 sendCommandToClient("TYPE:NFV_COMMAND|DATA:Migrate_VNF_to_Node_B\n");
            }
        } else {
            EV << "⏳ Client not yet connected, waiting..." << endl;
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
    char buffer[1024];
    ssize_t bytesRead = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytesRead > 0) {
        buffer[bytesRead] = '\0';
        std::string msg(buffer);

        // Print received data
        EV << "📥 DATA RECEIVED from ns-3 client!" << endl;
        EV << "📊 Data content: " << buffer << endl;
        std::cout << "📥 DATA RECEIVED from ns-3 client!" << std::endl;
        std::cout << "📊 Data: " << buffer << std::endl;

        // === ADD THIS BLOCK ===
        Json::Value root;
        Json::CharReaderBuilder reader;
        std::string errs;
        std::istringstream s(msg);
        if (Json::parseFromStream(reader, s, &root, &errs)) {
            if (root.isMember("request") && root["request"].asString() == "position" && root.isMember("node")) {
                int nodeId = root["node"].asInt();
                // Generate or look up new position for nodeId
                double x = 100.0 + nodeId * 10; // Example logic
                double y = 200.0 + nodeId * 5;
                double z = 0.0;

                Json::Value resp;
                resp["node"] = nodeId;
                resp["x"] = x;
                resp["y"] = y;
                resp["z"] = z;
                Json::StreamWriterBuilder builder;
                std::string response = Json::writeString(builder, resp) + "\n";
                ::send(client_socket, response.c_str(), response.length(), 0);

                EV << "📤 Sent position to ns-3: " << response << endl;
                std::cout << "📤 Sent position to ns-3: " << response << std::endl;
            }
        }
        // === END BLOCK ===

        // Optionally, send ACK for other data
        std::string ack = "ACK: Data received\n";
        ::send(client_socket, ack.c_str(), ack.length(), 0);

    } else if (bytesRead == 0) {
        EV << "ℹ️ ns-3 client disconnected." << endl;
        std::cout << "ℹ️ ns-3 client disconnected." << std::endl;
        close(client_socket);
        client_socket = -1;
        client_connected = false;
    } else {
        // bytesRead < 0 - check errno
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            EV << "❌ recv() error: " << strerror(errno) << endl;
            std::cout << "❌ recv() error: " << strerror(errno) << std::endl;
        } else {
            // No data available right now (normal for non-blocking)
            EV << "🔍 No data available (EAGAIN/EWOULDBLOCK)" << endl;
        }
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
