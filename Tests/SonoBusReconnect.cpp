// Run on Linux with: bash Tests/run-sonobus-reconnect.sh
#include "../Limelight/Stream/SonoBusReceiver.h"
#include "aoo/aoo_net.h"
#include <arpa/inet.h>
#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <poll.h>
#include <thread>
#include <unistd.h>

using namespace std::chrono_literals;

template<class Address>
static void direct(const char *host, Address &loopback, uint16_t &portField) {
    int sender = socket(reinterpret_cast<sockaddr *>(&loopback)->sa_family, SOCK_DGRAM, 0);
    auto addr = reinterpret_cast<sockaddr *>(&loopback);
    socklen_t length = sizeof(loopback);
    assert(bind(sender, addr, length) == 0);
    assert(getsockname(sender, addr, &length) == 0);
    const uint16_t destination = ntohs(portField);

    uint16_t saved = 0;
    auto reconnect = [&] {
        auto receiver = SonoBusOpen(host, destination, 48000, 256, nullptr, &saved);
        assert(receiver && saved);
        // Inspect the UDP source port at the other end, as a SonoBus sender would.
        for (;;) {
            pollfd pfd{sender, POLLIN, 0};
            assert(poll(&pfd, 1, 2000) == 1);
            char msg[1024]{};
            assert(recvfrom(sender, msg, sizeof(msg) - 1, 0, addr, &length) > 0);
            if (strstr(msg, "/invite")) {
                assert(ntohs(portField) == saved);
                break;
            }
        }
        SonoBusClose(receiver);
    };
    reconnect();
    const uint16_t original = saved;
    for (int i = 0; i < 5; ++i) {
        reconnect();
        assert(saved == original);
    }
    int blocker = socket(addr->sa_family, SOCK_DGRAM, 0);
    portField = htons(saved);
    assert(bind(blocker, addr, length) == 0);
    reconnect();
    assert(saved != original);
    const uint16_t replacement = saved;
    close(blocker);
    reconnect();
    assert(saved == replacement);
    close(sender);
}

struct Membership { int joins = 0, leaves = 0; };
static int32_t membership(void *context, const aoo_event **events, int32_t count) {
    auto &seen = *static_cast<Membership *>(context);
    for (int i = 0; i < count; ++i) {
        if (events[i]->type == AOONET_SERVER_GROUP_JOIN_EVENT || events[i]->type == AOONET_SERVER_GROUP_LEAVE_EVENT) {
            auto *e = reinterpret_cast<const aoonet_server_group_event *>(events[i]);
            assert(strcmp(e->user, "Moonlight-reconnect-test") == 0 && strcmp(e->group, "reconnect") == 0);
            if (e->type == AOONET_SERVER_GROUP_JOIN_EVENT) ++seen.joins; else ++seen.leaves;
            assert(seen.joins - seen.leaves == (e->type == AOONET_SERVER_GROUP_JOIN_EVENT ? 1 : 0));
        }
    }
    return count;
}

static void group() {
    int32_t error = 0;
    int port = 40000 + getpid() % 10000;
    auto server = aoonet_server_new(port, &error);
    assert(server);
    std::thread worker(aoonet_server_run, server);
    SonoBusGroup config{"reconnect", "test-password", "Moonlight-reconnect-test",
                        [](const char *error) { fprintf(stderr, "%s\n", error); abort(); }};
    Membership seen;
    auto waitFor = [&](int &counter, int expected) {
        auto deadline = std::chrono::steady_clock::now() + 5s;
        while (counter < expected) {
            assert(std::chrono::steady_clock::now() < deadline);
            aoonet_server_handle_events(server, membership, &seen);
            std::this_thread::sleep_for(5ms);
        }
    };
    uint16_t saved = 0;
    for (int i = 1; i <= 6; ++i) {
        auto lastPort = saved;
        auto receiver = SonoBusOpen("127.0.0.1", port, 48000, 256, &config, &saved);
        assert(receiver && saved && (!lastPort || saved == lastPort));
        waitFor(seen.joins, i);
        SonoBusClose(receiver); // Reopen immediately, without waiting for a leave notification.
    }
    waitFor(seen.leaves, 6);
    aoonet_server_quit(server);
    worker.join();
    aoonet_server_free(server);
}

int main() {
    sockaddr_in v4{};
    v4.sin_family = AF_INET;
    v4.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    direct("127.0.0.1", v4, v4.sin_port);
    sockaddr_in6 v6{};
    v6.sin6_family = AF_INET6;
    v6.sin6_addr = in6addr_loopback;
    direct("::1", v6, v6.sin6_port);
    group();
    puts("Passed IPv4/IPv6 reconnect, occupied port fallback, and six private group joins/leaves.");
}
