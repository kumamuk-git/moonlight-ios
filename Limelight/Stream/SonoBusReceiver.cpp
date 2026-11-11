#include "SonoBusReceiver.h"
#include "aoo/aoo.h"
#include "aoo/aoo_net.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <list>
#include <memory>
#include <mutex>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

using Clock = std::chrono::steady_clock;

struct AudioPeer {
    SonoBusReceiver *receiver;
    sockaddr_in address;
    bool present;
};

struct SonoBusReceiver {
    int socket = -1;
    std::unique_ptr<aoo_sink, decltype(&aoo_sink_free)> sink{nullptr, aoo_sink_free};
    std::unique_ptr<aoonet_client, decltype(&aoonet_client_free)> client{nullptr, aoonet_client_free};
    std::thread clientThread;
    std::string groupName, password;
    void (*onError)(const char *) = nullptr;
    bool joined = false;
    // The sink retains endpoint pointers, including after an uninvite.
    std::list<AudioPeer> peers;
    AudioPeer *activePeer = nullptr;
    Clock::time_point nextInvite{};
    std::thread networkThread;
    std::atomic<bool> stopping{false};
    std::atomic<bool> mono{false};
    std::vector<float> left, right;

    ~SonoBusReceiver() {
        stopping = true;
        if (networkThread.joinable()) {
            networkThread.join();
        }
        if (clientThread.joinable()) {
            aoonet_client_quit(client.get());
            clientThread.join();
        }
        if (socket >= 0) {
            close(socket);
        }
    }
};

template<typename Address>
static uint16_t bindLocalPort(int fd, Address &address, uint16_t &portField, uint16_t preferred) {
    auto *addr = reinterpret_cast<sockaddr *>(&address);
    socklen_t length = sizeof(address);
    portField = htons(preferred);
    if (bind(fd, addr, length) < 0) {
        if (!preferred || errno != EADDRINUSE) {
            return 0;
        }
        // Another app has taken our previous port. Remember the replacement on success.
        portField = 0;
        if (bind(fd, addr, length) < 0) {
            return 0;
        }
    }
    return getsockname(fd, addr, &length) == 0 ? ntohs(portField) : 0;
}

static uint16_t bindLocalPort(int fd, int family, uint16_t preferred) {
    if (family == AF_INET) {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        return bindLocalPort(fd, addr, addr.sin_port, preferred);
    } else if (family == AF_INET6) {
        sockaddr_in6 addr{};
        addr.sin6_family = AF_INET6;
        return bindLocalPort(fd, addr, addr.sin6_port, preferred);
    }
    return 0;
}

static int connectSocket(const char *host, uint16_t port, uint16_t &localPort) {
    char service[6];
    snprintf(service, sizeof(service), "%u", (unsigned)port);
    addrinfo hints{}, *addresses;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_NUMERICSERV;
    if (getaddrinfo(host, service, &hints, &addresses) != 0) {
        return -1;
    }

    int fd = -1;
    for (auto *addr = addresses; addr; addr = addr->ai_next) {
        fd = socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
        if (fd >= 0) {
            uint16_t boundPort = bindLocalPort(fd, addr->ai_family, localPort);
            if (boundPort && connect(fd, addr->ai_addr, addr->ai_addrlen) == 0 &&
                fcntl(fd, F_SETFL, O_NONBLOCK) == 0) {
                localPort = boundPort;
                break;
            }
            close(fd);
            fd = -1;
        }
    }
    freeaddrinfo(addresses);
    return fd;
}

static int32_t sendPacket(void *endpoint, const char *data, int32_t size) {
    auto *peer = static_cast<AudioPeer *>(endpoint);
    auto *receiver = peer->receiver;
    if (peer != receiver->activePeer) {
        return 0;
    }
    if (receiver->client) {
        return (int32_t)sendto(receiver->socket, data, size, 0, (const sockaddr *)&peer->address, sizeof(peer->address));
    }
    return (int32_t)send(receiver->socket, data, size, 0);
}

static int32_t handleEvents(void *context, const aoo_event **events, int32_t count) {
    auto *receiver = static_cast<SonoBusReceiver *>(context);
    auto *sink = receiver->sink.get();
    for (int i = 0; i < count; ++i) {
        if (events[i]->type != AOO_SOURCE_ADD_EVENT && events[i]->type != AOO_SOURCE_FORMAT_EVENT) {
            continue;
        }
        auto *event = reinterpret_cast<const aoo_source_event *>(events[i]);
        if (event->endpoint != receiver->activePeer) {
            continue;
        }
        if (events[i]->type == AOO_SOURCE_ADD_EVENT && event->id != 0) {
            // SonoBus accepts the first invite on source 0, then assigns a peer-specific ID.
            aoo_sink_uninvite_source(sink, event->endpoint, 0, sendPacket);
            aoo_sink_invite_source(sink, event->endpoint, event->id, sendPacket);
        } else if (events[i]->type == AOO_SOURCE_FORMAT_EVENT) {
            aoo_format_storage format{};
            if (aoo_sink_get_source_format(sink, event->endpoint, event->id, &format)) {
                receiver->mono = format.header.nchannels == 1;
            }
        }
    }
    return count;
}

