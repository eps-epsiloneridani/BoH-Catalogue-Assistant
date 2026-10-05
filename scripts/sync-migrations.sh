#!/usr/bin/env bash
# Copy db/migrations/*.sql into the app package's bundled resources.
# Canonical source remains db/migrations/ (docs/DATABASE.md §Migrations, D5/D7).
# The bundle copy is the Migrator's fallback when the repo directory isn't present
# (e.g. a packaged .app). Run this whenever db/migrations/ changes.

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SRC="$ROOT/db/migrations"
DEST="$ROOT/app/Sources/BoHLibrarianCore/Resources/Migrations"

[ -d "$SRC" ] || { echo "error: $SRC does not exist" >&2; exit 1; }
mkdir -p "$DEST"
rm -f "$DEST"/*.sql

count=0
for f in "$SRC"/*.sql; do
  cp "$f" "$DEST/"
  count=$((count + 1))
done

echo "Synced $count migration file(s) -> app/Sources/BoHLibrarianCore/Resources/Migrations"