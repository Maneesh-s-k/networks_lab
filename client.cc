#include "message.hh"
#include "client.hh"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdlib>
#include <stdexcept>
#include <vector>
#include <chrono>

using namespace std;

bool Client::initializeUDP() {
    udpSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udpSocket < 0) {
        cerr << "Error creating UDP socket\n";
        return false;
    }

    sockaddr_in clientAddr{};
    clientAddr.sin_family = AF_INET;
    clientAddr.sin_addr.s_addr = INADDR_ANY;
    clientAddr.sin_port = 0;

    if (::bind(udpSocket, (struct sockaddr*)&clientAddr, sizeof(clientAddr)) < 0) {
        cerr << "Error binding UDP socket\n";
        close(udpSocket);
        udpSocket = -1;
        return false;
    }

    socklen_t len = sizeof(clientAddr);
    if (getsockname(udpSocket, (struct sockaddr*)&clientAddr, &len) == -1) {
        cerr << "Error getting UDP socket name\n";
        close(udpSocket);
        udpSocket = -1;
        return false;
    }

    udpPort = ntohs(clientAddr.sin_port);
    cout << "UDP socket bound on port " << udpPort << "\n";
    return true;
}

bool Client::connectToServer() {
    tcpSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (tcpSocket < 0) {
        cerr << "Error creating TCP socket\n";
        return false;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverTcpPort);

    if (inet_pton(AF_INET, serverIpAddress.c_str(), &serverAddr.sin_addr) <= 0) {
        cerr << "Invalid server IP address\n";
        close(tcpSocket);
        tcpSocket = -1;
        return false;
    }

    if (::connect(tcpSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        cerr << "TCP connection to server failed\n";
        close(tcpSocket);
        tcpSocket = -1;
        return false;
    }

    cout << "Connected to server " << serverIpAddress << " on port " << serverTcpPort << "\n";
    return true;
}

int Client::requestUDPPort() {
    Message request(1, "Requesting UDP port");
    vector<char> serializedRequest = request.serialize();

    if (send(tcpSocket, serializedRequest.data(), serializedRequest.size(), 0) < 0) {
        cerr << "Error sending UDP port request to server\n";
        return -1;
    }

    char buffer[1024];
    int bytesRead = recv(tcpSocket, buffer, sizeof(buffer), 0);
    if (bytesRead <= 0) {
        cerr << "Error receiving UDP port response from server\n";
        return -1;
    }

    try {
        Message response = Message::deserialize(buffer, bytesRead);
        if (response.messageType == 2) {
            int serverUdpPort = stoi(response.messageContent);
            cout << "Received allocated UDP port from server: " << serverUdpPort << "\n";

            Message udpPortMsg(5, to_string(udpPort));
            vector<char> udpPortSerialized = udpPortMsg.serialize();

            if (send(tcpSocket, udpPortSerialized.data(), udpPortSerialized.size(), 0) < 0) {
                cerr << "Error sending client UDP port info to server\n";
                return -1;
            }
            return serverUdpPort;
        } else {
            cerr << "Unexpected message type in UDP port response\n";
            return -1;
        }
    } catch (const exception& e) {
        cerr << "Exception deserializing UDP port response: " << e.what() << "\n";
        return -1;
    }
}

bool Client::sendUDPMessage(int udpPort, const string& message) {
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(udpPort);

    if (inet_pton(AF_INET, serverIpAddress.c_str(), &serverAddr.sin_addr) <= 0) {
        cerr << "Invalid server IP address\n";
        return false;
    }

    Message udpMessage(3, message);
    vector<char> serializedMessage = udpMessage.serialize();

    ssize_t bytesSent = sendto(udpSocket, serializedMessage.data(), serializedMessage.size(), 0,
                              (struct sockaddr*)&serverAddr, sizeof(serverAddr));
    if (bytesSent < 0) {
        cerr << "Error sending UDP message to server\n";
        return false;
    }

    cout << "Sent UDP message to server: " << message << "\n";
    return true;
}

bool Client::receiveUDPResponse() {
    if (udpSocket < 0) {
        cerr << "UDP socket not initialized\n";
        return false;
    }

    vector<char> reassembledData;
    size_t expectedChunks = 32;
    size_t receivedChunks = 0;

    auto startTime = chrono::steady_clock::now();

    while (receivedChunks < expectedChunks) {
        char buffer[2048];
        sockaddr_in fromAddr{};
        socklen_t fromLen = sizeof(fromAddr);

        int bytesReceived = recvfrom(udpSocket, buffer, sizeof(buffer), 0,
                                     (struct sockaddr*)&fromAddr, &fromLen);
        if (bytesReceived < 0) {
            cerr << "Error receiving UDP response\n";
            return false;
        }

        try {
            Message segment = Message::deserialize(buffer, bytesReceived);
            if (segment.messageType != 3 && segment.messageType != 4) {
                cerr << "Unexpected message type received in UDP response: " << segment.messageType << "\n";
                continue;
            }

            reassembledData.insert(reassembledData.end(),
                                  segment.messageContent.begin(),
                                  segment.messageContent.end());
            receivedChunks++;
        } catch (const exception& e) {
            cerr << "Deserialization error: " << e.what() << "\n";
            continue;
        }
    }

    auto endTime = chrono::steady_clock::now();
    chrono::duration<double> duration = endTime - startTime;
    double throughput = reassembledData.size() / 1024.0 / duration.count();

    cout << "Received full UDP response (" << reassembledData.size() << " bytes) in "
         << duration.count() << " seconds. Throughput: " << throughput << " KB/s\n";
    return true;
}

void Client::disconnect() {
    if (tcpSocket >= 0) {
        close(tcpSocket);
        tcpSocket = -1;
    }
    if (udpSocket >= 0) {
        close(udpSocket);
        udpSocket = -1;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <Server IP Address> <Server Port number>\n";
        return 1;
    }

    string serverIp = argv[1];
    int port = atoi(argv[2]);
    if (port <= 0 || port > 65535) {
        cerr << "Invalid server port number\n";
        return 1;
    }

    Client client(serverIp, port);

    if (!client.initializeUDP()) {
        cerr << "Failed to initialize UDP socket\n";
        return 1;
    }

    if (!client.connectToServer()) {
        cerr << "Failed to connect to server\n";
        return 1;
    }

    int serverUdpPort = client.requestUDPPort();
    if (serverUdpPort == -1) {
        client.disconnect();
        return 1;
    }

    if (!client.sendUDPMessage(serverUdpPort, "Hello from Client")) {
        client.disconnect();
        return 1;
    }

    if (!client.receiveUDPResponse()) {
        client.disconnect();
        return 1;
    }

    client.disconnect();
    return 0;
}
