#include "game_server.h"
#include "combat.h"
#include <sodium.h>
#include <iostream>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <cstring>

GameServer::GameServer(int port)
    : port(port), server_fd(-1), epfd(-1), starterRoom("You are in the starter room. Here you can talk to people or attack a rat. Have fun."), player_store("textarpg.db"), events(MAX_EVENTS) {}

GameServer::~GameServer() {
    closeServer();
}

int GameServer::run() {
    if (sodium_init() < 0) {
        std::cerr << "Failed to initialize libsodium.\n";
        return 1;
    }

    if (!setupServer()) return 1;
    if (!player_store.open()) {
        std::cerr << "Failed to open player database.\n";
        return 1;
    }

    while (true) {
        bool haveConnectedPlayer = false;
        for (const auto &entry : stage) {
            if (entry.second == 4) {
                haveConnectedPlayer = true;
                break;
            }
        }
        int timeoutMs = haveConnectedPlayer ? 1000 : -1;

        int n = epoll_wait(epfd, events.data(), MAX_EVENTS, timeoutMs);
        if (n < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait");
            break;
        }

        if (n == 0) {
            processCombatTurns();
            flushPendingSends();
            continue;
        }

        for (int i = 0; i < n; ++i) {
            int fd = events[i].data.fd;
            uint32_t evts = events[i].events;
            if (fd == server_fd) {
                acceptNewClients();
            } else {
                handleClientEvent(fd, evts);
            }
        }
    }

    return 0;
}

bool GameServer::setupServer() {
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return false;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        closeServer();
        return false;
    }

    if (listen(server_fd, 4) < 0) {
        perror("listen");
        closeServer();
        return false;
    }

    std::cout << "TextARPG server listening on port " << port << "\r\n";

    epfd = epoll_create1(0);
    if (epfd < 0) {
        perror("epoll_create1");
        closeServer();
        return false;
    }

    int sflags = fcntl(server_fd, F_GETFL, 0);
    fcntl(server_fd, F_SETFL, sflags | O_NONBLOCK);

    epoll_event sev{};
    sev.events = EPOLLIN;
    sev.data.fd = server_fd;
    if (epoll_ctl(epfd, EPOLL_CTL_ADD, server_fd, &sev) < 0) {
        perror("epoll_ctl: server_fd");
        closeServer();
        return false;
    }

    return true;
}

void GameServer::closeServer() {
    if (server_fd >= 0) {
        close(server_fd);
        server_fd = -1;
    }
    if (epfd >= 0) {
        close(epfd);
        epfd = -1;
    }
}

bool GameServer::modifyEpollOut(int fd, bool want_out) {
    epoll_event ev{};
    ev.data.fd = fd;
    ev.events = EPOLLIN | EPOLLRDHUP | (want_out ? EPOLLOUT : 0);
    if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev) < 0) {
        if (errno == EINVAL) {
            return epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == 0;
        }
        return false;
    }
    return true;
}

void GameServer::acceptNewClients() {
    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) break;
            perror("accept");
            break;
        }
        int cflags = fcntl(client_fd, F_GETFL, 0);
        fcntl(client_fd, F_SETFL, cflags | O_NONBLOCK);

        epoll_event cev{};
        cev.data.fd = client_fd;
        cev.events = EPOLLIN | EPOLLRDHUP;
        if (epoll_ctl(epfd, EPOLL_CTL_ADD, client_fd, &cev) < 0) {
            perror("epoll_ctl: add client");
            close(client_fd);
            continue;
        }

        auto session = std::make_shared<Session>();
        session->fd = client_fd;
        session->player = std::make_shared<Player>(0, std::string(""), 0);
        clients[client_fd] = session;
        stage[client_fd] = 0;

        session->queueMessage("Welcome to TextARPG server!\r\n");
        session->queueMessage("Please enter your username:\r\n");
        auto res = session->trySendData();
        if (res == PENDING) modifyEpollOut(client_fd, true);
        if (res == ERROR) {
            closeClient(client_fd);
        }
    }
}

void GameServer::handleClientEvent(int fd, uint32_t evts) {
    auto it = clients.find(fd);
    if (it == clients.end()) {
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
        close(fd);
        return;
    }

    auto session = it->second;
    if (evts & (EPOLLHUP | EPOLLERR | EPOLLRDHUP)) {
        closeClient(fd);
        return;
    }

    if (evts & EPOLLIN) {
        char buf[512];
        while (true) {
            ssize_t r = recv(fd, buf, sizeof(buf), 0);
            if (r > 0) {
                session->buffer.append(buf, buf + r);
            } else if (r == 0) {
                closeClient(fd);
                break;
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) break;
                closeClient(fd);
                break;
            }
        }

        processClientLines(session);
    }

    if (evts & EPOLLOUT) {
        auto res = session->trySendData();
        if (res == COMPLETE) {
            modifyEpollOut(fd, false);
            if (stage[fd] == -1 && !session->hadDataToSend()) {
                closeClient(fd);
            }
        } else if (res == PENDING) {
            modifyEpollOut(fd, true);
        } else {
            closeClient(fd);
        }
    }
}

