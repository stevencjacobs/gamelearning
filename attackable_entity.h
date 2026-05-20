#ifndef ATTACKABLE_ENTITY_H
#define ATTACKABLE_ENTITY_H
#include <string>
class AttackableEntity {
public:
    public:
    AttackableEntity(int health, std::string name, int attack_power);
    int getHealth() const;
    void setHealth(int health);
    std::string getName() const;
    void setName(std::string name);
    int getAttackPower() const;
    void setAttackPower(int attack_power);
private:
    int health;
    int attack_power;
    std::string name;
};

#endif // ATTACKABLE_ENTITY_H