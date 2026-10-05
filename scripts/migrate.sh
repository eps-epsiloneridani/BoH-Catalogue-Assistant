#!/usr/bin/env bash
# Apply pending migrations from db/migrations/ to the database (default ./Boh.db,
# override with BOH_DB_PATH). Migrations are forward-only, numbered NNN_*.sql, and each
# sets its own PRAGMA user_version inside a transaction. See docs/DATABASE.md.
#
#   scripts/migrate.sh            apply pending migrations
#   scripts/migrate.sh --status   show current version and per-file applied/pending

set -euo pipefail

DB="${BOH_DB_PATH:-Boh.db}"
MIGDIR="$(cd "$(dirname "$0")/../db/migrations" && pwd)"

command -v sqlite3 >/dev/null 2>&1 || { echo "error: sqlite3 not found in PATH" >&2; exit 1; }
[ -f "$DB" ] || { echo "error: database '$DB' does not exist" >&2; exit 1; }

current=$(sqlite3 "$DB" "PRAGMA user_version;")

count_rows() { # table -> row count, or 0 if table absent
  sqlite3 "$DB" "SELECT COUNT(*) FROM $1;" 2>/dev/null || echo 0
}

if [ "${1:-}" = "--status" ]; then
  echo "Database : $DB"
  echo "Version  : $current"
  for f in "$MIGDIR"/*.sql; do
    n=$(basename "$f"); n="${n%%_*}"
    if [ "$n" -le "$current" ]; then s="applied"; else s="PENDING"; fi
    printf '%-8s %-40s %s\n' "$s" "$(basename "$f")" "(v$n)"
  done
  exit 0
fi

applied=0
for f in "$MIGDIR"/*.sql; do
  base=$(basename "$f")
  n="${base%%_*}"
  if [ "$n" -le "$current" ]; then continue; fi

  # Safety net for 001: it drops the legacy tables — refuse if they still hold data.
  if [ "$n" -eq 1 ]; then
    legacy=$(( $(count_rows Books) + $(count_rows Memories) ))
    if [ "$legacy" -gt 0 ]; then
      echo "error: legacy Books/Memories tables contain $legacy rows." >&2
      echo "       001 would drop them. Back up/export first, then edit 001." >&2
      exit 1
    fi
  fi

  echo "Applying $base ..."
  sqlite3 -bail -init /dev/null "$DB" < "$f"
  after=$(sqlite3 "$DB" "PRAGMA user_version;")
  if [ "$after" -lt "$n" ]; then
    echo "error: $base did not advance user_version to $n (still $after)" >&2
    exit 1
  fi
  current=$after
  applied=$((applied + 1))
done

echo "Schema at version $current ($applied migration(s) applied this run)."
sqlite3 "$DB" ".tables"