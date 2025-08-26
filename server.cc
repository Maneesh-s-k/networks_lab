#include "message.hh"
#include "server.hh"
#include <iostream>
#include <cstring>      // for memset
#include <sys/socket.h> // socket functions -(bind, listen, accept)
#include <netinet/in.h> // sockaddr_in
#include <unistd.h>     // close
#include <cstdlib>      // for atoi
#include <thread>
#include <stdexcept>
#include <vector>

using namespace std;

int getFreeUdpPort() {
    // Dynamic and private ports ranging from 49152 to 65535
    return 49152+rand()%(65535-49152+1);
}


bool Server::initializeTCP(){

    //create tcp connection
    tcpSocket=socket(AF_INET, SOCK_STREAM, 0);
    if(tcpSocket<0){
        cerr<<"Error creating TCP socket\n";
        return false;
    }
    else{
        cout<<"TCP socket created successfully\n";
    }

    int opt=1;
    if(setsockopt(tcpSocket,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt))<0){
        cerr<<"setsockopt(SO_REUSEADDR) failed\n";
        close(tcpSocket);
        return false;
    }

    // Setup sockaddr_in struct for server address & port
    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family=AF_INET;
    serverAddr.sin_addr.s_addr=INADDR_ANY; // listens to all interfaces

    // bind to all interfaces, or use inet_addr(ipAddress.c_str()) to bind specific IP
    serverAddr.sin_port = htons(tcpPort);

    // Bind socket to IP and port
    if (::bind(tcpSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "Error binding TCP socket\n";
        close(tcpSocket);
        return false;
    }

    return true;
}


void Server::acceptClients(){

    if(listen(tcpSocket, 5)<0){
        cerr<<"Error listening on TCP socket\n";
        return;
    }
    else{
        cout<<"Server listening on port "<<tcpPort<<"\n";
    }

    while(true){

        sockaddr_in clientAddr;
        socklen_t clientLen=sizeof(clientAddr);
        int clientSocket=::accept(tcpSocket, (struct sockaddr*)&clientAddr, &clientLen);
        if(clientSocket<0){
            cerr<<"Error accepting client connection\n";
            continue;
        }
        else{
            cout<<"Client connected successfully\n";
        }

        // Handle client in a separate thread
        thread clientThread(&Server::handleClientTCP, this, clientSocket);
        clientThread.detach(); // Detach thread to allow independent execution
    }
}


void Server::handleClientTCP(int clientSocket){

    // Receive request for UDP port
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    int bytesRead=recv(clientSocket, buffer, sizeof(buffer), 0);
    if(bytesRead<=0){
        cerr<<"Error receiving data from client\n";
        close(clientSocket);
        return;
    }

    // Deserialize message
    Message request;
    try{
        request=Message::deserialize(buffer, bytesRead);
    }
    catch(const std::invalid_argument& e){
        cerr<<"Invalid message received: "<<e.what()<<"\n";
        close(clientSocket);
        return;
    }

    if(request.messageType!=1){ // assuming 1 is request UDP port type
        cerr<<"Unexpected message type received: "<<request.messageType<<"\n";
        close(clientSocket);
        return;
    }

    // Allocate UDP port
    int udpPort=getFreeUdpPort();
    if(udpPort==-1){
        cerr<<"Error allocating UDP port\n";
        close(clientSocket);
        return;
    }

    // Send allocated UDP port back to client
    Message response(2, to_string(udpPort)); // assuming 2 is response UDP port type
    vector<char> serializedResponse=response.serialize();
    send(clientSocket, serializedResponse.data(), serializedResponse.size(), 0);

    // Close TCP connection
    close(clientSocket);

    // Handle UDP communication on allocated port
    handleUDPCommunication(udpPort);
}


void Server::handleUDPCommunication(int udpPort){

    // Create UDP socket
    udpSocket=socket(AF_INET, SOCK_DGRAM, 0);
    if(udpSocket<0){
        cerr<<"Error creating UDP socket\n";
        return;
    }
    else{
        cout<<"UDP socket created successfully on port "<<udpPort<<"\n";
    }

    // Setup sockaddr_in struct for server address & port
    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family=AF_INET;
    serverAddr.sin_addr.s_addr=INADDR_ANY; // listens to all interfaces
    serverAddr.sin_port=htons(udpPort);

    // Bind UDP socket to the allocated port
    if(::bind(udpSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr))<0){
        cerr<<"Error binding UDP socket\n";
        close(udpSocket);
        return;
    }

    // Receive message from client
    char buffer[1024];
    memset(buffer, 0, sizeof(buffer));
    sockaddr_in clientAddr;
    socklen_t clientLen=sizeof(clientAddr);
    int bytesRead=recvfrom(udpSocket, buffer, sizeof(buffer), 0, (struct sockaddr*)&clientAddr, &clientLen);
    if(bytesRead<0){
        cerr<<"Error receiving UDP message\n";
        close(udpSocket);
        return;
    }

    // Deserialize message
    Message udpMessage;
    try{
        udpMessage=Message::deserialize(buffer, bytesRead);
    }
    catch(const std::invalid_argument& e){
        cerr<<"Invalid UDP message received: "<<e.what()<<"\n";
        close(udpSocket);
        return;
    }

    if(udpMessage.messageType!=3){ // assuming 3 is UDP message type
        cerr<<"Unexpected UDP message type received: "<<udpMessage.messageType<<"\n";
        close(udpSocket);
        return;
    }

    cout<<"Received UDP message: "<<udpMessage.messageContent<<"\n";

    // Send response back to client
    Message udpResponse(4, "Message received"); // assuming 4 is UDP response type
    vector<char> serializedResponse=udpResponse.serialize();
    sendto(udpSocket, serializedResponse.data(), serializedResponse.size(), 0, (struct sockaddr*)&clientAddr, clientLen);

    // Close UDP socket
    close(udpSocket);
}


// Shutdown server and close sockets
void Server::shutdown(){
    if(tcpSocket>=0){
        close(tcpSocket);
        tcpSocket=-1;
    }
    if(udpSocket>=0){
        close(udpSocket);
        udpSocket=-1;
    }
    cout<<"Server sockets closed\n";
}


int main(int argc,char* argv[]){
    if(argc!=2){
        cerr<<"Usage: "<<argv[0]<<" <Server Port number>\n";
        return 1;
    }

    int port=atoi(argv[1]);
    if(port<=0 || port>65535){
        cerr<<"Invalid port number\n";
        return 1;
    }

    Server server("0.0.0.0",port,FCFS);

    if(!server.initializeTCP()){
        cerr<<"Failed to initialize TCP socket\n";
        return 1;
    }

    server.acceptClients();
    server.shutdown();

    return 0;
}