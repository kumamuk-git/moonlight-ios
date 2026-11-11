#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SonoBusReceiver SonoBusReceiver;

// Receives from the direct UDP port of one SonoBus instance on the streaming host.
SonoBusReceiver *SonoBusOpen(const char *host, uint16_t port, int sampleRate, int blockSize);
void SonoBusRender(SonoBusReceiver *receiver, float *interleavedStereo, int frames);
// Stop the audio callback before closing its receiver.
void SonoBusClose(SonoBusReceiver *receiver);

#ifdef __cplusplus
}
#endif
