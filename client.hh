#ifndef CLIENT_HH
#define CLIENT_HH

#include <string>

class Client {
private:
    std::string serverIpAddress;
    int serverTcpPort;
    int tcpSocket = -1;
    int udpSocket = -1;
    int udpPort = -1;

public:
    Client(const std::string& ip, int tcpPort)
        : serverIpAddress(ip), serverTcpPort(tcpPort) {}

    ~Client() { disconnect(); }

    bool initializeUDP();
    bool connectToServer();
    int requestUDPPort();
    bool sendUDPMessage(int udpPort, const std::string& message);
    bool receiveUDPResponse();
    void disconnect();
};

#endif
