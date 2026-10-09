# BoH Librarian

## Note from the bot wrangler

This was created to scratch an exceptionally specific itch - note taking for [Book of Hours](https://bookofhours.game) (is good, you should buy, and don't forget to wishlist [Travelling at Night!](https://store.steampowered.com/app/2915730/Travelling_At_Night/)) 

This could have as easily been a spreadsheet, or even a notebook, but unfortunately for all concerned the advent of agentic coding tools means that even the most ham-fisted wannabe developer (me) can come up with something that, superficially, looks OK. 

I initially prompted pi to tell me about SQlite syntax when it occurred to me that I could do a bit more, and here we are. The mechanics are not thought through and the app has evolved rather than being designed, in a process that I suspect is depressingly familiar to anyone who has to tried to steer a software project through user acceptance. 

To pi's credit, it didn't tell me to piss off and storm off to the pub, which is both the saving grace and the ultimate downfall of these things.

I suspect literally no one else will have a use for this, but if you do and you can think of ways to improve them, by all means send me a PR (or have the agent do it, I'm not precious about this particular repo).

## From the bot

A native macOS companion app for recording findings while playing
[*Book of Hours*](https://bookofhours.game) (Weather Factory). Single-user, offline,
zero dependencies. Data lives in `Boh.db` (SQLite) at the repo root — one database,
any number of playthroughs. (The db file itself is dev-local and not tracked by git
as of 2026-10-09; your real playthrough data lives in Application Support.)

What it does: record books (with difficulty, language, contamination, lessons and
the memory each one yields), memories and their aspects, skills and their levels,
and free-form journal notes — all scoped to the current playthrough — plus a Reading
Helper that works out which of your recorded memories and skills could open a given
book.

```sh
cd app && swift run        # run it (needs Xcode/swift 6 toolchain, macOS 14+)
cd app && swift test       # 108 unit tests on in-memory databases
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
## Built with

Vibe coded end-to-end with the [pi coding agent](https://pi.dev) running GLM-5.3 and
GLM-5.3-flash (Z.ai) — aside from some minor copy changes, every line of this repo is
agent-written.

## License

Copyright © 2026 eps@epsiloneridani.io. This project is licensed under the
[GNU General Public License v3.0](LICENSE) — see [LICENSE](LICENSE) for the full terms.
