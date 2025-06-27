#ifndef __V2X_COSIMULATION_ORCHESTRATOR_H_
#define __V2X_COSIMULATION_ORCHESTRATOR_H_

#include <omnetpp.h>
#include <sys/socket.h>
#include <thread>

using namespace omnetpp;

class Orchestrator : public cSimpleModule {
  private:
    // Socket communication state
    int port;
    int server_fd = -1;
    int client_socket = -1;
    std::thread server_thread;
    bool client_connected = false;

    // OMNeT++ message to periodically check for client data
    cMessage *socket_check_event = nullptr;

  protected:
    virtual void initialize() override;
    virtual void handleMessage(cMessage *msg) override;
    virtual void finish() override;

    // Server logic methods
    void startTcpServer();
    void handleClientData();
    void sendCommandToClient(const std::string& command);
};

#endif // __V2X_COSIMULATION_ORCHESTRATOR_H_
