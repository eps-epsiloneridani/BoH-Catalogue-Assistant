# Book of Hours — mechanics that matter to this app

Distilled from the [Book of Hours Wiki](https://book-of-hours.fandom.com/) and community
guides (sources at the bottom). This file records what the schema and UI are *built to
track*, plus open questions to verify during play. If play contradicts something here,
**fix this file first**, then adjust the app.

*Game era note: current as of the House of Light DLC / COSELEY update.*

## Principles (13)

Aspects that everything in the game carries. The original db schema had 11 columns and
**missed Lantern and Nectar** — that is why principles are a table now.

| Principle | Gloss (paraphrased) |
|---|---|
| Edge | battle, struggle, conquest |
| Forge | fire, transformation, destruction |
| Grail | hunger, lust, birth and the feast |
| Heart | continuation, preservation |
| Knock | openings; unseals every barrier |
| Lantern | illumination; the House of the Sun |
| Moon | the nocturnal, the forgotten (added with Numa) |
| Moth | chaos and yearning |
| Nectar | the green pulse of the seasons (once called Blood) |
| Rose | the rose that encompasseth all; new horizons (added with Numa) |
| Scale | the deep earth; what endures |
| Sky | wind, storm, mathematics, balance |
| Winter | silence, endings, not-quite-dead |

## Books ("Readables")

- Books are found **uncatalogued** (titles drawn randomly from era-decks — Baronial, Curia,
  Nocturnal, Solar, Dawn; a few are guaranteed, e.g. the Dispensary trio and all Numen books).
  Cataloguing at any desk (or via Consider, any soul element) reveals:
  **title, mystery principle + level, language, contamination, value**.
- **Mystery**: reading the book needs a combined total of one principle ≥ its level
  (levels run roughly 4–18; 25 for endgame recipes). Sources that stack: Element of the Soul,
  a Skill, a Memory (or the day's Weather), Tools, Inks, occasionally Papers/Helpers.
- **Language**: exotic-language books additionally require knowing that language
  (a Skill; see below). Native languages: Latin, Greek, Sanskrit, Aramaic, Phrygian.
- **Types**: book, scroll, phonograph **record**, **film**. Records/films can't be read at a
  normal desk — special equipment (phonograph, projector) is needed.
- **Contamination**: some books carry a curse/theoplasm/infestation/corruption. Removing it
  is a separate crafting challenge with a skill effective against that type. Reading a
  contaminated book with the wrong soul element risks **maladies**.
- **First successful read ("mastering")** grants: 1–3 **Lessons** (mystery 4–6 → 1,
  8–14 → 2, 16+ → 3) **and a specific Memory**. Every subsequent **re-read** (60s, any soul,
  no mystery check, needs language if applicable) yields **that same memory**.
  → This book↔memory pairing is the core fact the whole app hangs off.
- **Terminology:** the requirement's numeric level is what the community calls
  **difficulty** — wiki book tables label it "Mastery Difficulty" — and players
  conventionally shelve unread books *by difficulty*. The app therefore records it as
  `Books.difficulty`, usable even before the book is mastered. "Mystery" refers to the
  principle side of the requirement (the pair is what the game's card shows).
- ~280+ books exist in total; ~170+ obtainable in Hush House (incl. hidden Numen books), the
  rest via Oriflamme's auctions.

## Memories

- Cards that add principles to an action (reading, crafting, room unlocking, assisting).
- **Expire at dawn** unless **Persistent**; all memories (even persistent, even Numina)
  vanish at the first dawn after the season of **Numa**.
- A single memory typically carries 1–4 principles at levels 1–6
  (e.g. Wormwood Dream: Edge 3 / Moon 6 / Winter 6; Numina are 5/5/5 across three).
- Sources: reading/re-reading books, the day's **weather** (drawn from seasonal decks),
  talking to assistants/visitors, Consider-ing or consuming items, crafting at workstations,
  gathering, swimming, Numa events.
- Special kinds worth tagging: **Weather**, **Numen** (persistent, 5/5/5, victory item — only
  one may be held at a time, re-obtainable from its book), and memories with
  "Evolve via <Wisdom>" aspects.

## Skills, Wisdoms, Elements

- **63 skills + 10 exotic languages** (languages are skills). Each skill has a
  **primary principle (starts at 2) and secondary principle (starts at 1)**.
- Skills level up (max 9) with **Lessons + Memories**: reaching level N costs N
  understanding-cards matching at least one of the skill's principles (first card must be a
  Lesson). Each level adds +1 to *both* its principles. So a level-L skill contributes
  **L+1 primary / L secondary**.
- Lessons come from mastering books (skill-specific) and House of Light salons.
- Committing a skill to the **Tree of Wisdoms** (one of 9 Wisdoms, two allowed per skill)
  grants a specific **Element of the Soul** (Health, Chor, Shapt, Mettle, Ereb, Trist, Fet,
  Phost, Wist) and enables evolving that element (+, ++, +++).
- Languages are learned by paying visitors an Iron Spintria; they also slot into the Tree.

## Crafting & rooms (out of scope for v2 — future tables)

- Workstations accept recipes at three challenge tiers: **Prentice 5 / Scholar 10 / Keeper 15**.
  Products include memories, inks (Encaustum Terminale = 7/7/7), tools, food/drink.
- Rooms need an assistant boosted to the required principle level; one unlock per day.

## Runs are playthroughs

BoH is run-based: each playthrough is a fresh Librarian and library; findings do not
carry over (and new runs draw books from randomised decks). The app scopes every
book/memory/skill/journal row to a `Playthroughs` row; `Meta['active_playthrough']`
holds the loaded run. Switching = loading; creating = new game; the old run's data
stays in `Boh.db` (and git) untouched.

## Seasons & Numa (context)

- Seasons last 6 days; a day is 360s of game time. Weather drawn each day per seasonal deck.
- **Numa**: 1-day season, arrives unpredictably (1-in-9 token deck, 3 early blockers);
  memories wipe at its end; skills can be broken down into universal Lessons; endgame
  Histories are presented during Numa.

## Concept → schema mapping

| Game concept | Where it lives in `Boh.db` |
|---|---|
| Principle | `Principles` (13 rows, seeded) |
| Language | `Languages` (15 rows, seeded) |
| A book | `Books` (one row per physical copy; `read_status` uncatalogued → catalogued → mastered) |
| Book's reading challenge | `Books.mystery_principle_id` + `difficulty` ("Mastery Difficulty") |
| Book's language | `Books.language_id` → `Languages` |
| Book's type | `Books.book_kind` (book/scroll/film/record) |
| Book's contamination | `Books.contamination` |
| Memory a book always yields | `Books.yielded_memory_id` → `Memories` |
| Lessons a book grants | `Books.lessons` (count) + `BookLessons` (which skills) |
| A memory | `Memories` (+ `kind`, `persistent`) |
| Memory's aspects | `MemoryAspects` (memory × principle × level) |
| How a memory is obtained | `MemorySources` (kind + free-text detail) |
| A skill or language | `Skills` (`is_language`, primary/secondary principle, level, wisdom, element) |
| Anything the player wants to jot | `Journal` (optionally linked to book/memory/skill) |
| A saved game (playthrough) | `Playthroughs`; findings carry `playthrough_id`; `Meta` remembers the loaded one |

## To verify while playing

1. Do high-mystery books in exotic languages need the language **level** ≥ 2, or merely known?
2. Exact lesson counts at odd mystery levels (7? 15?).
3. Can any book yield *different* memories across reads (wiki says always the same memory)?
4. Native-language list (Latin, Greek, Sanskrit, Aramaic, Phrygian — from a guide, confirm).
5. Whether `contamination` types beyond the four listed exist post-DLC.

## Sources

- Wiki: *Principles*, *Memory*, *Numen*, *Skills*, *The Tree of Wisdoms*,
  *Elements of the Soul*, *Season*, *Numa* pages — book-of-hours.fandom.com
- Steam guide: *How to Meet the Aspect Requirements* (eightroomsofelixir)
- GameFAQs guide/walkthrough (alberozuko) — languages, soul stats, reading flow
- youarelowonhealth.com — *How to solve book mysteries* (catalogue/read/re-read loop)