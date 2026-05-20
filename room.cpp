#include <algorithm>
#include "room.h"

Room::Room(const std::string &description)
    : description(description) {}

const std::string &Room::getDescription() const {
    return description;
}

void Room::setDescription(const std::string &description) {
    this->description = description;
}

void Room::addPlayer(const std::shared_ptr<Session> &session) {
    players.push_back(session);
}

void Room::removePlayer(int fd) {
    players.erase(std::remove_if(players.begin(), players.end(), [fd](const std::shared_ptr<Session> &session) {
        return session && session->fd == fd;
    }), players.end());
}

void Room::addEntity(const std::shared_ptr<AttackableEntity> &entity) {
    entities.push_back(entity);
}

void Room::removeEntity(const std::shared_ptr<AttackableEntity> &entity) {
    entities.erase(std::remove_if(entities.begin(), entities.end(), [&entity](const std::shared_ptr<AttackableEntity> &existing) {
        return existing == entity;
    }), entities.end());
}

void Room::broadcast(const std::string &message) {
    for (const auto &session : players) {
        if (session) {
            session->queueMessage(message);
        }
    }
}

void Room::broadcastFrom(const std::string &sender, const std::string &message) {
    std::string fullMessage = sender + " says: " + message;
    if (!fullMessage.empty() && fullMessage.back() != '\n') {
        fullMessage += "\r\n";
    }
    for (const auto &session : players) {
        if (session) {
            session->queueMessage(fullMessage);
        }
    }
}

void Room::broadcastFrom(const std::shared_ptr<Session> &sender, const std::string &message) {
    std::string fullMessage = sender->player->getName() + " says: " + message;
    if (!fullMessage.empty() && fullMessage.back() != '\n') {
        fullMessage += "\r\n";
    }
    for (const auto &session : players) {
        if (session && session != sender) {
            session->queueMessage(fullMessage);
        }
    }
}

const std::vector<std::shared_ptr<Session>> &Room::getPlayers() const {
    return players;
}

const std::vector<std::shared_ptr<AttackableEntity>> &Room::getEntities() const {
    return entities;
}
