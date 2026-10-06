-- 004_rename_difficulty.sql
-- The numeric level of a book's reading requirement: adopt the term the game
-- community and the wiki use for it — "Difficulty" ("Mastery Difficulty" on wiki
-- book tables). Pure rename of Books.mystery_level; no data changes.
-- Rationale: the player records difficulty when they catalogue a book they cannot
-- yet master, so it must be a first-class field in their vocabulary.

BEGIN;

ALTER TABLE Books RENAME COLUMN mystery_level TO difficulty;

PRAGMA user_version = 4;
COMMIT;