#include "combat.h"

Combat::Combat(AttackableEntity* player, AttackableEntity* enemy) : player(player), enemy(enemy) {}

void Combat::attack(AttackableEntity& attacker, AttackableEntity& defender) {
    int damage = attacker.getAttackPower();
    int new_health = defender.getHealth() - damage;
    defender.setHealth(new_health);
}

bool Combat::combatTurn() {
    //Player attacks enemy
    attack(*player, *enemy);
   // Check if enemy is defeated
    if (enemy->getHealth() <= 0) {
         return false; // Combat ends, player wins
    }
    //Enemy attacks player
    attack(*enemy, *player);
    // Check if player is defeated
    if (player->getHealth() <= 0) {
        return false; // Combat ends, enemy wins
    }
    return true; // Return true if combat continues, false if it ends
}