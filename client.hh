#ifndef CLIENT_HH
#define CLIENT_HH

#include "message.hh"
#include <string>

class Client {
public:
    Client(const std::string& ip,int tcpPort,int sizeKB,const std::string& proto,int num):
        serverIpAddress(ip),
        serverTcpPort(tcpPort),
        messageSizeKB(sizeKB),
        protocol(proto),
        numMessages(num)
    {}

    bool transferAllMessages();

private:
    std::string serverIpAddress;
    // The TCP port number used to connect to the server.
    int serverTcpPort;
    int messageSizeKB;
    std::string protocol;
    int numMessages;
};


#endif