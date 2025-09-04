#include "message.hh"
#include <cstring>
#include <stdexcept>

using namespace std;

vector<char> Message::serialize() const {
    vector<char> buffer(sizeof(int) * 2 + messageContent.size());
    // Layout: [Type(int)][Length(int)][Content(bytes)]
    memcpy(buffer.data(), &messageType, sizeof(int));
    memcpy(buffer.data() + sizeof(int), &messageLength, sizeof(int));
    if (messageLength > 0) {
        memcpy(buffer.data() + 2 * sizeof(int),
               messageContent.data(),
               static_cast<size_t>(messageLength));
    }
    return buffer;
}

Message Message::deserialize(const char* buffer, int length) {
    if (length < 2 * static_cast<int>(sizeof(int))) {
        throw invalid_argument("Invalid buffer, too small to contain the header");
    }
    int type = 0;
    int len = 0;
    memcpy(&type, buffer, sizeof(int));
    memcpy(&len, buffer + sizeof(int), sizeof(int));
    if (len < 0) {
        throw invalid_argument("Invalid message length");
    }
    if (length < 2 * static_cast<int>(sizeof(int)) + len) {
        throw invalid_argument("Invalid buffer, too small to contain the full message");
    }
    string content;
    if (len > 0) {
        content.assign(buffer + 2 * sizeof(int), buffer + 2 * sizeof(int) + len);
    }
    return Message(type, content);
}