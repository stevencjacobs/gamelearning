#ifndef SESSION_H
#define SESSION_H

#include <memory>
#include <queue>
#include <string>
#include "player.h"

enum SendResult {
    COMPLETE,
    PENDING,
    ERROR
};

struct Session {
    int fd = -1;
    std::shared_ptr<Player> player;
    std::string buffer;
    std::queue<std::string> outgoing_messages;
    std::string current_send_buffer;

    void queueMessage(const std::string &message);
    bool hadDataToSend() const;
    SendResult trySendData();
};

#endif // SESSION_H
