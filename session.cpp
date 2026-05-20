#include "session.h"
#include <sys/socket.h>
#include <unistd.h>
#include <cerrno>
#include <iostream>
#include <cstring>

void Session::queueMessage(const std::string &message) {
    outgoing_messages.push(message);
}

bool Session::hadDataToSend() const {
    return !current_send_buffer.empty() || !outgoing_messages.empty();
}

SendResult Session::trySendData() {
    if (current_send_buffer.empty() && !outgoing_messages.empty()) {
        current_send_buffer = outgoing_messages.front();
        outgoing_messages.pop();
    }

    if (current_send_buffer.empty()) {
        return COMPLETE;
    }

    ssize_t sent = send(fd, current_send_buffer.c_str(), current_send_buffer.size(), 0);
    if (sent < 0) {
        if (errno == EWOULDBLOCK || errno == EAGAIN) {
            return PENDING;
        }
        std::cerr << "Error sending data to player " << (player ? player->getName() : "<unknown>") << ": " << strerror(errno) << std::endl;
        return ERROR;
    }

    if (static_cast<size_t>(sent) < current_send_buffer.size()) {
        current_send_buffer = current_send_buffer.substr(sent);
        return PENDING;
    }

    current_send_buffer.clear();
    return outgoing_messages.empty() ? COMPLETE : PENDING;
}
