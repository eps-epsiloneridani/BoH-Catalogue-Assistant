-- 005_playthroughs.sql
-- Book of Hours is run-based: each new Librarian starts a fresh Hush House, and
-- findings do not carry over. This migration introduces playthroughs:
--
--   * Playthroughs — one row per saved game (name, notes, created_at).
--   * Meta         — key/value table; key 'active_playthrough' holds the loaded
--                   playthrough's id.
--   * playthrough_id columns on Books, Memories, Skills and Journal. Scoping is
--     enforced by the application layer (every repository is constructed with a
--     playthrough id); the columns are nullable because SQLite cannot add a
--     NOT NULL column without a default, and legacy rows (if any) are backfilled
--     to the default playthrough below. ON DELETE CASCADE so deleting a playthrough
--     removes its findings (UI confirms first).
--
-- Principles and Languages are game-structure lookups and stay unscoped.

BEGIN;

CREATE TABLE Playthroughs (
  id         INTEGER PRIMARY KEY AUTOINCREMENT NOT NULL,
  name       TEXT    NOT NULL,
  created_at TEXT    NOT NULL DEFAULT (datetime('now')),
  notes      TEXT
);

CREATE TABLE Meta (
  key   TEXT PRIMARY KEY NOT NULL,
  value TEXT
);

ALTER TABLE Books    ADD COLUMN playthrough_id INTEGER REFERENCES Playthroughs(id) ON DELETE CASCADE;
ALTER TABLE Memories ADD COLUMN playthrough_id INTEGER REFERENCES Playthroughs(id) ON DELETE CASCADE;
ALTER TABLE Skills    ADD COLUMN playthrough_id INTEGER REFERENCES Playthroughs(id) ON DELETE CASCADE;
ALTER TABLE Journal   ADD COLUMN playthrough_id INTEGER REFERENCES Playthroughs(id) ON DELETE CASCADE;

-- Default playthrough for any pre-existing rows (the db is empty as of 2026-10-05,
-- but the migration stays correct for data-bearing databases).
INSERT INTO Playthroughs (name) VALUES ('First playthrough');
UPDATE Books    SET playthrough_id = (SELECT MAX(id) FROM Playthroughs);
UPDATE Memories SET playthrough_id = (SELECT MAX(id) FROM Playthroughs);
UPDATE Skills    SET playthrough_id = (SELECT MAX(id) FROM Playthroughs);
UPDATE Journal   SET playthrough_id = (SELECT MAX(id) FROM Playthroughs);

INSERT INTO Meta (key, value)
  VALUES ('active_playthrough', (SELECT MAX(id) FROM Playthroughs));

CREATE INDEX idx_books_playthrough    ON Books (playthrough_id);
CREATE INDEX idx_memories_playthrough ON Memories (playthrough_id);
CREATE INDEX idx_skills_playthrough   ON Skills (playthrough_id);
CREATE INDEX idx_journal_playthrough  ON Journal (playthrough_id);

PRAGMA user_version = 5;
COMMIT;