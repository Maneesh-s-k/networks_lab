#include "message.hh"
#include "client.hh"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdexcept>
#include <vector>
#include <sstream>

using namespace std;

bool Client::transferAllMessages() {
    pid_t clientPid=getpid();
    
    cout<<"Client PID "<<clientPid<<" starting "<<numMessages
        <<" messages of "<<messageSizeKB<<"KB each via "<<protocol<<endl;

    for(int i=0;i<numMessages;++i) {
        // Create negotiation socket for each message
        int negotiationSocket=socket(AF_INET,SOCK_STREAM,0);
        if(negotiationSocket<0) {
            cerr<<"Error: Could not create negotiation socket.\n";
            return false;
        }

        // Connect to server for negotiation
        sockaddr_in serverAddr{};
        serverAddr.sin_family=AF_INET;
        serverAddr.sin_port=htons(serverTcpPort);
        inet_pton(AF_INET,serverIpAddress.c_str(),&serverAddr.sin_addr);

        if(::connect(negotiationSocket,(struct sockaddr*)&serverAddr,sizeof(serverAddr))<0) {
            cerr<<"Error: TCP negotiation connection to server failed.\n";
            close(negotiationSocket);
            return false;
        }

        // Send negotiation request
        stringstream ss;
        ss<<protocol<<" "<<messageSizeKB<<" "<<clientPid;
        Message request(1,ss.str());

        vector<char> serializedRequest=request.serialize();
        if(send(negotiationSocket,serializedRequest.data(),serializedRequest.size(),0)<0) {
            cerr<<"Error: Failed to send negotiation request.\n";
            close(negotiationSocket);
            return false;
        }

        // Receive negotiation response
        char buffer[1024];
        int bytesRead=recv(negotiationSocket,buffer,sizeof(buffer),0);
        if(bytesRead<=0) {
            cerr<<"Error: Did not receive negotiation response from server.\n";
            close(negotiationSocket);
            return false;
        }

        // Parse response to get data port
        int dataPort=0;
        try {
            Message response=Message::deserialize(buffer,bytesRead);
            if(response.messageType!=2) {
                cerr<<"Error: Unexpected message type in response: "<<response.messageType<<"\n";
                close(negotiationSocket);
                return false;
            }
            dataPort=stoi(response.messageContent);
        } catch(const exception& e) {
            cerr<<"Error: Invalid negotiation response: "<<e.what()<<"\n";
            close(negotiationSocket);
            return false;
        }

        close(negotiationSocket);

        // Prepare data to send
        string data(messageSizeKB*1024,'A');
        sockaddr_in dataServerAddr{};
        dataServerAddr.sin_family=AF_INET;
        dataServerAddr.sin_port=htons(dataPort);
        inet_pton(AF_INET,serverIpAddress.c_str(),&dataServerAddr.sin_addr);

        // Send data via TCP or UDP
        if(protocol=="tcp") {
            int dataSocket=socket(AF_INET,SOCK_STREAM,0);
            if(dataSocket<0) {
                cerr<<"Error: creating data TCP socket\n";
                return false;
            }

            if(::connect(dataSocket,(struct sockaddr*)&dataServerAddr,sizeof(dataServerAddr))<0) {
                perror("TCP data connect failed");
                cerr<<"Error: TCP data transfer connection failed on port "<<dataPort<<"\n";
                close(dataSocket);
                return false;
            }

            // Send all data
            size_t totalSent=0;
            while(totalSent<data.size()) {
                ssize_t sent=send(dataSocket,data.data()+totalSent,data.size()-totalSent,0);
                if(sent<=0) {
                    perror("TCP send failed");
                    close(dataSocket);
                    return false;
                }
                totalSent+=sent;
            }

            // Receive final response
            recv(dataSocket,buffer,sizeof(buffer),0);
            close(dataSocket);

        } else if(protocol=="udp") {
            int dataSocket=socket(AF_INET,SOCK_DGRAM,0);
            if(dataSocket<0) {
                cerr<<"Error: creating data UDP socket\n";
                return false;
            }

            // Set larger send buffer for UDP
            int bufferSize=messageSizeKB*1024+2048;
            setsockopt(dataSocket,SOL_SOCKET,SO_SNDBUF,&bufferSize,sizeof(bufferSize));

            // Send data
            if(sendto(dataSocket,data.data(),data.size(),0,
                      (struct sockaddr*)&dataServerAddr,sizeof(dataServerAddr))<0) {
                perror("UDP sendto failed");
                cerr<<"Error: sending UDP data to port "<<dataPort<<"\n";
                close(dataSocket);
                return false;
            }

            // Receive final response
            recvfrom(dataSocket,buffer,sizeof(buffer),0,nullptr,nullptr);
            close(dataSocket);
        }

        cout<<"Message "<<(i+1)<<"/"<<numMessages
            <<" sent successfully on port "<<dataPort<<"\n";
    }

    cout<<"Client PID "<<clientPid<<" completed all "<<numMessages<<" messages.\n";
    return true;
}

int main(int argc,char* argv[]) {
    if(argc!=6) {
        cerr<<"Usage: "<<argv[0]<<" <Server IP> <Server Port> <Mode (tcp/udp)> <Message Size KB> <Num Messages>\n";
        return 1;
    }

    string serverIp=argv[1];
    int port=atoi(argv[2]);
    string protocol=argv[3];
    int messageSize=atoi(argv[4]);
    int numMessages=atoi(argv[5]);

    if(protocol!="tcp"&&protocol!="udp") {
        cerr<<"Invalid protocol. Use 'tcp' or 'udp'.\n";
        return 1;
    }

    if(protocol=="udp"&&(messageSize<1||messageSize>32)) {
        cerr<<"UDP mode only supports message sizes from 1 to 32 KB.\n";
        return 1;
    }

    Client client(serverIp,port,messageSize,protocol,numMessages);
    if(!client.transferAllMessages()) {
        cerr<<"Transfer failed.\n";
        return 1;
    }

    return 0;
}
