#ifndef PLAYER_STORE_H
#define PLAYER_STORE_H

#include <memory>
#include <optional>
#include <string>
#include "player.h"
#include <sqlite3.h>

struct PlayerProfile {
    std::string name;
    int health;
    int attack_power;
    std::string greeting;
};

class PlayerStore {
public:
    explicit PlayerStore(const std::string &dbPath);
    ~PlayerStore();

    bool open();
    bool userExists(const std::string &username);
    std::optional<PlayerProfile> loadProfile(const std::string &username);
    bool createUser(const PlayerProfile &profile, const std::string &password);
    bool verifyPassword(const std::string &username, const std::string &password);
    bool savePlayer(const std::shared_ptr<Player> &player);

private:
    bool ensureSchema();
    bool executeSimple(const std::string &sql);
    std::string loadPasswordHash(const std::string &username);

    sqlite3 *db;
    std::string dbPath;
};

#endif // PLAYER_STORE_H
