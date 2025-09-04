#ifndef SERVER_HH
#define SERVER_HH

#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <netinet/in.h>
#include <thread>

enum SchedulingPolicy { FCFS, RR };

struct UDPClientState {
    sockaddr_in clientAddr;
    std::vector<std::vector<char>> chunks;
    size_t nextChunkToSend = 0;
    size_t totalChunks = 0;
    size_t bytesTransferred = 0;
    std::chrono::steady_clock::time_point startTime;
    bool completed = false;
};

class Server {
public:
    Server(const std::string& ip, int port, SchedulingPolicy policy)
        : ipAddress(ip),
          tcpPort(port),
          tcpSocket(-1),
          udpSocket(-1),
          schedulingPolicy(policy),
          isRunning(false)
    {}

    ~Server() { shutdown(); }

    bool initializeTCP();
    void acceptClients();
    void handleClientTCP(int clientSocket, sockaddr_in clientTcpAddr);
    void tcpScheduler();
    void udpScheduler();
    void shutdown();

private:
    std::string ipAddress;
    int tcpPort;
    int tcpSocket;
    int udpSocket;
    SchedulingPolicy schedulingPolicy;
    bool isRunning;

    std::queue<std::pair<int, sockaddr_in>> tcpRequestQueue;
    std::mutex tcpQueueMutex;
    std::condition_variable tcpCv;

    std::queue<UDPClientState> udpClientQueue;
    std::mutex udpQueueMutex;
    std::condition_variable udpCv;

    std::thread tcpSchedulerThread;
    std::thread udpSchedulerThread;
};

#endif
