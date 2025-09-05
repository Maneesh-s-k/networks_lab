#ifndef MESSAGE_HH
#define MESSAGE_HH

#include <iostream>
#include <string>
#include <vector>

class Message {
public:
    int messageType;
    int messageLength;
    std::string messageContent;

    Message(int type=0,const std::string& content=""): 
        messageType(type),
        messageLength(static_cast<int>(content.size())),
        messageContent(content) {}

    std::vector<char> serialize() const;
    static Message deserialize(const char* buffer,int length);
};


#endif