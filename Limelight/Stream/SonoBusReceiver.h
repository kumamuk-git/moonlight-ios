#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SonoBusReceiver SonoBusReceiver;

// A 4096-byte AOO group/join packet uses 24 bytes for the OSC address,
// 8 for the typetags, 36 for the password digest and 1 for the name terminator.
enum { SonoBusMaxGroupNameBytes = 4096 - 24 - 8 - 36 - 1 };
typedef struct {
    const char *name;
    const char *password;
    const char *username;
    void (*onError)(const char *message);
} SonoBusGroup;

// In group mode host/port identify the connection server. Otherwise they identify the audio sender.
SonoBusReceiver *SonoBusOpen(const char *host, uint16_t port, int sampleRate, int blockSize, const SonoBusGroup *group);
void SonoBusRender(SonoBusReceiver *receiver, float *interleavedStereo, int frames);
// Stop the audio callback before closing its receiver.
void SonoBusClose(SonoBusReceiver *receiver);

#ifdef __cplusplus
}
#endif
