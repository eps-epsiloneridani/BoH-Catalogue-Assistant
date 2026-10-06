# BoH Librarian

A native macOS companion app for recording findings while playing
[*Book of Hours*](https://bookofhours.game) (Weather Factory). Single-user, offline,
zero dependencies. Data lives in `Boh.db` (SQLite) at the repo root and is versioned
with the repo — one database, any number of playthroughs.

What it does: record books (with difficulty, language, contamination, lessons and
the memory each one yields), memories and their aspects, skills and their levels,
and free-form journal notes — all scoped to the current playthrough — plus a Reading
Helper that works out which of your recorded memories and skills could open a given
book.

```sh
cd app && swift run        # run it (needs Xcode/swift 6 toolchain, macOS 14+)
cd app && swift test       # 78 unit tests on in-memory databases
```

- What the app does & how it's built → [`docs/GUI_PLAN.md`](docs/GUI_PLAN.md)
- Data model → [`docs/DATABASE.md`](docs/DATABASE.md)
- Game-mechanics reference → [`docs/GAME_MECHANICS.md`](docs/GAME_MECHANICS.md)
- Status & plan → [`docs/ROADMAP.md`](docs/ROADMAP.md)
- Hands-on test checklist → [`docs/MANUAL_TEST.md`](docs/MANUAL_TEST.md)
- Working notes for agents/contributors → [`AGENTS.md`](AGENTS.md)

Maintenance from the terminal:

```sh
scripts/migrate.sh          # apply pending schema migrations
scripts/migrate.sh --status # show current/pending migrations
sqlite3 Boh.db              # inspect data directly
```