void GameServer::processClientLines(std::shared_ptr<Session> session) {
    int fd = session->fd;
    size_t pos;
    while ((pos = session->buffer.find('\n')) != std::string::npos) {
        std::string line = session->buffer.substr(0, pos);
        if (!line.empty() && line.back() == '\r') line.pop_back();
        session->buffer.erase(0, pos + 1);

        int st = stage[fd];
        if (st == 0) {
            std::string username = line;
            if (username.empty()) {
                session->queueMessage("Username cannot be empty. Please enter your username:\r\n");
                continue;
            }

            if (player_store.userExists(username)) {
                loginStates[fd] = LoginState{username, true, player_store.loadProfile(username)};
                stage[fd] = 1;
                session->queueMessage("Username found. Please enter your password:\r\n");
            } else {
                loginStates[fd] = LoginState{username, false, std::nullopt};
                stage[fd] = 1;
                session->queueMessage("New username. Please choose a password:\r\n");
            }
        } else if (st == 1) {
            auto itState = loginStates.find(fd);
            if (itState == loginStates.end()) {
                session->queueMessage("Internal login error. Please reconnect.\r\n");
                stage[fd] = -1;
                break;
            }
            auto &state = itState->second;
            if (line.empty()) {
                session->queueMessage("Password cannot be empty. Please enter your password:\r\n");
                continue;
            }

            if (state.existing) {
                if (!player_store.verifyPassword(state.username, line)) {
                    session->queueMessage("Invalid password. Please try again:\r\n");
                    continue;
                }

                if (!state.profile.has_value()) {
                    session->queueMessage("Unable to load account data. Please reconnect later.\r\n");
                    stage[fd] = -1;
                    break;
                }

                auto existingPlayer = std::make_shared<Player>(state.profile->health, state.profile->name, state.profile->attack_power);
                existingPlayer->setGreeting(state.profile->greeting);
                session->player = existingPlayer;
                manager.addPlayer(session);
                session->queueMessage("Welcome back, " + session->player->getName() + "!\r\n");
            } else {
                PlayerProfile profile{state.username, 100, 1, ""};
                if (!player_store.createUser(profile, line)) {
                    session->queueMessage("Unable to create account. Please try again later.\r\n");
                    stage[fd] = -1;
                    break;
                }
                session->player->setName(state.username);
                session->player->setHealth(100);
                session->player->setAttackPower(1);
                session->player->setGreeting("");
                manager.addPlayer(session);
                session->queueMessage("Account created. Welcome, " + session->player->getName() + "!\r\n");
            }

            loginStates.erase(itState);
            session->queueMessage("Your character has " + std::to_string(session->player->getHealth()) + " health and " + std::to_string(session->player->getAttackPower()) + " attack power.\r\n");
            stage[fd] = 4;
            session->queueMessage("You can type 'quit' to exit.\r\n");
            starterRoom.addPlayer(session);
            session->queueMessage(starterRoom.getDescription() + "\r\n");
            ensureRat();
        } else if (st == 4) {
            handleCommand(session, line);
        }

        auto sendRes = session->trySendData();
        if (sendRes == PENDING) modifyEpollOut(fd, true);
        if (sendRes == ERROR) {
            closeClient(fd);
            break;
        }

        if (stage[fd] == -1) {
            if (!session->hadDataToSend()) {
                closeClient(fd);
                break;
            } else {
                modifyEpollOut(fd, true);
            }
        }
    }
}

