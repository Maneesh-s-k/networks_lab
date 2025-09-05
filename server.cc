#include "message.hh"
#include "server.hh"
#include <iostream>
#include <iomanip>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdlib>
#include <stdexcept>
#include <vector>
#include <chrono>
#include <arpa/inet.h>
#include <sstream>
#include <algorithm>

using namespace std;


void Server::scheduler() {
    while(isRunning) {
        ClientRequest clientReq;
        bool hasRequest=false;
        
        {
            unique_lock<mutex> lock(queueMutex);
            
            if(schedulingPolicy==FCFS) {
                // FCFS: Serve ALL messages from first client before moving to next
                cv.wait(lock,[this]{ 
                    return (!fcfsClientOrder.empty()&&!fcfsClientQueues[fcfsClientOrder.front()].empty()) 
                           ||!isRunning; 
                });
                
                if(!isRunning) break;
                
                if(!fcfsClientOrder.empty()) {
                    int clientPid=fcfsClientOrder.front();
                    if(!fcfsClientQueues[clientPid].empty()) {
                        clientReq=move(fcfsClientQueues[clientPid].front());
                        fcfsClientQueues[clientPid].pop();
                        
                        // If this client has no more requests, remove from order
                        if(fcfsClientQueues[clientPid].empty()) {
                            fcfsClientOrder.pop();
                            fcfsClientQueues.erase(clientPid);
                        }
                        
                        hasRequest=true;
                        cout<<"[FCFS] Serving PID "<<clientReq.clientPid 
                             <<" ("<<clientReq.protocol<<" "<<clientReq.sizeKB<<"KB)"
                             <<" - "<<fcfsClientQueues[clientPid].size()<<" requests remaining\n";
                    }
                }
                
            } else { // RR
                // RR: One message per client, then rotate
                cv.wait(lock,[this]{ return !rrActiveClients.empty()||!isRunning; });
                
                if(!isRunning) break;
                
                if(!rrActiveClients.empty()) {
                    int clientPid=rrActiveClients.front();
                    rrActiveClients.pop_front();
                    
                    if(!rrClientQueues[clientPid].empty()) {
                        clientReq=move(rrClientQueues[clientPid].front());
                        rrClientQueues[clientPid].pop();
                        
                        // If client has more requests, put back at end of round-robin
                        if(!rrClientQueues[clientPid].empty()) {
                            rrActiveClients.push_back(clientPid);
                        } else {
                            rrClientQueues.erase(clientPid);
                        }
                        
                        hasRequest=true;
                        cout<<"[RR] Turn for PID "<<clientReq.clientPid 
                             <<" ("<<clientReq.protocol<<" "<<clientReq.sizeKB<<"KB)"
                             <<" - "<<rrClientQueues[clientPid].size()<<" requests remaining\n";
                    }
                }
            }
        }
        
        // Handle request synchronously to maintain scheduling order
        if(hasRequest) {
            handleNegotiation(clientReq);
        }
    }
}

void Server::handleNegotiation(const ClientRequest& clientReq) {
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET,&(clientReq.clientAddr.sin_addr),client_ip,INET_ADDRSTRLEN);

    try {
        int dataSocket=(clientReq.protocol=="tcp")? 
            socket(AF_INET,SOCK_STREAM,0):socket(AF_INET,SOCK_DGRAM,0);
        if(dataSocket<0) {
            close(clientReq.clientSocket);
            return;
        }

        sockaddr_in dataAddr{};
        dataAddr.sin_family=AF_INET;
        dataAddr.sin_addr.s_addr=INADDR_ANY;
        dataAddr.sin_port=0;

        if(::bind(dataSocket,(struct sockaddr*)&dataAddr,sizeof(dataAddr))<0) {
            close(clientReq.clientSocket);
            close(dataSocket);
            return;
        }
        
        if(clientReq.protocol=="tcp"&&::listen(dataSocket,1)<0) {
            close(clientReq.clientSocket);
            close(dataSocket);
            return;
        }
        
        socklen_t len=sizeof(dataAddr);
        getsockname(dataSocket,(struct sockaddr*)&dataAddr,&len);
        int dataPort=ntohs(dataAddr.sin_port);

        {
            lock_guard<mutex> lock(logMutex);
            cout<<"Client (PID "<<clientReq.clientPid<<") negotiated port "<<dataPort 
                 <<" for "<<clientReq.sizeKB<<"KB "<<clientReq.protocol<<" transfer.\n";
        }
        
        Message resp(2,to_string(dataPort));
        vector<char> respSer=resp.serialize();
        send(clientReq.clientSocket,respSer.data(),respSer.size(),0);
        close(clientReq.clientSocket);

        // Handle data transfer synchronously
        handleDataTransfer(clientReq.protocol,clientReq.sizeKB,dataSocket, 
                         string(client_ip),clientReq.clientPid);

    } catch(const exception& e) {
        cerr<<"Error during negotiation: "<<e.what()<<"\n";
        close(clientReq.clientSocket);
    }
}

