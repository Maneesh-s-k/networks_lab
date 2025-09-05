#ifndef SERVER_HH
#define SERVER_HH

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <netinet/in.h>
#include <fstream>
#include <optional>


enum SchedulingPolicy{FCFS,RR};


struct ClientRequest{
    int clientSocket;
    sockaddr_in clientAddr;
};


class Server{
public:
    Server(int port,SchedulingPolicy policy,std::optional<std::string> csvLogFileName)
        :tcpPort(port),
         tcpSocket(-1),
         schedulingPolicy(policy),
         isRunning(false)
    {
        if(csvLogFileName){
            csvLogFile.open(*csvLogFileName,std::ios_base::app);
            if(csvLogFile.is_open()){
                csvLogFile.seekp(0,std::ios::end);
                if(csvLogFile.tellp()==0){
                    // Add "Policy" to the CSV header
                    csvLogFile<<"Policy,Protocol,MessageSizeKB,TransferTimeSeconds,ThroughputKbps\n";
                }
            }
        }
    }

    ~Server(){
        if(csvLogFile.is_open()){
            csvLogFile.close();
        }
        shutdown();
    }

    bool initialize();
    void start();
    void shutdown();

private:
    void scheduler();
    void handleNegotiation(int clientSocket,sockaddr_in clientAddr);
    void handleDataTransfer(const std::string& protocol,int sizeKB,int dataSocket,const std::string& clientIp,int clientPid);

    int tcpPort;
    int tcpSocket;
    SchedulingPolicy schedulingPolicy;
    bool isRunning;

    std::queue<ClientRequest> requestQueue;
    std::mutex queueMutex;
    std::condition_variable cv;
    std::thread schedulerThread;

    std::ofstream csvLogFile;
    std::mutex logMutex;
};


#endif