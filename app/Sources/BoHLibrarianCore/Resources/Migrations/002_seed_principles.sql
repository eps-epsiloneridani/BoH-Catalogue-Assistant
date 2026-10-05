-- 002_seed_principles.sql — the 13 Principles of Book of Hours.
-- Structural reference data only (spoiler policy: docs/DECISIONS.md D6).
-- Glosses paraphrased from the game's principle descriptions (see docs/GAME_MECHANICS.md).
-- Colors are editable UI defaults for badges, not canon.

BEGIN;

INSERT INTO Principles (name, sort_order, color, notes) VALUES
  ('Edge',    1,  '#7B241C', 'battle, struggle, conquest'),
  ('Forge',   2,  '#D35400', 'fire, transformation, destruction'),
  ('Grail',   3,  '#884EA0', 'hunger, lust, birth and the feast'),
  ('Heart',   4,  '#E74C3C', 'continuation, preservation'),
  ('Knock',   5,  '#148F77', 'openings; unseals every barrier'),
  ('Lantern', 6,  '#F4D03F', 'illumination; the House of the Sun'),
  ('Moon',    7,  '#5D6D7E', 'the nocturnal, the forgotten (added with Numa)'),
  ('Moth',    8,  '#717D7E', 'chaos and yearning'),
  ('Nectar',  9,  '#27AE60', 'the green pulse of the seasons (once called Blood)'),
  ('Rose',    10, '#EC87C0', 'the rose that encompasseth all; new horizons (added with Numa)'),
  ('Scale',   11, '#6E2C00', 'the deep earth; what endures'),
  ('Sky',     12, '#5DADE2', 'wind, storm, mathematics, balance'),
  ('Winter',  13, '#85C1E9', 'silence, endings, not-quite-dead');

PRAGMA user_version = 2;
COMMIT;