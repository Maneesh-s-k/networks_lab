#include "message.hh"
#include "server.hh"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <unistd.h>
#include <cstdlib>
#include <thread>
#include <stdexcept>
#include <vector>

using namespace std;

int getFreeUdpPort() {
    return 49152 + rand() % (65535 - 49152 + 1);
}

void Server::tcpScheduler() {
    while (isRunning) {
        int clientSocket;
        sockaddr_in clientTcpAddr;
        {
            unique_lock<mutex> lock(tcpQueueMutex);
            tcpCv.wait(lock, [this] { return !tcpRequestQueue.empty() || !isRunning; });

            if (!isRunning && tcpRequestQueue.empty()) break;

            auto entry = tcpRequestQueue.front();
            tcpRequestQueue.pop();
            clientSocket = entry.first;
            clientTcpAddr = entry.second;
        }
        handleClientTCP(clientSocket, clientTcpAddr);
        close(clientSocket);
    }
}

void Server::udpScheduler() {
    while (isRunning) {
        UDPClientState clientState;
        {
            unique_lock<mutex> lock(udpQueueMutex);
            udpCv.wait(lock, [this] { return !udpClientQueue.empty() || !isRunning; });

            if (!isRunning && udpClientQueue.empty()) break;

            clientState = udpClientQueue.front();
            udpClientQueue.pop();
        }

        // Send exactly one chunk
        const auto& chunk = clientState.chunks[clientState.nextChunkToSend];
        ssize_t sent = sendto(udpSocket, chunk.data(), chunk.size(), 0,
                              (struct sockaddr*)&clientState.clientAddr, sizeof(clientState.clientAddr));
        if (sent < 0) {
            cerr << "Error sending UDP chunk\n";
            // optionally handle error/retry
        }
        clientState.bytesTransferred += chunk.size();
        clientState.nextChunkToSend++;

        if (clientState.nextChunkToSend < clientState.totalChunks) {
            lock_guard<mutex> lock(udpQueueMutex);
            udpClientQueue.push(clientState);
            udpCv.notify_one();
        } else {
            auto endTime = chrono::steady_clock::now();
            chrono::duration<double> duration = endTime - clientState.startTime;
            double throughput = clientState.bytesTransferred / 1024.0 / duration.count();
            cout << "Completed UDP transfer to client (" << clientState.bytesTransferred << " bytes) in "
                 << duration.count() << " seconds. Throughput: " << throughput << " KB/s\n";
        }
    }
}

