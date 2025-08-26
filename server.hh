#ifndef SERVER_HH
#define SERVER_HH

#include <string>

enum SchedulingPolicy { FCFS, RR };

class Server{
private:
    std::string ipAddress;
    int tcpPort;
    int tcpSocket;
    int udpSocket;
    SchedulingPolicy schedulingPolicy;

public:
    Server(const std::string& ip, int port, SchedulingPolicy policy){
        ipAddress=ip;
        tcpPort=port;
        schedulingPolicy=policy;
        tcpSocket=-1;
        udpSocket=-1;
    }
    ~Server(){
        shutdown();
    }

    bool initializeTCP();
    void acceptClients();
    void handleClientTCP(int clientSocket);
    void handleUDPCommunication(int udpPort);
    void shutdown();
};


#endif