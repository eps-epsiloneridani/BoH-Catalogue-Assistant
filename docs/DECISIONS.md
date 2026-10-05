# Decisions

Short log of the decisions that shape this project. Each entry: decision, rationale,
revisit trigger. Reference these as D1…D8 in other docs.

| # | Decision | Rationale | Revisit if |
|---|---|---|---|
| D1 | The database is `Boh.db` in the repo root, **committed to git** | Single user; git gives free versioning/backup of playthrough data; one obvious file | Multiple machines or huge size (>50 MB) |
| D2 | **No external Swift dependencies**; hand-written thin wrapper over the system `SQLite3` C API | `swift build` must work offline; this is a small CRUD app; SQLite C API is stable | Wrapper grows past ~300 lines or we need real relational mapping — then consider GRDB |
| D3 | **Aspects/principles are rows, not columns** (junction tables `MemoryAspects`, plus FK columns on `Books`/`Skills`) | The principle set has already changed once in-game (Numa added Rose and Moon), and the legacy schema's 11 fixed columns missed **Lantern and Nectar** (13 principles exist). Row-based aspects need no ALTER to extend | Never for v2; a column-per-principle design is explicitly rejected |
| D4 | **SwiftUI + Swift Package Manager**, no `.xcodeproj`; library target + thin executable target; Swift **5 language mode**; macOS 14+ minimum | Keeps the repo text-only and agent-friendly; @Observable needs macOS 14; Swift 5 mode avoids strict-concurrency friction for a UI app that confines DB access to the main thread | Project needs Xcode-only features (rare) or Xcode templates for Share/Menu extensions |
| D5 | **Migrations are numbered `.sql` files** in `db/migrations/`, forward-only, tracked by `PRAGMA user_version`; app applies them on launch (repo dir first, bundled copy as fallback) | Same schema source for CLI (`sqlite3`/scripts) and app; SQL files are reviewable in git | — |
| D6 | **Seed only structural data** (principles, languages). Books/skills/memories are never pre-seeded | Recording discoveries is the entire point of the app; pre-seeding all 280+ books would spoil the playthrough. Wiki import can be an *opt-in* tool much later | User explicitly asks for a full data import |
| D7 | App resolves the DB path as: `BOH_DB_PATH` env var → `./Boh.db` → `../Boh.db` (covers `swift run` from repo root **or** from `app/`) → `~/Library/Application Support/BoH Librarian/Boh.db` (created; packaged-app case). Migrations: `BOH_MIGRATIONS` → `./db/migrations` → `../db/migrations` → bundled copy | Dev runs via `swift run` use the repo db from either cwd; a packaged `.app` still works standalone | — |
| D8 | Legacy two-table schema (`Books`, `Memories`, 11 principle columns) was **dropped** by migration 001 rather than transformed | The db was verified empty (0 rows in both tables); the original DDL is preserved in git history (pre-migration commit) and in this doc set | N/A — one-time decision, recorded here |

Note on D8: the legacy schema was a reasonable first sketch and correctly anticipated the
core entity pair (books ↔ yielded memory). What it couldn't survive was the discovery
that the principle set is 13 (missing columns) and the need for many-to-many aspects.