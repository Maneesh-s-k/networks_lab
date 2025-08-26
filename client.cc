#include "message.hh"
#include "client.hh"
#include <iostream>
#include <cstring>      // for memset
#include <sys/socket.h> // socket functions -(bind, listen, accept)
#include <netinet/in.h> // sockaddr_in
#include <arpa/inet.h> // inet_addr
#include <unistd.h>     // close    
#include <cstdlib>      // for atoi
#include <stdexcept>
#include <thread>
#include <vector>


using namespace std;

//initialize connection to server(TCP)
bool Client::connectToServer(){
    tcpSocket=socket(AF_INET,SOCK_STREAM,0);
    if(tcpSocket<0){
        cerr<<"Error creating TCP socket\n";
        return false;
    }
    
    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr)); 
    serverAddr.sin_family=AF_INET;  
    serverAddr.sin_port=htons(serverTcpPort);

    if(inet_pton(AF_INET,serverIpAddress.c_str(),&serverAddr.sin_addr)<=0){
        cerr<<"Invalid server IP address\n";
        close(tcpSocket);
        tcpSocket=-1;
        return false;
    }

    if(::connect(tcpSocket,(struct sockaddr*)&serverAddr,sizeof(serverAddr))<0){
        cerr<<"TCP connection to server failed\n";
        close(tcpSocket);
        tcpSocket=-1;
        return false;
    }

    cout<<"Connected to server "<<serverIpAddress<<" on port "<<serverTcpPort<<"\n";
    return true;
}


int Client::requestUDPPort(){
    //send request message(type 1) to server
    Message request(1, "Requesting UDP port"); // assuming 1 is request UDP port type
    vector<char> serializedRequest=request.serialize();

    if(send(tcpSocket, serializedRequest.data(),serializedRequest.size(),0)<0){
        cerr<<"Error sending UDP port request to server\n";
        return -1;
    }

    //receive response from server
    char buffer[1024];
    int bytesRead=recv(tcpSocket,buffer,sizeof(buffer),0);
    if(bytesRead<=0){
        cerr<<"Error receiving UDP port response from server\n";
        return -1;
    }

    try{
        Message response=Message::deserialize(buffer, bytesRead);
        if(response.messageType==2){ //messageType 2 is response UDP port type
            int udpPort=stoi(response.messageContent);
            cout<<"Received allocated UDP port from server: "<<udpPort<<"\n";
            return udpPort;
        }
        else{
            cerr<<"Unexpected message type received in UDP port response: "<<response.messageType<<"\n";
            return -1;
        }
    }
    catch(const exception& e){
        cerr<<"Invalid UDP port response received: "<<e.what()<<"\n";
        return -1;
    }
}

//send UDP message to server on allocated UDP port
bool Client::sendUDPMessage(int udpPort,const string& message){
    //create UDP socket
    udpSocket=socket(AF_INET,SOCK_DGRAM,0);
    if(udpSocket<0){
        cerr<<"Error creating UDP socket\n";
        return false;
    }

    //setup server address struct
    sockaddr_in serverAddr;
    memset(&serverAddr,0,sizeof(serverAddr));
    serverAddr.sin_family=AF_INET;
    serverAddr.sin_port=htons(udpPort);
    if(inet_pton(AF_INET, serverIpAddress.c_str(), &serverAddr.sin_addr)<=0){
        cerr<<"Invalid server IP address\n";
        close(udpSocket);
        udpSocket=-1;
        return false;
    }


    //create UDP message(type 3)
    Message udpMessage(3, message); // messageType 3 is UDP message
    vector<char> serializedMessage=udpMessage.serialize();

    ssize_t bytesSent=sendto(udpSocket, serializedMessage.data(), serializedMessage.size(), 0, (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if(bytesSent<0){
        cerr<<"Error sending UDP message to server\n";
        close(udpSocket);
        udpSocket=-1;
        return false;
    }

    cout<<"Sent UDP message to server: "<<message<<"\n";
    return true;
}

//receive UDP response from server
bool Client::receiveUDPResponse(){
    if(udpSocket<0){
        cerr<<"UDP socket not initialized\n";
        return false;
    }

    char buffer[1024];
    memset(buffer,0,sizeof(buffer));
    sockaddr_in fromAddr;
    socklen_t fromLen=sizeof(fromAddr);

    int bytesReceived=recvfrom(udpSocket,buffer,sizeof(buffer),0,(struct sockaddr*)&fromAddr,&fromLen);
    if(bytesReceived<0){
        cerr<<"Error receiving UDP response from server\n";
        return false;
    }

    try{
        Message response=Message::deserialize(buffer,bytesReceived);
        if(response.messageType==4){ // assuming 4 is UDP response type
            cout<<"Received UDP response from server: "<<response.messageContent<<"\n";
            return true;
        }
        else{
            cerr<<"Unexpected message type received in UDP response: "<<response.messageType<<"\n";
            return false;
        }
    }
    catch(const exception& e){
        cerr<<"Invalid UDP response(failed to deserialize) received: "<<e.what()<<"\n";
        return false;
    }
}


//gracefull termination of connection from server
void Client::disconnect(){
    if(tcpSocket>=0){
        close(tcpSocket);
        tcpSocket=-1;
    }
    if(udpSocket>=0){
        close(udpSocket);
        udpSocket=-1;
    }
}


int main(int argc,char* argv[]){
    if(argc!=3){
        cerr<<"Usage: "<<argv[0]<<" <Server IP Address> <Server Port number>\n";
        return 1;
    }

    string serverIp=argv[1];
    int port=atoi(argv[2]);

    if(port<=0 || port>65535){
        cerr<<"Invalid server port number\n";
        return 1;
    }

    Client client(serverIp,port);

    if(!client.connectToServer()){
        cerr<<"Failed to connect to server\n";
        return 1;
    }

    int udpPort=client.requestUDPPort();
    if(udpPort==-1){
        client.disconnect();
        return 1;
    }

    if(!client.sendUDPMessage(udpPort,"Hello from Client")){
        client.disconnect();
        return 1;
    }

    if(!client.receiveUDPResponse()){
        client.disconnect();
        return 1;
    }

    client.disconnect();
    return 0;   
}
