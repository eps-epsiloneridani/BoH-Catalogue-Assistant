-- 008_humanize_transit_locations.sql
-- Locations stamped by the location feature used the save's raw sphere room ids:
--   purchases.europe — Oriflamme's auction: lots won, awaiting shelving (the
--                      auction recipes deposit wins at ~/purchases.<region>)
--   portage<N>        — items being carried between rooms (runtime spheres)
--   fixedverbs        — a task's output sphere (item mid-craft)
-- Humanize those prefixes for rows the location feature stamped (created_at
-- >= 2026-10-07 — location stamping did not exist before it; recorded locations
-- a user typed earlier carry earlier stamps and are never touched).

BEGIN;

UPDATE Books SET location = 'Oriflamme''s auction' || substr(location, LENGTH('purchases.europe') + 1)
WHERE location LIKE 'purchases.europe%' AND created_at >= '2026-10-07';

UPDATE Books SET location = 'in portage (player inventory)' || substr(location, LENGTH('portage' || (substr(location, 8, 1))) + 1)
WHERE location LIKE 'portage%' AND created_at >= '2026-10-07';

UPDATE Books SET location = 'in a task''s output sphere' || substr(location, LENGTH('fixedverbs') + 1)
WHERE location LIKE 'fixedverbs%' AND created_at >= '2026-10-07';

PRAGMA user_version = 8;

COMMIT;