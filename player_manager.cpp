#include "player_manager.h"
#include <iostream>

void PlayerManager::addPlayer(std::shared_ptr<Session> session) {
    if (!session) return;
    sessions[session->fd] = session;
}

void PlayerManager::removePlayer(int fd) {
    sessions.erase(fd);
}

std::shared_ptr<Session> PlayerManager::getPlayer(int fd) {
    auto it = sessions.find(fd);
    if (it != sessions.end()) return it->second;
    return nullptr;
}

void PlayerManager::broadcastMessage(int sender_fd, const std::string &message) {
    std::string sender_name = "Unknown";
    auto s = getPlayer(sender_fd);
    if (s && s->player) sender_name = s->player->getName();
    std::string full_message = sender_name + ": " + message + "\r\n";
    for (auto &pair : sessions) {
        if (pair.first == sender_fd) continue;
        if (pair.second) pair.second->queueMessage(full_message);
    }
}