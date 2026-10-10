#!/usr/bin/env bash
# Qt build + full ctest run. Usage: scripts/qt-test.sh
set -euo pipefail
cd "$(dirname "$0")/.."
cmake -S qt -B qt/build > /tmp/qt-cfg.log 2>&1 || { tail -20 /tmp/qt-cfg.log; exit 1; }
cmake --build qt/build > /tmp/qt-build.log 2>&1 || { grep -i -m5 error /tmp/qt-build.log; exit 1; }
ctest --test-dir qt/build --output-on-failure "$@"