static bool sameAddress(const sockaddr_in &a, const sockaddr_in &b) {
    return a.sin_family == b.sin_family && a.sin_addr.s_addr == b.sin_addr.s_addr && a.sin_port == b.sin_port;
}

static void updateActivePeer(SonoBusReceiver *receiver) {
    if (receiver->activePeer && receiver->activePeer->present && receiver->joined) {
        return;
    }
    if (receiver->activePeer) {
        auto *sink = receiver->sink.get();
        aoo_sink_uninvite_all(sink);
        aoo_sink_send(sink);
        aoo_sink_reset(sink);
        receiver->activePeer = nullptr;
        receiver->mono = false;
    }
    if (receiver->joined) {
        for (auto &peer : receiver->peers) {
            if (peer.present) {
                receiver->activePeer = &peer;
                receiver->nextInvite = {};
                break;
            }
        }
    }
}

static int32_t handleGroupEvents(void *context, const aoo_event **events, int32_t count) {
    auto *receiver = static_cast<SonoBusReceiver *>(context);
    for (int i = 0; i < count; ++i) {
        auto *reply = reinterpret_cast<const aoonet_reply_event *>(events[i]);
        switch (reply->type) {
        case AOONET_CLIENT_CONNECT_EVENT:
            if (reply->result > 0) {
                aoonet_client_group_join(receiver->client.get(), receiver->groupName.c_str(), receiver->password.c_str());
            } else {
                receiver->onError("Couldn’t connect to the SonoBus server. Check your connection and restart the stream.");
            }
            break;
        case AOONET_CLIENT_GROUP_JOIN_EVENT:
            receiver->joined = reply->result > 0;
            if (!receiver->joined) {
                receiver->onError("Couldn’t join the SonoBus group. Check the group name and password in Stream Audio, then reconnect.");
            }
            break;
        case AOONET_CLIENT_PEER_JOIN_EVENT:
        case AOONET_CLIENT_PEER_LEAVE_EVENT: {
            auto *peer = reinterpret_cast<const aoonet_client_peer_event *>(events[i]);
            if (receiver->groupName != peer->group || !peer->address || peer->length < (int)sizeof(sockaddr_in)) {
                break;
            }
            const auto &address = *static_cast<const sockaddr_in *>(peer->address);
            if (address.sin_family != AF_INET) {
                break;
            }
            auto it = std::find_if(receiver->peers.begin(), receiver->peers.end(),
                [&](const AudioPeer &p) { return sameAddress(p.address, address); });
            bool present = peer->type == AOONET_CLIENT_PEER_JOIN_EVENT;
            if (it != receiver->peers.end()) {
                it->present = present;
            } else if (present) {
                receiver->peers.push_back({receiver, address, true});
            }
            break;
        }
        case AOONET_CLIENT_PEER_JOINFAIL_EVENT:
            if (!receiver->activePeer) {
                receiver->onError("Joined the SonoBus group, but couldn’t reach the other device over UDP.");
            }
            break;
        case AOONET_CLIENT_DISCONNECT_EVENT:
            receiver->joined = false;
            for (auto &peer : receiver->peers) {
                peer.present = false;
            }
            receiver->onError("Disconnected from the SonoBus server. Reconnect the stream to join again.");
            break;
        default:
            continue;
        }
        updateActivePeer(receiver);
    }
    return count;
}

static int32_t sendGroupPacket(void *context, const char *data, int32_t size, void *address) {
    auto *receiver = static_cast<SonoBusReceiver *>(context);
    return (int32_t)sendto(receiver->socket, data, size, 0, static_cast<sockaddr *>(address), sizeof(sockaddr_in));
}

static bool connectGroup(SonoBusReceiver *receiver, const char *host, uint16_t port,
                         const SonoBusGroup &group, uint16_t &localPort) {
    receiver->socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (receiver->socket < 0) {
        return false;
    }
    localPort = bindLocalPort(receiver->socket, AF_INET, localPort);
    if (!localPort || fcntl(receiver->socket, F_SETFL, O_NONBLOCK) < 0) {
        return false;
    }
    receiver->groupName = group.name;
    receiver->password = group.password;
    receiver->onError = group.onError;
    receiver->client.reset(aoonet_client_new(receiver, sendGroupPacket, localPort));
    if (!receiver->client || !aoonet_client_connect(receiver->client.get(), host, port, group.username, "")) {
        return false;
    }
    receiver->clientThread = std::thread(aoonet_client_run, receiver->client.get());
    return true;
}

