#ifndef COMBAT_H
#define COMBAT_H
#include "attackable_entity.h"

class Combat {
public:
    Combat(AttackableEntity* player, AttackableEntity* enemy);
    static void attack(AttackableEntity& attacker, AttackableEntity& defender);
    bool combatTurn();
private:
    AttackableEntity* player;
    AttackableEntity* enemy;
};

#endif // COMBAT_H