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
#include <cstdio> // For perror

using namespace std;

bool Client::transferMessage() {
    // --- PHASE 1: NEGOTIATION (TCP) ---
    int negotiationSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (negotiationSocket < 0) {
        cerr << "Error: Could not create negotiation socket.\n";
        return false;
    }

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(serverTcpPort);
    inet_pton(AF_INET, serverIpAddress.c_str(), &serverAddr.sin_addr);

    if (::connect(negotiationSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        cerr << "\nError: TCP negotiation connection to server failed.\n";
        close(negotiationSocket);
        return false;
    }

    stringstream ss;
    ss << protocol << " " << messageSizeKB;
    Message request(1, ss.str());
    vector<char> serializedRequest = request.serialize();
    send(negotiationSocket, serializedRequest.data(), serializedRequest.size(), 0);

    char buffer[1024];
    int bytesRead = recv(negotiationSocket, buffer, sizeof(buffer), 0);
    if (bytesRead <= 0) {
        cerr << "\nError: Did not receive negotiation response from server.\n";
        close(negotiationSocket);
        return false;
    }
    
    int dataPort = 0;
    try {
        Message response = Message::deserialize(buffer, bytesRead);
        if (response.messageType != 2) {
            cerr << "\nError: Unexpected message type in response: " << response.messageType << "\n";
            close(negotiationSocket);
            return false;
        }
        dataPort = stoi(response.messageContent);
    } catch (const exception& e) {
        cerr << "\nError: Invalid negotiation response: " << e.what() << "\n";
        close(negotiationSocket);
        return false;
    }
    
    close(negotiationSocket);

    // --- PHASE 2: DATA TRANSFER (TCP or UDP) ---
    string data(messageSizeKB * 1024, 'A');
    sockaddr_in dataServerAddr{};
    dataServerAddr.sin_family = AF_INET;
    dataServerAddr.sin_port = htons(dataPort);
    inet_pton(AF_INET, serverIpAddress.c_str(), &dataServerAddr.sin_addr);

    if (protocol == "tcp") {
        int dataSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (dataSocket < 0) {
            cerr << "\nError: creating data TCP socket\n"; return false;
        }
        if (::connect(dataSocket, (struct sockaddr*)&dataServerAddr, sizeof(dataServerAddr)) < 0) {
            perror("\nconnect failed");
            cerr << "Error: TCP data transfer connection failed on port " << dataPort << "\n";
            close(dataSocket); return false;
        }
        send(dataSocket, data.data(), data.size(), 0);
        recv(dataSocket, buffer, sizeof(buffer), 0); // Wait for confirmation
        close(dataSocket);

    } else if (protocol == "udp") {
        int dataSocket = socket(AF_INET, SOCK_DGRAM, 0);
        if (dataSocket < 0) {
            cerr << "\nError: creating data UDP socket\n"; return false;
        }

        int bufferSize = messageSizeKB * 1024 + 2048;
        setsockopt(dataSocket, SOL_SOCKET, SO_SNDBUF, &bufferSize, sizeof(bufferSize));

        if (sendto(dataSocket, data.data(), data.size(), 0, (struct sockaddr*)&dataServerAddr, sizeof(dataServerAddr)) < 0) {
            perror("\nsendto failed");
            cerr << "Error: sending UDP data to port " << dataPort << "\n"; 
            close(dataSocket); 
            return false;
        }
        recvfrom(dataSocket, buffer, sizeof(buffer), 0, nullptr, nullptr); // Wait for confirmation
        close(dataSocket);
    }

    return true;
}

int main(int argc, char* argv[]) {
    if (argc != 6) {
        cerr << "Usage: " << argv[0] << " <Server IP> <Server Port> <Mode (tcp/udp)> <Message Size KB> <Num Messages>\n";
        return 1;
    }

    string serverIp = argv[1];
    int port = atoi(argv[2]);
    string protocol = argv[3];
    int messageSize = atoi(argv[4]);
    int numMessages = atoi(argv[5]);

    if (protocol != "tcp" && protocol != "udp") {
        cerr << "Invalid protocol. Use 'tcp' or 'udp'.\n"; return 1;
    }
    if (protocol == "udp" && (messageSize < 1 || messageSize > 32)) {
        cerr << "UDP mode only supports message sizes from 1 to 32 KB.\n"; return 1;
    }
    
    Client client(serverIp, port, messageSize, protocol, numMessages);
    for (int i = 0; i < numMessages; ++i) {
        cout << "Sending message " << (i + 1) << "/" << numMessages << "... ";
        cout.flush();

        if (client.transferMessage()) {
            cout << "OK.\n";
        } else {
            // Error message is printed inside transferMessage()
            cerr << "Transfer failed.\n";
            return 1;
        }
    }
    cout << "All transfers completed successfully.\n";
    return 0;
}