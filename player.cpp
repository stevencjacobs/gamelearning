#include "player.h"

Player::~Player() = default;

std::string Player::greet() const {
    return greeting;
}

void Player::setGreeting(std::string greeting) {
    this->greeting = greeting;
}
