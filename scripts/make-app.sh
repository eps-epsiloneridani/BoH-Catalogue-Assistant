#!/usr/bin/env bash
# Package BoH Librarian as a double-clickable macOS app (no terminal needed).
# Roadmap Phase 6. Output: dist/BoH Librarian.app (gitignored).
#
# The packaged app resolves its database per D7 (no repo next to it):
#   ~/Library/Application Support/BoH Librarian/Boh.db — created on first launch.
# Populate it via Manage Playthroughs → "Import from Save…".

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP_NAME="BoH Librarian"
BUNDLE_ID="dev.bohlibrarian.BoHLibrarian"
VERSION="0.1.0"
DIST="$ROOT/dist"
APP="$DIST/$APP_NAME.app"

cd "$ROOT/app"

echo "Building release binary (Apple Silicon)…"
swift build -c release
BIN_PATH="$(swift build -c release --show-bin-path)"
BINARY="$BIN_PATH/BoHLibrarian"

rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS"
cp "$BINARY" "$APP/Contents/MacOS/$APP_NAME"

# The SPM resource bundle (bundled migrations) ships inside the app: without it the
# packaged app silently resolved migrations via the hardcoded .build-directory
# fallback - machine-specific, breaks anywhere the dev tree is absent.
BUNDLE="$BIN_PATH/BoHLibrarian_BoHLibrarianCore.bundle"
mkdir -p "$APP/Contents/Resources"
cp -R "$BUNDLE" "$APP/Contents/Resources/"
cp "$ROOT/scripts/assets/AppIcon.icns" "$APP/Contents/Resources/AppIcon.icns"


GIT_HASH="$(git -C "$ROOT" rev-parse --short HEAD)"
cat > "$APP/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key><string>${APP_NAME}</string>
    <key>CFBundleDisplayName</key><string>${APP_NAME}</string>
    <key>CFBundleIdentifier</key><string>${BUNDLE_ID}</string>
    <key>CFBundleExecutable</key><string>${APP_NAME}</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>CFBundleShortVersionString</key><string>${VERSION}</string>
    <key>CFBundleVersion</key><string>${GIT_HASH}</string>
    <key>LSMinimumSystemVersion</key><string>14.0</string>
    <key>NSHighResolutionCapable</key><true/>
    <key>CFBundleIconFile</key><string>AppIcon</string>
    <key>NSPrincipalClass</key><string>NSApplication</string>
    <key>LSApplicationCategoryType</key><string>public.app-category.productivity</string>
    <key>NSHumanReadableCopyright</key><string>Personal-use companion tool for Book of Hours (Weather Factory). Not affiliated.</string>
</dict>
</plist>
PLIST

echo "Ad-hoc code signing…"
codesign --force --deep --sign - "$APP" >/dev/null 2>&1

echo
echo "Packaged: $APP"
echo "Version:  $VERSION (build $GIT_HASH)"
echo "First launch creates its database at"
echo "  ~/Library/Application Support/BoH Librarian/Boh.db"
echo "(keep it in the Dock with right-click → Options → Keep in Dock, if you like)"