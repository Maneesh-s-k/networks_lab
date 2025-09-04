#ifndef SERVER_HH
#define SERVER_HH

#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <netinet/in.h>

enum SchedulingPolicy { FCFS, RR };

struct ClientRequest {
    int clientSocket;
    sockaddr_in clientAddr;
};

class Server {
public:
    Server(int port, SchedulingPolicy policy)
        : tcpPort(port),
          tcpSocket(-1),
          schedulingPolicy(policy),
          isRunning(false)
    {}

    ~Server() { shutdown(); }

    bool initialize();
    void start();
    void shutdown();

private:
    void scheduler();
    void handleNegotiation(int clientSocket, sockaddr_in clientAddr);
    void handleDataTransfer(const std::string& protocol, int sizeKB, int port);

    int tcpPort;
    int tcpSocket;
    SchedulingPolicy schedulingPolicy;
    bool isRunning;

    std::queue<ClientRequest> requestQueue;
    std::mutex queueMutex;
    std::condition_variable cv;
    std::thread schedulerThread;
};

#endif