bool Server::initializeTCP() {
    tcpSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcpSocket < 0) {
        cerr << "Error creating TCP socket\n";
        return false;
    }
    cout << "TCP socket created successfully\n";

    int opt = 1;
    if (setsockopt(tcpSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        cerr << "setsockopt(SO_REUSEADDR) failed\n";
        close(tcpSocket);
        return false;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(tcpPort);

    if (::bind(tcpSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        cerr << "Error binding TCP socket\n";
        close(tcpSocket);
        return false;
    }

    udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket < 0) {
        cerr << "Error creating UDP socket\n";
        close(tcpSocket);
        return false;
    }

    if (::bind(udpSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        cerr << "Error binding UDP socket\n";
        close(udpSocket);
        close(tcpSocket);
        return false;
    }
    cout << "UDP socket created and bound to port " << tcpPort << "\n";
    return true;
}

void Server::acceptClients() {
    if (::listen(tcpSocket, 5) < 0) {
        cerr << "Error listening on TCP socket\n";
        return;
    }
    cout << "Server listening on port " << tcpPort << "\n";

    isRunning = true;

    tcpSchedulerThread = thread(&Server::tcpScheduler, this);
    udpSchedulerThread = thread(&Server::udpScheduler, this);

    while (isRunning) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        int clientSocket = ::accept(tcpSocket, (struct sockaddr*)&clientAddr, &clientLen);
        if (clientSocket < 0) {
            cerr << "Error accepting client connection\n";
            continue;
        }
        cout << "Client connected successfully, adding to request queue\n";

        {
            lock_guard<mutex> lock(tcpQueueMutex);
            tcpRequestQueue.push(make_pair(clientSocket, clientAddr));
        }
        tcpCv.notify_one();
    }

    tcpSchedulerThread.join();
    udpSchedulerThread.join();
}

void Server::handleClientTCP(int clientSocket, sockaddr_in clientTcpAddr) {
    char buffer[1024] = {0};
    int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesRead <= 0) {
        cerr << "Error receiving data from client\n";
        return;
    }

    Message request;
    try {
        request = Message::deserialize(buffer, bytesRead);
    } catch (const std::invalid_argument& e) {
        cerr << "Invalid message received: " << e.what() << "\n";
        return;
    }

    if (request.messageType != 1) {
        cerr << "Unexpected message type received: " << request.messageType << "\n";
        return;
    }

    int udpPort = getFreeUdpPort();
    if (udpPort == -1) {
        cerr << "Error allocating UDP port\n";
        return;
    }

    Message response(2, to_string(udpPort));
    vector<char> serializedResponse = response.serialize();
    send(clientSocket, serializedResponse.data(), serializedResponse.size(), 0);

    // Receive client's UDP port (type 5)
    bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
    if (bytesRead <= 0) {
        cerr << "Error receiving client UDP port info\n";
        return;
    }

    Message udpPortMsg;
    try {
        udpPortMsg = Message::deserialize(buffer, bytesRead);
    } catch (const std::invalid_argument& e) {
        cerr << "Invalid UDP port info message: " << e.what() << "\n";
        return;
    }

    if (udpPortMsg.messageType != 5) {
        cerr << "Expected client UDP port info!\n";
        return;
    }

    int clientUdpPort = stoi(udpPortMsg.messageContent);

    UDPClientState clientUdpState;
    memset(&clientUdpState.clientAddr, 0, sizeof(clientUdpState.clientAddr));
    clientUdpState.clientAddr.sin_family = AF_INET;
    clientUdpState.clientAddr.sin_addr.s_addr = clientTcpAddr.sin_addr.s_addr;
    clientUdpState.clientAddr.sin_port = htons(clientUdpPort);
    clientUdpState.startTime = chrono::steady_clock::now();

    string largeMessage(32 * 1024, 'A');
    const size_t CHUNK_SIZE = 1024;
    for (size_t i = 0; i < largeMessage.size(); i += CHUNK_SIZE) {
        size_t chunkLen = min(CHUNK_SIZE, largeMessage.size() - i);
        clientUdpState.chunks.emplace_back(largeMessage.begin() + i, largeMessage.begin() + i + chunkLen);
    }
    clientUdpState.totalChunks = clientUdpState.chunks.size();
    clientUdpState.nextChunkToSend = 0;
    clientUdpState.bytesTransferred = 0;
    clientUdpState.completed = false;

    {
        lock_guard<mutex> lock(udpQueueMutex);
        udpClientQueue.push(clientUdpState);
    }
    udpCv.notify_one();
}

void Server::shutdown() {
    isRunning = false;
    tcpCv.notify_all();
    udpCv.notify_all();

    if (tcpSchedulerThread.joinable()) tcpSchedulerThread.join();
    if (udpSchedulerThread.joinable()) udpSchedulerThread.join();

    if (tcpSocket >= 0) {
        close(tcpSocket);
        tcpSocket = -1;
    }
    if (udpSocket >= 0) {
        close(udpSocket);
        udpSocket = -1;
    }
    cout << "Server sockets closed\n";
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <ServerPortnumber> <SchedulingPolicy(1-fcfs,2-RR)>\n";
        return 1;
    }

    int port = atoi(argv[1]);
    if (port <= 0 || port > 65535) {
        cerr << "Invalid port number\n";
        return 1;
    }

    int schednum = atoi(argv[2]);
    SchedulingPolicy policy = (schednum == 1) ? FCFS : RR;
    if (schednum != 1 && schednum != 2) {
        cerr << "Invalid scheduling policy. Use 1 for FCFS, 2 for RR\n";
        return 1;
    }

    Server server("0.0.0.0", port, policy);

    if (!server.initializeTCP()) {
        cerr << "Failed to initialize TCP socket\n";
        return 1;
    }

    server.acceptClients();
    server.shutdown();

    return 0;
}
