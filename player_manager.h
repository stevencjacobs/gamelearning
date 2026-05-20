#ifndef PLAYER_MANAGER_H
#define PLAYER_MANAGER_H

#include "session.h"
#include <unordered_map>
#include <memory>

class PlayerManager {
public:
    void addPlayer(std::shared_ptr<Session> session);
    void removePlayer(int fd);
    std::shared_ptr<Session> getPlayer(int fd);
    void broadcastMessage(int sender_fd, const std::string &message);
private:
    std::unordered_map<int, std::shared_ptr<Session>> sessions; // Map of fd to Session
};

#endif // PLAYER_MANAGER_H