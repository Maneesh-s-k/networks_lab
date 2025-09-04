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

using namespace std;

void Server::scheduler() {
    while (isRunning) {
        ClientRequest clientReq;
        {
            unique_lock<mutex> lock(queueMutex);
            cv.wait(lock, [this] { return !requestQueue.empty() || !isRunning; });
            if (!isRunning && requestQueue.empty()) break;

            clientReq = requestQueue.front();
            requestQueue.pop();
        }
        thread(&Server::handleNegotiation, this, clientReq.clientSocket, clientReq.clientAddr).detach();
    }
}

void Server::handleDataTransfer(const string& protocol, int sizeKB, int dataSocket) {
    const size_t totalBytesToReceive = sizeKB * 1024;
    size_t bytesReceived = 0;
    auto startTime = chrono::steady_clock::now();
    
    sockaddr_in addr;
    socklen_t len = sizeof(addr);
    getsockname(dataSocket, (struct sockaddr*)&addr, &len);
    int port = ntohs(addr.sin_port);

    if (protocol == "tcp") {
        int acceptedSocket = ::accept(dataSocket, nullptr, nullptr);
        close(dataSocket); // Close listener after connection is accepted.
        if (acceptedSocket < 0) {
            cerr << "Port " << port << ": Error accepting TCP data connection.\n"; return;
        }

        char buffer[4096];
        while (bytesReceived < totalBytesToReceive) {
            int n = recv(acceptedSocket, buffer, sizeof(buffer), 0);
            if (n <= 0) break;
            bytesReceived += n;
        }

        Message finalResp(4, "TCP transfer complete");
        vector<char> finalRespSer = finalResp.serialize();
        send(acceptedSocket, finalRespSer.data(), finalRespSer.size(), 0);
        close(acceptedSocket);

    } else if (protocol == "udp") {
        char buffer[32 * 1024]; 
        sockaddr_in clientDataAddr{};
        socklen_t clientLen = sizeof(clientDataAddr);
        
        int n = recvfrom(dataSocket, buffer, sizeof(buffer), 0, (struct sockaddr*)&clientDataAddr, &clientLen);
        if (n > 0) {
            bytesReceived += n;
        }
        
        Message finalResp(4, "UDP transfer complete");
        vector<char> finalRespSer = finalResp.serialize();
        sendto(dataSocket, finalRespSer.data(), finalRespSer.size(), 0, (struct sockaddr*)&clientDataAddr, clientLen);
        close(dataSocket);
    }

    auto endTime = chrono::steady_clock::now();
    chrono::duration<double> duration = endTime - startTime;
    double throughputMbs = (duration.count() > 0) ? (bytesReceived * 8.0) / (duration.count() * 1024 * 1024) : 0;
    
    cout << fixed << setprecision(2);
    cout << "[RESULT] Port " << port << " (" << protocol << "): "
         << static_cast<double>(bytesReceived) / 1024.0 << " KB in "
         << duration.count() << "s -> " << throughputMbs << " Mbps.\n";
}


void Server::handleNegotiation(int clientSocket, sockaddr_in clientAddr) {
    char client_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(clientAddr.sin_addr), client_ip, INET_ADDRSTRLEN);

    char recv_buf[1024];
    int bytesRead = recv(clientSocket, recv_buf, sizeof(recv_buf), 0);
    if (bytesRead <= 0) {
        close(clientSocket);
        return;
    }

    try {
        Message req = Message::deserialize(recv_buf, bytesRead);
        if (req.messageType != 1) {
            cerr << "Error: Unexpected message type from " << client_ip << "\n";
            close(clientSocket);
            return;
        }

        stringstream ss(req.messageContent);
        string protocol;
        int sizeKB;
        ss >> protocol >> sizeKB;

        int dataSocket = (protocol == "tcp") ? socket(AF_INET, SOCK_STREAM, 0) : socket(AF_INET, SOCK_DGRAM, 0);
        if (dataSocket < 0) { close(clientSocket); return; }

        sockaddr_in dataAddr{};
        dataAddr.sin_family = AF_INET;
        dataAddr.sin_addr.s_addr = INADDR_ANY;
        dataAddr.sin_port = 0;

        if (::bind(dataSocket, (struct sockaddr*)&dataAddr, sizeof(dataAddr)) < 0) {
             close(clientSocket); close(dataSocket); return;
        }
        
        if (protocol == "tcp" && ::listen(dataSocket, 1) < 0) {
             close(clientSocket); close(dataSocket); return;
        }
        
        socklen_t len = sizeof(dataAddr);
        getsockname(dataSocket, (struct sockaddr*)&dataAddr, &len);
        int dataPort = ntohs(dataAddr.sin_port);

        cout << "[+] Client " << client_ip << " negotiated port " << dataPort << " for " << sizeKB << "KB " << protocol << " transfer.\n";

        Message resp(2, to_string(dataPort));
        vector<char> respSer = resp.serialize();
        send(clientSocket, respSer.data(), respSer.size(), 0);

        thread(&Server::handleDataTransfer, this, protocol, sizeKB, dataSocket).detach();

    } catch (const exception& e) {
        cerr << "Error handling negotiation for " << client_ip << ": " << e.what() << "\n";
    }
    close(clientSocket);
}

bool Server::initialize() {
    tcpSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcpSocket < 0) {
        cerr << "Error creating main TCP socket\n"; return false;
    }

    int opt = 1;
    setsockopt(tcpSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(tcpPort);

    if (::bind(tcpSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        cerr << "Error binding main TCP socket\n"; close(tcpSocket); return false;
    }
    return true;
}

void Server::start() {
    if (::listen(tcpSocket, 10) < 0) {
        cerr << "Error listening on main TCP socket\n"; return;
    }
    cout << "Server listening for negotiations on port " << tcpPort << "...\n";

    isRunning = true;
    schedulerThread = thread(&Server::scheduler, this);

    while (isRunning) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = ::accept(tcpSocket, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientSocket < 0) {
            if (isRunning) cerr << "Error accepting client connection\n";
            continue;
        }
        
        {
            lock_guard<mutex> lock(queueMutex);
            requestQueue.push({clientSocket, clientAddr});
        }
        cv.notify_one();
    }
}

void Server::shutdown() {
    isRunning = false;
    cv.notify_all();
    ::shutdown(tcpSocket, SHUT_RDWR);
    if (tcpSocket >= 0) { close(tcpSocket); tcpSocket = -1; }
    if (schedulerThread.joinable()) schedulerThread.join();
    cout << "Server shut down.\n";
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <ServerPort> <SchedulingPolicy (1-FCFS, 2-RR)>\n";
        return 1;
    }
    int port = atoi(argv[1]);
    int schednum = atoi(argv[2]);
    SchedulingPolicy policy = (schednum == 2) ? RR : FCFS;

    Server server(port, policy);
    if (!server.initialize()) {
        cerr << "Failed to initialize server\n";
        return 1;
    }

    server.start();
    return 0;
}