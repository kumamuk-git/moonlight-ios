#include "SonoBusReceiver.h"
#include "aoo/aoo.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <fcntl.h>
#include <memory>
#include <mutex>
#include <netdb.h>
#include <poll.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <vector>

struct SonoBusReceiver {
    int socket = -1;
    std::unique_ptr<aoo_sink, decltype(&aoo_sink_free)> sink{nullptr, aoo_sink_free};
    std::thread networkThread;
    std::atomic<bool> stopping{false};
    std::atomic<bool> mono{false};
    std::vector<float> left, right;

    ~SonoBusReceiver() {
        stopping = true;
        if (networkThread.joinable()) {
            networkThread.join();
        }
        if (socket >= 0) {
            close(socket);
        }
    }
};

static int connectSocket(const char *host, uint16_t port) {
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
            if (connect(fd, addr->ai_addr, addr->ai_addrlen) == 0 &&
                fcntl(fd, F_SETFL, O_NONBLOCK) == 0) {
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
    auto *receiver = static_cast<SonoBusReceiver *>(endpoint);
    return (int32_t)send(receiver->socket, data, size, 0);
}

static int32_t handleEvents(void *endpoint, const aoo_event **events, int32_t count) {
    auto *receiver = static_cast<SonoBusReceiver *>(endpoint);
    auto *sink = receiver->sink.get();
    for (int i = 0; i < count; ++i) {
        if (events[i]->type == AOO_SOURCE_ADD_EVENT) {
            auto *event = reinterpret_cast<const aoo_source_event *>(events[i]);
            if (event->id != 0) {
                // SonoBus accepts the first invite on source 0, then assigns a peer-specific ID.
                aoo_sink_uninvite_source(sink, endpoint, 0, sendPacket);
                aoo_sink_invite_source(sink, endpoint, event->id, sendPacket);
            }
        } else if (events[i]->type == AOO_SOURCE_FORMAT_EVENT) {
            auto *event = reinterpret_cast<const aoo_source_event *>(events[i]);
            aoo_format_storage format{};
            if (aoo_sink_get_source_format(sink, endpoint, event->id, &format)) {
                receiver->mono = format.header.nchannels == 1;
            }
        }
    }
    return count;
}

static void receivePackets(SonoBusReceiver *receiver) {
    using Clock = std::chrono::steady_clock;
    auto *sink = receiver->sink.get();
    auto nextInvite = Clock::now();
    pollfd fd{receiver->socket, POLLIN, 0};
    alignas(8) char packet[AOO_MAXPACKETSIZE + 4]{};
    while (!receiver->stopping) {
        if (Clock::now() >= nextInvite) {
            aoo_sink_invite_source(sink, receiver, 0, sendPacket);
            nextInvite = Clock::now() + std::chrono::seconds(1);
        }
        aoo_sink_handle_events(sink, handleEvents, receiver);
        aoo_sink_send(sink);

        if (poll(&fd, 1, 10) > 0) {
            // Consume pending UDP errors as well (e.g. SonoBus has not started yet).
            auto size = recv(fd.fd, packet, AOO_MAXPACKETSIZE + 1, 0);
            if (size <= 0 || size > AOO_MAXPACKETSIZE) {
                continue;
            }
            std::fill_n(packet + size, 3, '\0');
            int32_t type, id;
            if (aoo_parse_pattern(packet, (int32_t)size, &type, &id) && type == AOO_TYPE_SINK &&
                aoo_sink_handle_message(sink, packet, (int32_t)size, receiver, sendPacket)) {
                nextInvite = Clock::now() + std::chrono::seconds(3);
            }
        }
    }
    aoo_sink_uninvite_all(sink);
    aoo_sink_send(sink);
}

SonoBusReceiver *SonoBusOpen(const char *host, uint16_t port, int sampleRate, int blockSize) {
    if (!host || !port || sampleRate <= 0 || blockSize <= 0) {
        return nullptr;
    }
    try {
        static std::once_flag initialized;
        std::call_once(initialized, aoo_initialize);
        auto receiver = std::make_unique<SonoBusReceiver>();
        receiver->socket = connectSocket(host, port);
        if (receiver->socket < 0) {
            return nullptr;
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
