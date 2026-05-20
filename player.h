#ifndef PLAYER_H
#define PLAYER_H

#include <string>
#include <queue>
#include "attackable_entity.h"

class Player : public AttackableEntity {
public:
    using AttackableEntity::AttackableEntity; // Inherit constructor
    ~Player();
    std::string greet() const;
    void setGreeting(std::string greeting);
private:
    std::string greeting;
};

#endif // PLAYER_H