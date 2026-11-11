#!/usr/bin/env bash
set -eu
cd "$(dirname "$0")/.."
test_bin=$(mktemp)
trap 'rm -f "$test_bin"' EXIT
aoo=ThirdParty/aoo
g++ -std=c++17 -pthread -I"$aoo/lib" -I"$aoo/deps" \
  Tests/SonoBusReconnect.cpp Limelight/Stream/SonoBusReceiver.cpp \
  "$aoo"/lib/src/{client,server,source,sink,common,net_utils,sync,time,codec_pcm}.cpp \
  "$aoo/deps/md5/md5.c" "$aoo"/deps/oscpack/osc/{OscOutboundPacketStream,OscReceivedElements,OscTypes}.cpp \
  -o "$test_bin"
timeout 50 "$test_bin"
