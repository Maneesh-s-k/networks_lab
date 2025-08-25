#include "message.hh"
#include <cstring> // for memcpy
#include <stdexcept>

using namespace::std;

// convert message content into bytes(stored in vector<char> buffer
// and preceeded by header having messageLength and messageType
vector<char> Message::serialize() const {

    vector<char> buffer(sizeof(int)*2 + sizeof(char)*messageLength);
    memcpy(buffer.data(), &messageType, sizeof(int));
    memcpy(buffer.data()+sizeof(int), &messageLength, sizeof(int));
    memcpy(buffer.data()+2*sizeof(int), messageContent.data(), sizeof(char)*messageLength);
    return buffer;

}

// convert byte buffer to message object
Message Message::deserialize(const char* buffer, int length){

    if(length < 2*sizeof(int)){
        throw invalid_argument("Invalid buffer, too small to contain the header");
    }

    int type=0;
    int msglength=0;

    memcpy(&type, buffer, sizeof(int));
    memcpy(&msglength, buffer+sizeof(int), sizeof(int));

    if(length < (2*sizeof(int)+msglength*sizeof(char)) ){
        throw invalid_argument("Invalid buffer, too small to contain the full message");
    }
    string content;
    content.assign(buffer+2*sizeof(int), buffer+2*sizeof(int)+msglength*sizeof(char));

    return Message(type, content);

}