void GameServer::handleCommand(const std::shared_ptr<Session> &session, const std::string &line) {
    int fd = session->fd;
    auto player = session->player;
    if (!player) {
        session->queueMessage("Internal error: player session invalid.\r\n");
        stage[fd] = -1;
        return;
    }

    if (line == "quit") {
        session->queueMessage("Goodbye!\r\n");
        stage[fd] = -1;
        return;
    }

    if (line == "/look") {
        session->queueMessage(starterRoom.getDescription() + "\r\n");
        session->queueMessage("Players here:\r\n");
        const auto &roomPlayers = starterRoom.getPlayers();
        if (roomPlayers.empty()) {
            session->queueMessage("  (none)\r\n");
        } else {
            for (const auto &roomSession : roomPlayers) {
                if (roomSession && roomSession->player) {
                    session->queueMessage("  " + roomSession->player->getName() + "\r\n");
                }
            }
        }
        session->queueMessage("Entities here:\r\n");
        const auto &roomEntities = starterRoom.getEntities();
        if (roomEntities.empty()) {
            session->queueMessage("  (none)\r\n");
        } else {
            for (const auto &entity : roomEntities) {
                if (entity) {
                    session->queueMessage("  " + entity->getName() + " (HP: " + std::to_string(entity->getHealth()) + ", ATK: " + std::to_string(entity->getAttackPower()) + ")\r\n");
                }
            }
        }
        return;
    }

    if (line.rfind("/say ", 0) == 0) {
        std::string message = line.substr(5);
        starterRoom.broadcastFrom(session, message);
        session->queueMessage("You say: " + message + "\r\n");
        return;
    }

    if (line.rfind("/attack ", 0) == 0) {
        std::string target = line.substr(8);
        if (target.empty()) {
            session->queueMessage("Attack what? Usage: /attack <target>\r\n");
            return;
        }

        std::shared_ptr<AttackableEntity> enemy;
        for (const auto &entity : starterRoom.getEntities()) {
            if (entity && entity->getName() == target) {
                enemy = entity;
                break;
            }
        }
        if (!enemy) {
            session->queueMessage("No target named '" + target + "' here.\r\n");
            return;
        }

        Combat combat(player.get(), enemy.get());
        bool combatContinues = combat.combatTurn();
        session->queueMessage("You attack " + enemy->getName() + " for " + std::to_string(player->getAttackPower()) + " damage.\r\n");
        if (enemy->getHealth() <= 0) {
            session->queueMessage("You have defeated " + enemy->getName() + "!\r\n");
            starterRoom.broadcastFrom(session, enemy->getName() + " has been defeated.");
            starterRoom.removeEntity(enemy);
            activeCombat.erase(fd);
            return;
        }

        if (!combatContinues) {
            session->queueMessage("You were defeated by " + enemy->getName() + "!\r\n");
            stage[fd] = -1;
            activeCombat.erase(fd);
            return;
        }

        session->queueMessage(enemy->getName() + " attacks you back for " + std::to_string(enemy->getAttackPower()) + " damage.\r\n");
        if (player->getHealth() <= 0) {
            session->queueMessage("You have been defeated by " + enemy->getName() + "!\r\n");
            stage[fd] = -1;
            activeCombat.erase(fd);
            return;
        }

        activeCombat[fd] = CombatState{std::make_unique<Combat>(player.get(), enemy.get()), enemy, session};
        return;
    }

    session->queueMessage("Echo: " + line + "\r\n");
}

void GameServer::closeClient(int fd) {
    epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
    auto it = clients.find(fd);
    if (it != clients.end()) {
        if (it->second && it->second->player) {
            player_store.savePlayer(it->second->player);
        }
        manager.removePlayer(fd);
        starterRoom.removePlayer(fd);
        clients.erase(it);
    }
    stage.erase(fd);
    activeCombat.erase(fd);
}

void GameServer::processCombatTurns() {
    std::vector<int> finished;
    for (auto &entry : activeCombat) {
        int fd = entry.first;
        auto &combatState = entry.second;
        auto itClient = clients.find(fd);
        if (itClient == clients.end()) {
            finished.push_back(fd);
            continue;
        }
        auto session = itClient->second;
        if (!combatState.enemy) {
            finished.push_back(fd);
            continue;
        }

        bool combatContinues = combatState.combat->combatTurn();
        auto enemy = combatState.enemy;
        session->queueMessage("You attack " + enemy->getName() + " for " + std::to_string(session->player->getAttackPower()) + " damage.\r\n");
        if (enemy->getHealth() <= 0) {
            session->queueMessage("You have defeated " + enemy->getName() + "!\r\n");
            starterRoom.broadcastFrom(session, enemy->getName() + " has been defeated.");
            starterRoom.removeEntity(enemy);
            finished.push_back(fd);
            continue;
        }

        if (!combatContinues) {
            session->queueMessage("You were defeated by " + enemy->getName() + "!\r\n");
            stage[fd] = -1;
            finished.push_back(fd);
            continue;
        }

        session->queueMessage(enemy->getName() + " attacks you back for " + std::to_string(enemy->getAttackPower()) + " damage.\r\n");
        if (session->player->getHealth() <= 0) {
            session->queueMessage("You have been defeated by " + enemy->getName() + "!\r\n");
            stage[fd] = -1;
            finished.push_back(fd);
        }
    }
    for (int fd : finished) {
        activeCombat.erase(fd);
    }
}

void GameServer::flushPendingSends() {
    std::vector<int> needCleanup;
    for (const auto &entry : stage) {
        int fd = entry.first;
        if (entry.second != 4) continue;
        auto itClient = clients.find(fd);
        if (itClient == clients.end()) continue;
        auto session = itClient->second;
        if (!session->hadDataToSend()) continue;

        auto res = session->trySendData();
        if (res == COMPLETE) {
            modifyEpollOut(fd, false);
        } else if (res == PENDING) {
            modifyEpollOut(fd, true);
        } else {
            needCleanup.push_back(fd);
        }
    }
    for (int fd : needCleanup) {
        closeClient(fd);
    }
}

void GameServer::ensureRat() {
    bool ratPresent = false;
    for (const auto &entity : starterRoom.getEntities()) {
        if (entity && entity->getName() == "Rat") {
            ratPresent = true;
            break;
        }
    }
    if (!ratPresent) {
        starterRoom.addEntity(std::make_shared<AttackableEntity>(10, "Rat", 1));
    }
}
