#!/usr/bin/env bash
# Write a human-readable SQL dump of the database into snapshots/ (gitignored).
# Useful to eyeball what changed before committing the binary Boh.db. See docs/DATABASE.md.

set -euo pipefail

DB="${BOH_DB_PATH:-Boh.db}"
OUTDIR="snapshots"

[ -f "$DB" ] || { echo "error: database '$DB' does not exist" >&2; exit 1; }
mkdir -p "$OUTDIR"

OUT="$OUTDIR/dump-$(date +%Y%m%d-%H%M%S).sql"
sqlite3 "$DB" .dump > "$OUT"
echo "Wrote $OUT ($(wc -l < "$OUT" | tr -d ' ') lines)"