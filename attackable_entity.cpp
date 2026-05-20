#include "attackable_entity.h"

AttackableEntity::AttackableEntity(int health, std::string name, int attack_power) {
    this->health = health;
    this->name = name;
    this->attack_power = attack_power;
}

int AttackableEntity::getHealth() const {
    return health;
}

void AttackableEntity::setHealth(int health) {
    this->health = health;
}

std::string AttackableEntity::getName() const {
    return name;
}

void AttackableEntity::setName(std::string name) {
    this->name = name;
}

int AttackableEntity::getAttackPower() const {
    return attack_power;
}

void AttackableEntity::setAttackPower(int attack_power) {
    this->attack_power = attack_power;
}