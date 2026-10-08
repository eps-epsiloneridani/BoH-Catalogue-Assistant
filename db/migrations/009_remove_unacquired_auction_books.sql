-- 009_remove_unacquired_auction_books.sql
-- Ruling (2026-10-08): books sitting in Oriflamme's-auction purchases.* spheres
-- are SEEN but not OWNED - unearned; the spoiler posture says never import them.
-- The earlier import (before the skip existed) created such rows; remove them.
-- Scoped to location-feature-stamped rows (created_at >= 2026-10-07). Their
-- linked hidden memories stay in the table (they remain unearned/hidden).

BEGIN;

DELETE FROM Books
WHERE created_at >= '2026-10-07'
  AND (location LIKE 'Oriflamme%' OR location LIKE 'purchases.%');

PRAGMA user_version = 9;

COMMIT;
