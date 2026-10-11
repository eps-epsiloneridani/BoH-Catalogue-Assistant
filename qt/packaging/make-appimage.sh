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

# Qt plugins are dlopened at runtime — ldd can't see them, so linuxdeploy alone
# never ships the xcb platform plugin (the clean-Debian failure). Copy the ones
# the app needs into the AppDir BEFORE linuxdeploy runs: its ldd scan then also
# bundles the plugins' own deps (libxkbcommon-x11, libxcb-*, gtk3 for the style).
PLUGIN_DIR=""
for candidate in /usr/lib/x86_64-linux-gnu/qt6/plugins /usr/lib/qt6/plugins /usr/lib64/qt6/plugins; do
  if [ -d "$candidate/platforms" ]; then PLUGIN_DIR="$candidate"; break; fi
done
if [ -z "$PLUGIN_DIR" ]; then
  echo "Qt plugin dir not found — install qt6-base-dev" >&2
  exit 1
fi
mkdir -p "$APPDIR/usr/plugins/platforms" "$APPDIR/usr/plugins/styles" "$APPDIR/usr/plugins/imageformats"
cp "$PLUGIN_DIR/platforms/libqxcb.so" "$APPDIR/usr/plugins/platforms/"
# libqxcb's companion libQt6XcbQpa is NOT picked up by linuxdeploy's scan
# (found by the real-install failure: the plugin then resolved the SYSTEM
# XcbQpa — 6.8 — against our bundled 6.2 core → symbol-version crash).
# Ship it explicitly, UNDER ITS SONAME. Debian ships the real file as
# libQt6XcbQpa.so.6.2.4 with a libQt6XcbQpa.so.6 symlink; the dynamic loader
# matches DT_NEEDED against the FILE NAME, so a file named ...so.6.2.4 is
# invisible to a plugin needing "libQt6XcbQpa.so.6" — cp'ing the readlink -f
# target (the second bug) left the plugin reaching for the host's Qt again.
# cp the symlink path, not its target: the copy lands as libQt6XcbQpa.so.6.
XCBPA="$(dirname "$PLUGIN_DIR")/../libQt6XcbQpa.so.6"
if [ -f "$XCBPA" ]; then
  mkdir -p "$APPDIR/usr/lib"
  cp "$XCBPA" "$APPDIR/usr/lib/libQt6XcbQpa.so.6"
else
  echo "warning: libQt6XcbQpa.so.6 not found next to $PLUGIN_DIR" >&2
fi
cp "$PLUGIN_DIR/styles/"libqgtk3.so "$APPDIR/usr/plugins/styles/" 2>/dev/null || true
cp "$PLUGIN_DIR/styles/"libqfusionstyle.so "$APPDIR/usr/plugins/styles/" 2>/dev/null || true
cp "$PLUGIN_DIR/imageformats/"libqjpeg.so "$APPDIR/usr/plugins/imageformats/" 2>/dev/null || true
cp "$PLUGIN_DIR/imageformats/"libqgif.so "$APPDIR/usr/plugins/imageformats/" 2>/dev/null || true

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

# Wrap AppRun BEFORE deployment — linuxdeploy preserves an existing AppRun.
# (The first attempt wrote it after --output appimage: the packed image shipped
# linuxdeploy's default AppRun with no QT_PLUGIN_PATH, so the bundled plugins
# were never found on clean systems.)
cat > "$APPDIR/AppRun" <<'APPRUN'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
export LD_LIBRARY_PATH="$HERE/usr/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$HERE/usr/plugins"
exec "$HERE/usr/bin/boh-librarian" "$@"
APPRUN
chmod +x "$APPDIR/AppRun"

"$LINUXDEPLOY" --appimage-extract-and-run --appdir "$APPDIR" --output appimage

mkdir -p dist
mv "BoH Librarian-${ARCH}.AppImage" dist/ 2>/dev/null || \
  mv ./*.AppImage dist/ 2>/dev/null || true
# linuxdeploy names the image after the .desktop Name (underscores), so match
# whatever it actually produced rather than guessing the spelling.
OUT="$(ls dist/*-${ARCH}.AppImage 2>/dev/null | head -1)"
if [ -z "$OUT" ]; then
  echo "ERROR: linuxdeploy produced no AppImage in dist/" >&2
  exit 1
fi
OUT="$(realpath "$OUT")"

# Self-check for the failure class that shipped twice: a Qt lib resolving to
# the HOST instead of the bundle (host Qt 6.8 + bundled Qt 6.2 = plugin load
# dies). Extract the finished image and ldd the binary and the xcb plugin with
# the bundle's LD_LIBRARY_PATH; every Qt6 lib must come from inside the image.
VERIFY="$(mktemp -d)"
( cd "$VERIFY" && "$OUT" --appimage-extract >/dev/null 2>&1 )
if [ ! -f "$VERIFY/squashfs-root/usr/bin/boh-librarian" ]; then
  echo "ERROR: self-check could not extract $OUT" >&2
  rm -rf "$VERIFY"
  exit 1
fi
LEAK="$(LD_LIBRARY_PATH="$VERIFY/squashfs-root/usr/lib" \
  ldd "$VERIFY/squashfs-root/usr/bin/boh-librarian" \
      "$VERIFY/squashfs-root/usr/plugins/platforms/libqxcb.so" 2>/dev/null \
  | grep 'Qt6' | grep -v "squashfs-root/usr/lib" || true)"
rm -rf "$VERIFY"
if [ -n "$LEAK" ]; then
  echo "ERROR: the packaged AppImage resolves Qt libraries to the host:" >&2
  echo "$LEAK" >&2
  exit 1
fi
echo "AppImage at dist/ (Qt libs self-contained: verified)"
