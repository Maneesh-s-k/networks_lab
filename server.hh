#ifndef SERVER_HH
#define SERVER_HH

#include <string>
#include <queue>
#include <deque>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <netinet/in.h>
#include <fstream>
#include <optional>

enum SchedulingPolicy {FCFS, RR};

struct ClientRequest {
    int clientSocket;
    sockaddr_in clientAddr;
    int clientPid;
    std::string protocol;
    int sizeKB;
};

class Server {
public:
    Server(int port, SchedulingPolicy policy, std::optional<std::string> csvLogFileName)
        : tcpPort(port), tcpSocket(-1), schedulingPolicy(policy), isRunning(false) {
        if (csvLogFileName) {
            csvLogFile.open(*csvLogFileName, std::ios_base::app);
            if (csvLogFile.is_open()) {
                csvLogFile.seekp(0, std::ios::end);
                if (csvLogFile.tellp() == 0) {
                    csvLogFile << "Policy,Protocol,MessageSizeKB,TransferTimeMicroseconds,ThroughputKbps\n";
                }
            }
        }
    }

    ~Server() {
        if (csvLogFile.is_open()) {
            csvLogFile.close();
        }
        shutdown();
    }

    bool initialize();
    void start();
    void shutdown();

private:
    void scheduler();
    void handleNegotiation(const ClientRequest& clientReq);
    void handleDataTransfer(const std::string& protocol, int sizeKB, int dataSocket, 
                           const std::string& clientIp, int clientPid);

    int tcpPort;
    int tcpSocket;
    SchedulingPolicy schedulingPolicy;
    bool isRunning;

    // FCFS: single queue, serve one client completely before next
    std::queue<int> fcfsClientOrder;  // PIDs in order
    std::unordered_map<int, std::queue<ClientRequest>> fcfsClientQueues;

    // RR: round-robin over clients
    std::deque<int> rrActiveClients;  // PIDs in round-robin order
    std::unordered_map<int, std::queue<ClientRequest>> rrClientQueues;

    std::mutex queueMutex;
    std::condition_variable cv;
    std::thread schedulerThread;

    std::ofstream csvLogFile;
    std::mutex logMutex;
};

#endif
