-- 003_seed_languages.sql — languages of Book of Hours.
-- Native five per the GameFAQs guide (VERIFY in play — docs/GAME_MECHANICS.md);
-- exotic ten per the wiki's Languages table (learned from visitors for an Iron Spintria).

BEGIN;

INSERT INTO Languages (name, native) VALUES
  ('Latin',             1),
  ('Greek',             1),
  ('Sanskrit',          1),
  ('Aramaic',           1),
  ('Phrygian',          1),
  ('Cracktrack',        0),
  ('Deep Mandaic',      0),
  ('Ericapaean',        0),
  ('Fucine',            0),
  ('Hyksos',            0),
  ('Kernewek Henavek',  0),
  ('Killasimi',         0),
  ('Ramsund',           0),
  ('Sabazine',          0),
  ('Vak',               0);

PRAGMA user_version = 3;
COMMIT;