void Server::handleDataTransfer(const string& protocol,int sizeKB,int dataSocket,
                               const string& clientIp,int clientPid) {
    const size_t totalBytesToReceive=sizeKB*1024;
    size_t bytesReceived=0;
    auto startTime=chrono::steady_clock::now();
    
    sockaddr_in addr;
    socklen_t len=sizeof(addr);
    getsockname(dataSocket,(struct sockaddr*)&addr,&len);
    int port=ntohs(addr.sin_port);

    if(protocol=="tcp") {
        int acceptedSocket=::accept(dataSocket,nullptr,nullptr);
        close(dataSocket); 
        if(acceptedSocket<0) {
            cerr<<"Port "<<port<<": Error accepting TCP data connection.\n";
            return;
        }
        char buffer[4096];
        while(bytesReceived<totalBytesToReceive) {
            int n=recv(acceptedSocket,buffer,sizeof(buffer),0);
            if(n<=0) break;
            bytesReceived+=n;
        }
        Message finalResp(4,"TCP transfer complete");
        vector<char> finalRespSer=finalResp.serialize();
        send(acceptedSocket,finalRespSer.data(),finalRespSer.size(),0);
        close(acceptedSocket);

    } else if(protocol=="udp") {
        char buffer[32*1024]; 
        sockaddr_in clientDataAddr{};
        socklen_t clientLen=sizeof(clientDataAddr);
        
        // Loop to handle potential fragmentation
        while(bytesReceived<totalBytesToReceive) {
            int n=recvfrom(dataSocket,buffer,sizeof(buffer),0, 
                           (struct sockaddr*)&clientDataAddr,&clientLen);
            if(n<=0) break;
            bytesReceived+=n;
            if(bytesReceived>=totalBytesToReceive) break;
        }
        
        Message finalResp(4,"UDP transfer complete");
        vector<char> finalRespSer=finalResp.serialize();
        sendto(dataSocket,finalRespSer.data(),finalRespSer.size(),0,
               (struct sockaddr*)&clientDataAddr,clientLen);
        close(dataSocket);
    }

    auto endTime=chrono::steady_clock::now();
    auto duration_us=chrono::duration_cast<chrono::microseconds>(endTime-startTime);
    long long microseconds=duration_us.count();

    double throughputKbps=0;
    if(microseconds>0) {
        throughputKbps=(static_cast<double>(bytesReceived)*8.0*1000000.0)/ 
                        (static_cast<double>(microseconds)*1024.0);
    }

    {
        lock_guard<mutex> lock(logMutex);
        cout<<fixed<<setprecision(2);
        cout<<"Client (PID "<<clientPid<<") on Port "<<port<<" ("<<protocol<<"): "
             <<static_cast<double>(bytesReceived)/1024.0<<" KB in "
             <<microseconds<<"us -> "<<throughputKbps<<" Kbps.\n";
        cout<<"Client (PID "<<clientPid<<") on Port "<<port<<": Disconnected.\n";

        if(csvLogFile.is_open()) {
            string policyStr=(this->schedulingPolicy==FCFS)?"FCFS":"RR";
            csvLogFile<<policyStr<<","
                      <<protocol<<","
                      <<sizeKB<<","
                      <<microseconds<<","
                      <<throughputKbps<<"\n";
        }
    }
}

