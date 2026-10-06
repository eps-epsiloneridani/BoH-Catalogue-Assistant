-- 007_fix_imported_book_kinds.sql
-- Data repair: the save importer stamping 2026-10-06 mis-read kind markers.
--
-- `SaveImporter.bookKind(in:)` mapped the tomes.json aspect `codex` to
-- BookKind 'record'. In the game's data `codex` is the plain bound-book
-- format (256 of the 281 installed tomes); phonograph records carry the
-- aspect `record.phonograph`. Every book created by the two imports of
-- AUTOSAVE.json on 2026-10-06 therefore entered the table as 'record':
--
--   * 2026-10-06 12:58:45  -> "First playthrough"  (41 of its 42 books)
--   * 2026-10-06 13:19:42  -> "Imported from AUTOSAVE" (41 of its 42 books)
--
-- The journal + updated_at trail proves no later write touched those rows,
-- so repairing at insert-time-fingerprint is safe: rows created by the app's
-- manual entry (14:22+) — including two genuine phonograph records — carry
-- their own later timestamps and are left alone. Rows already 'scroll' (the
-- two Tantras) are not 'record' and are left alone.
--
-- updated_at is stamped (rows genuinely change); created_at keeps the import
-- fingerprint for future audits. Forward-only, data-preserving: only
-- book_kind text changes; ids and every other column are untouched.

BEGIN;

UPDATE Books
SET book_kind = 'book',
    updated_at = datetime('now')
WHERE book_kind = 'record'
  AND created_at IN ('2026-10-06 12:58:45', '2026-10-06 13:19:42');

PRAGMA user_version = 7;

COMMIT;