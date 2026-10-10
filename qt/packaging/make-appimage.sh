#!/usr/bin/env bash
# Package the Qt app as a self-contained AppImage (plan Task 18).
# Usage: scripts/make-appimage.sh   (from the repo root or anywhere)
# Requires: cmake, Qt 6 dev packages, linuxdeploy (downloaded on first run —
# CI caches it; offline runs can point LINUXDEPLOY at a local copy).
set -euo pipefail
cd "$(dirname "$0")/../.."   # repo root

ARCH="${ARCH:-x86_64}"
APPDIR="qt/dist/AppDir"
BUILD="qt/build-appimage"

# Memory discipline: unbounded -j$(nproc) OOM-crashed the dev machine
# (6.6 GB / 4 cores). Default to 2 jobs; override with BOH_BUILD_JOBS.
JOBS="${BOH_BUILD_JOBS:-2}"

cmake -S qt -B "$BUILD" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" -j"$JOBS"

rm -rf "$APPDIR"
mkdir -p "$APPDIR/usr/bin" "$APPDIR/usr/share/applications" "$APPDIR/usr/share/icons/hicolor/256x256/apps" "$APPDIR/usr/share/icons/hicolor/512x512/apps"
cp "$BUILD/boh-librarian" "$APPDIR/usr/bin/"
cp qt/packaging/boh-librarian.desktop "$APPDIR/usr/share/applications/"
cp scripts/assets/AppIcon.iconset/icon_256x256.png "$APPDIR/usr/share/icons/hicolor/256x256/apps/boh-librarian.png"
cp scripts/assets/AppIcon.iconset/icon_512x512.png "$APPDIR/usr/share/icons/hicolor/512x512/apps/boh-librarian.png"

# linuxdeploy bundles the Qt libraries/plugins (platforms, styles, iconengines).
LINUXDEPLOY="qt/dist/linuxdeploy-${ARCH}.AppImage"
if [ ! -x "$LINUXDEPLOY" ] && [ -n "${LINUXDEPLOY:-}" ]; then :; fi
if [ ! -x "$LINUXDEPLOY" ]; then
  if [ -n "${LINUXDEPLOY_LOCAL:-}" ] && [ -x "${LINUXDEPLOY_LOCAL}" ]; then
    cp "$LINUXDEPLOY_LOCAL" "$LINUXDEPLOY"
  else
    echo "downloading linuxdeploy (set LINUXDEPLOY_LOCAL to skip)"
    curl -L -o "$LINUXDEPLOY" \
      "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-${ARCH}.AppImage"
    chmod +x "$LINUXDEPLOY"
  fi
fi

"$LINUXDEPLOY" --appimage-extract-and-run --appdir "$APPDIR" --output appimage
mkdir -p dist
mv "BoH Librarian-${ARCH}.AppImage" dist/ 2>/dev/null || \
  mv ./*.AppImage dist/ 2>/dev/null || true
echo "AppImage at dist/"