bool Server::initialize(){
    tcpSocket=socket(AF_INET,SOCK_STREAM,0);
    if(tcpSocket<0){
        cerr<<"Error creating main TCP socket\n";
        return false;
    }
    int opt=1;
    setsockopt(tcpSocket,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    sockaddr_in serverAddr{};
    serverAddr.sin_family=AF_INET;
    serverAddr.sin_addr.s_addr=INADDR_ANY;
    serverAddr.sin_port=htons(tcpPort);
    if(::bind(tcpSocket,(struct sockaddr*)&serverAddr,sizeof(serverAddr))<0){
        cerr<<"Error binding main TCP socket\n";
        close(tcpSocket);
        return false;
    }
    return true;
}

void Server::start(){
    if(::listen(tcpSocket,10)<0){
        cerr<<"Error listening on main TCP socket\n";
        return;
    }
    
    string policyName=(schedulingPolicy==FCFS)?"FCFS":"RR";
    cout<<"Server listening on port "<<tcpPort<<" with "<<policyName<<" scheduling...\n";

    isRunning=true;
    schedulerThread=thread(&Server::scheduler,this);

    while(isRunning){
        sockaddr_in clientAddr{};
        socklen_t clientLen=sizeof(clientAddr);
        int clientSocket=::accept(tcpSocket,(struct sockaddr*)&clientAddr,&clientLen);
        if(clientSocket<0){
            if(isRunning) cerr<<"Error accepting client connection\n";
            continue;
        }
        
        // Parse the request to get PID and other info
        try{
            char recv_buf[1024];
            int bytesRead=recv(clientSocket,recv_buf,sizeof(recv_buf),0);
            if(bytesRead<=0) {
                close(clientSocket);
                continue;
            }

            Message req=Message::deserialize(recv_buf,bytesRead);
            if(req.messageType!=1) {
                close(clientSocket);
                continue;
            }

            stringstream ss(req.messageContent);
            string protocol;
            int sizeKB;
            int clientPid;
            ss>>protocol>>sizeKB>>clientPid;

            ClientRequest clientReq={clientSocket,clientAddr,clientPid,protocol,sizeKB};
            
            {
                lock_guard<mutex> lock(queueMutex);
                if(schedulingPolicy==FCFS) {
                    // Add to client's queue
                    fcfsClientQueues[clientPid].push(clientReq);
                    
                    // If this is a new client, add to order
                    bool isNewClient=(fcfsClientQueues[clientPid].size()==1);
                    if(isNewClient) {
                        // Check if client is already in order
                        queue<int> tempOrder=fcfsClientOrder;
                        bool found=false;
                        while(!tempOrder.empty()) {
                            if(tempOrder.front()==clientPid) {
                                found=true;
                                break;
                            }
                            tempOrder.pop();
                        }
                        if(!found) {
                            fcfsClientOrder.push(clientPid);
                        }
                    }
                } else { // RR
                    // Add to client's queue
                    rrClientQueues[clientPid].push(clientReq);
                    
                    // If this is a new client, add to round-robin
                    bool isNewClient=(rrClientQueues[clientPid].size()==1);
                    if(isNewClient) {
                        // Check if client is already in RR list
                        bool found=find(rrActiveClients.begin(),rrActiveClients.end(),clientPid) 
                                   !=rrActiveClients.end();
                        if(!found) {
                            rrActiveClients.push_back(clientPid);
                        }
                    }
                }
            }
            cv.notify_one();
            
        }catch(const exception& e){
            cerr<<"Error parsing client request: "<<e.what()<<"\n";
            close(clientSocket);
        }
    }
}

void Server::shutdown(){
    isRunning=false;
    cv.notify_all();
    ::shutdown(tcpSocket,SHUT_RDWR);
    if(tcpSocket>=0) {
        close(tcpSocket);
        tcpSocket=-1;
    }
    if(schedulerThread.joinable()){
        schedulerThread.join();
    }
    cout<<"Server shut down.\n";
}


int main(int argc,char* argv[]){
    if(argc<3||argc>4) {
        cerr<<"Usage: "<<argv[0]<<" <ServerPort> <SchedulingPolicy (1-FCFS, 2-RR)> [CsvLogFile]\n";
        return 1;
    }
    int port=atoi(argv[1]);
    int schednum=atoi(argv[2]);
    SchedulingPolicy policy=(schednum==2)?RR:FCFS;

    optional<string> logFileName;
    if(argc==4) {
        logFileName=argv[3];
    }

    Server server(port,policy,logFileName);
    if(!server.initialize()) {
        cerr<<"Failed to initialize server\n";
        return 1;
    }

    server.start();
    return 0;
}
