# BoH Librarian

A native macOS companion app for recording findings while playing
[*Book of Hours*](https://bookofhours.game) (Weather Factory). Single-user, offline,
zero dependencies. Data lives in `Boh.db` (SQLite) at the repo root and is versioned
with the repo.

- What the app will do → [`docs/GUI_PLAN.md`](docs/GUI_PLAN.md)
- Data model → [`docs/DATABASE.md`](docs/DATABASE.md)
- Game-mechanics reference → [`docs/GAME_MECHANICS.md`](docs/GAME_MECHANICS.md)
- Status & plan → [`docs/ROADMAP.md`](docs/ROADMAP.md)
- Working notes for agents/contributors → [`AGENTS.md`](AGENTS.md)

Maintenance from the terminal:

```sh
scripts/migrate.sh          # apply pending schema migrations
scripts/migrate.sh --status # show current/pending migrations
sqlite3 Boh.db              # inspect data directly
```