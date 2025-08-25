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

    Message(int type=0, const std::string& content=""){
        messageType=type;
        messageContent=content;
        messageLength=content.length();
    }

    // Serialize message into a byte buffer for sending
    std::vector<char> serialize() const;

    // Deserialize from a byte buffer to populate the message
    static Message deserialize(const char* buffer, int length);

};


#endif
