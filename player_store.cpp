#include "player_store.h"
#include <iostream>
#include <sodium.h>

PlayerStore::PlayerStore(const std::string &dbPath)
    : db(nullptr), dbPath(dbPath) {
}

PlayerStore::~PlayerStore() {
    if (db) {
        sqlite3_close(db);
        db = nullptr;
    }
}

bool PlayerStore::open() {
    if (sqlite3_open(dbPath.c_str(), &db) != SQLITE_OK) {
        std::cerr << "Unable to open SQLite database '" << dbPath << "': " << sqlite3_errmsg(db) << "\n";
        if (db) sqlite3_close(db);
        db = nullptr;
        return false;
    }
    return ensureSchema();
}

bool PlayerStore::executeSimple(const std::string &sql) {
    char *errMsg = nullptr;
    int rc = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQLite error: " << (errMsg ? errMsg : "unknown") << "\n";
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

bool PlayerStore::ensureSchema() {
    static const std::string createTableSql =
        "CREATE TABLE IF NOT EXISTS players ("
        "name TEXT PRIMARY KEY,"
        "password_hash TEXT NOT NULL,"
        "health INTEGER NOT NULL,"
        "attack_power INTEGER NOT NULL,"
        "greeting TEXT,"
        "last_room TEXT"
        ");";
    return executeSimple(createTableSql);
}

bool PlayerStore::userExists(const std::string &username) {
    const char *sql = "SELECT 1 FROM players WHERE name = ? LIMIT 1;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare exists statement: " << sqlite3_errmsg(db) << "\n";
        return false;
    }
    if (sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        std::cerr << "Failed to bind username for exists: " << sqlite3_errmsg(db) << "\n";
        sqlite3_finalize(stmt);
        return false;
    }

    bool exists = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return exists;
}

std::string PlayerStore::loadPasswordHash(const std::string &username) {
    const char *sql = "SELECT password_hash FROM players WHERE name = ?;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare password hash statement: " << sqlite3_errmsg(db) << "\n";
        return "";
    }
    if (sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        std::cerr << "Failed to bind username for password hash: " << sqlite3_errmsg(db) << "\n";
        sqlite3_finalize(stmt);
        return "";
    }

    std::string hash;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *text = sqlite3_column_text(stmt, 0);
        if (text) hash = reinterpret_cast<const char *>(text);
    }

    sqlite3_finalize(stmt);
    return hash;
}

bool PlayerStore::createUser(const PlayerProfile &profile, const std::string &password) {
    std::string passwordHash(crypto_pwhash_STRBYTES, '\0');
    if (crypto_pwhash_str(passwordHash.data(), password.c_str(), password.size(),
                          crypto_pwhash_OPSLIMIT_INTERACTIVE,
                          crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        std::cerr << "Password hashing failed: not enough memory or resources.\n";
        return false;
    }

    const char *sql =
        "INSERT INTO players (name, password_hash, health, attack_power, greeting, last_room) "
        "VALUES (?, ?, ?, ?, ?, ?);";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare createUser statement: " << sqlite3_errmsg(db) << "\n";
        return false;
    }

    if (sqlite3_bind_text(stmt, 1, profile.name.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 2, passwordHash.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_int(stmt, 3, profile.health) != SQLITE_OK ||
        sqlite3_bind_int(stmt, 4, profile.attack_power) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 5, profile.greeting.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 6, "starter", -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        std::cerr << "Failed to bind createUser parameters: " << sqlite3_errmsg(db) << "\n";
        sqlite3_finalize(stmt);
        return false;
    }

    bool result = sqlite3_step(stmt) == SQLITE_DONE;
    if (!result) {
        std::cerr << "Failed to create user: " << sqlite3_errmsg(db) << "\n";
    }
    sqlite3_finalize(stmt);
    return result;
}

bool PlayerStore::verifyPassword(const std::string &username, const std::string &password) {
    std::string hash = loadPasswordHash(username);
    if (hash.empty()) return false;
    return crypto_pwhash_str_verify(hash.c_str(), password.c_str(), password.size()) == 0;
}

std::optional<PlayerProfile> PlayerStore::loadProfile(const std::string &username) {
    const char *sql = "SELECT health, attack_power, greeting FROM players WHERE name = ?;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare load statement: " << sqlite3_errmsg(db) << "\n";
        return std::nullopt;
    }
    if (sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        std::cerr << "Failed to bind username for load: " << sqlite3_errmsg(db) << "\n";
        sqlite3_finalize(stmt);
        return std::nullopt;
    }

    std::optional<PlayerProfile> profile;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        profile = PlayerProfile{
            username,
            sqlite3_column_int(stmt, 0),
            sqlite3_column_int(stmt, 1),
            sqlite3_column_text(stmt, 2) ? reinterpret_cast<const char *>(sqlite3_column_text(stmt, 2)) : std::string()
        };
    }

    sqlite3_finalize(stmt);
    return profile;
}

bool PlayerStore::savePlayer(const std::shared_ptr<Player> &player) {
    const char *sql =
        "UPDATE players SET health = ?, attack_power = ?, greeting = ?, last_room = ? WHERE name = ?;";
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        std::cerr << "Failed to prepare save statement: " << sqlite3_errmsg(db) << "\n";
        return false;
    }

    if (sqlite3_bind_int(stmt, 1, player->getHealth()) != SQLITE_OK ||
        sqlite3_bind_int(stmt, 2, player->getAttackPower()) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 3, player->greet().c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 4, "starter", -1, SQLITE_TRANSIENT) != SQLITE_OK ||
        sqlite3_bind_text(stmt, 5, player->getName().c_str(), -1, SQLITE_TRANSIENT) != SQLITE_OK) {
        std::cerr << "Failed to bind save parameters: " << sqlite3_errmsg(db) << "\n";
        sqlite3_finalize(stmt);
        return false;
    }

    bool result = sqlite3_step(stmt) == SQLITE_DONE;
    if (!result) {
        std::cerr << "Failed to save player: " << sqlite3_errmsg(db) << "\n";
    }
    sqlite3_finalize(stmt);
    return result;
}
