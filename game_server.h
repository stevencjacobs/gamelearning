#ifndef GAME_SERVER_H
#define GAME_SERVER_H

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <sys/epoll.h>
#include "player.h"
#include "player_manager.h"
#include "room.h"
#include "combat.h"
#include "player_store.h"
#include "session.h"

class GameServer {
public:
    explicit GameServer(int port);
    ~GameServer();
    int run();

private:
    int port;
    int server_fd;
    int epfd;
    PlayerManager manager;
    Room starterRoom;
    PlayerStore player_store;
    std::unordered_map<int,std::shared_ptr<Session>> clients;
    std::unordered_map<int,int> stage; // 0=username,1=password,4=connected/idle
    struct LoginState {
        std::string username;
        bool existing = false;
        std::optional<PlayerProfile> profile;
    };
    std::unordered_map<int, LoginState> loginStates;

    struct CombatState {
        std::unique_ptr<Combat> combat;
        std::shared_ptr<AttackableEntity> enemy;
        std::shared_ptr<Session> session;
    };
    std::unordered_map<int, CombatState> activeCombat;

    static constexpr int MAX_EVENTS = 32;
    std::vector<epoll_event> events;

    bool setupServer();
    void closeServer();
    bool modifyEpollOut(int fd, bool want_out);
    void acceptNewClients();
    void handleClientEvent(int fd, uint32_t evts);
    void processClientLines(std::shared_ptr<Session> session);
    void handleCommand(const std::shared_ptr<Session> &session, const std::string &line);
    void processCombatTurns();
    void closeClient(int fd);
    void flushPendingSends();
    void ensureRat();
};

#endif // GAME_SERVER_H
