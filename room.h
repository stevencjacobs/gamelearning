#ifndef ROOM_H
#define ROOM_H

#include <memory>
#include <string>
#include <vector>
#include "attackable_entity.h"
#include "session.h"

class Room {
public:
    Room(const std::string &description = "");

    const std::string &getDescription() const;
    void setDescription(const std::string &description);

    void addPlayer(const std::shared_ptr<Session> &session);
    void removePlayer(int fd);
    void broadcast(const std::string &message);
    void broadcastFrom(const std::string &sender, const std::string &message);
    void broadcastFrom(const std::shared_ptr<Session> &sender, const std::string &message);
    const std::vector<std::shared_ptr<Session>> &getPlayers() const;

    void addEntity(const std::shared_ptr<AttackableEntity> &entity);
    void removeEntity(const std::shared_ptr<AttackableEntity> &entity);
    const std::vector<std::shared_ptr<AttackableEntity>> &getEntities() const;

private:
    std::string description;
    std::vector<std::shared_ptr<Session>> players;
    std::vector<std::shared_ptr<AttackableEntity>> entities;
};

#endif // ROOM_H
