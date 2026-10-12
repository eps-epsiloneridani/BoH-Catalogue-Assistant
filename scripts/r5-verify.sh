#!/usr/bin/env bash
# One-shot R5 verification: build, suite, then the leak/overlay probe across selections.
set -u
cd "$(dirname "$0")/.."
cmake --build qt/build > /tmp/r5-build.log 2>&1 || { grep -m4 -i error /tmp/r5-build.log; exit 1; }
ctest --test-dir qt/build 2>&1 | tail -1 || exit 1
g++ -std=c++20 -I qt/src/app -I qt/src/app/Views -I qt/src/core \
  $(pkg-config --cflags Qt6Widgets Qt6Gui Qt6Core) /tmp/overlay.cpp \
  qt/build/libbohapp.a qt/build/libbohcore.a \
  $(pkg-config --libs Qt6Widgets Qt6Gui Qt6Core) -lsqlite3 -o /tmp/overlay || exit 1
echo "== leak probe (children must NOT grow across selections) =="
BOH_DB_PATH=/tmp/boh-ui/Boh.db QT_QPA_PLATFORM=offscreen /tmp/overlay 2>&1 | grep -v propagate