static void receivePackets(SonoBusReceiver *receiver) {
    auto *sink = receiver->sink.get();
    pollfd fd{receiver->socket, POLLIN, 0};
    alignas(8) char packet[AOO_MAXPACKETSIZE + 4]{};
    while (!receiver->stopping) {
        if (receiver->client) {
            aoonet_client_handle_events(receiver->client.get(), handleGroupEvents, receiver);
            aoonet_client_send(receiver->client.get());
        }
        auto *peer = receiver->activePeer;
        if (peer && Clock::now() >= receiver->nextInvite) {
            aoo_sink_invite_source(sink, peer, 0, sendPacket);
            receiver->nextInvite = Clock::now() + std::chrono::seconds(1);
        }
        aoo_sink_handle_events(sink, handleEvents, receiver);
        aoo_sink_send(sink);

        if (poll(&fd, 1, 10) > 0) {
            // Consume pending UDP errors as well (e.g. SonoBus has not started yet).
            sockaddr_storage from{};
            socklen_t length = sizeof(from);
            auto size = recvfrom(fd.fd, packet, AOO_MAXPACKETSIZE + 1, 0, (sockaddr *)&from, &length);
            if (size <= 0 || size > AOO_MAXPACKETSIZE) {
                continue;
            }
            std::fill_n(packet + size, 3, '\0');
            int32_t type, id;
            if (receiver->client) {
                if (aoonet_parse_pattern(packet, (int32_t)size, &type)) {
                    aoonet_client_handle_message(receiver->client.get(), packet, (int32_t)size, &from);
                    continue;
                }
                if (!peer || !sameAddress(peer->address, reinterpret_cast<const sockaddr_in &>(from))) {
                    continue;
                }
            }
            if (aoo_parse_pattern(packet, (int32_t)size, &type, &id) && type == AOO_TYPE_SINK &&
                aoo_sink_handle_message(sink, packet, (int32_t)size, peer, sendPacket)) {
                receiver->nextInvite = Clock::now() + std::chrono::seconds(3);
            }
        }
    }
    aoo_sink_uninvite_all(sink);
    aoo_sink_send(sink);
}

SonoBusReceiver *SonoBusOpen(const char *host, uint16_t port, int sampleRate, int blockSize,
                            const SonoBusGroup *group, uint16_t *localPort) {
    if (!host || !port || !localPort || sampleRate <= 0 || blockSize <= 0) {
        return nullptr;
    }
    try {
        static std::once_flag initialized;
        std::call_once(initialized, aoo_initialize);
        auto receiver = std::make_unique<SonoBusReceiver>();
        uint16_t boundPort = *localPort;
        if (group) {
            if (!group->name || !*group->name || strlen(group->name) > SonoBusMaxGroupNameBytes ||
                !group->password || !group->username || !group->onError || !connectGroup(receiver.get(), host, port, *group, boundPort)) {
                return nullptr;
            }
        } else {
            receiver->socket = connectSocket(host, port, boundPort);
            if (receiver->socket < 0) {
                return nullptr;
            }
            receiver->peers.push_back({receiver.get(), {}, true});
            receiver->activePeer = &receiver->peers.front();
        }
        receiver->sink.reset(aoo_sink_new(1));
        if (!aoo_sink_setup(receiver->sink.get(), sampleRate, blockSize, 2)) {
            return nullptr;
        }
        aoo_sink_set_buffersize(receiver->sink.get(), 30); // milliseconds of receive jitter buffer
        int32_t flags = AOO_PROTOCOL_FLAG_COMPACT_DATA;
        aoo_sink_set_option(receiver->sink.get(), aoo_opt_protocol_flags, AOO_ARG(flags));
        receiver->left.resize(blockSize);
        receiver->right.resize(blockSize);
        receiver->networkThread = std::thread(receivePackets, receiver.get());
        *localPort = boundPort;
        return receiver.release();
    } catch (const std::exception &) {
        return nullptr;
    }
}

void SonoBusRender(SonoBusReceiver *receiver, float *stereo, int frames) {
    float *channels[] = {receiver->left.data(), receiver->right.data()};
    while (frames > 0) {
        int count = std::min(frames, (int)receiver->left.size());
        if (aoo_sink_process(receiver->sink.get(), channels, count, aoo_osctime_get())) {
            const float *right = receiver->mono ? channels[0] : channels[1];
            for (int i = 0; i < count; ++i) {
                stereo[2*i] = channels[0][i];
                stereo[2*i + 1] = right[i];
            }
        } else {
            std::fill_n(stereo, 2 * count, 0.0f);
        }
        frames -= count;
        stereo += 2 * count;
    }
}

void SonoBusClose(SonoBusReceiver *receiver) {
    delete receiver;
}
