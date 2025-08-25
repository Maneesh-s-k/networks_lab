#ifndef CLIENT_HH
#define CLIENT_HH

#include <string>
#include <iostream>
#include <vector>


class Client {

    private:
        std::string serverIpAddress;
        int serverTcpPort;
        int tcpSocket;
        int udpSocket;

    public:
        Client(const std::string& ip,int tcpPort){
            serverIpAddress=ip;
            serverTcpPort=tcpPort;
            tcpSocket=-1;
            udpSocket=-1;
       }

        ~Client(){
            disconnect();
        }

        bool connectToServer();
        int requestUDPPort();
        bool sendUDPMessage(int udpPort,const std::string& message);
        bool receiveUDPResponse();
        void disconnect();
};

#endif
