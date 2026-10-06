#!/usr/bin/env python3
"""Save-import prototype: preview a Book of Hours AUTOSAVE as BoH Librarian rows.

Feasibility proof for the pending "populate from Steam save" feature
(docs/SAVE_IMPORT.md). Read-only: touches nothing but stdout.

Usage:  python3 scripts/import-save-preview.py [path-to-AUTOSAVE.json]
"""

import glob
import json
import os
import re
import sys

GAME_ELEMENTS = os.path.expanduser(
    "~/Library/Application Support/Steam/steamapps/common/"
    "Book of Hours/OSX.app/Contents/Resources/Data/StreamingAssets/"
    "bhcontent/core/elements")
DEFAULT_SAVE = (os.path.expanduser("~/Library/Application Support/Weather Factory/"
                                   "Book of Hours/AUTOSAVE.json"))

# The game's JSON dialect: some files are UTF-16 with BOM, some UTF-8; all may
# contain raw control characters in strings and trailing commas. (Decode by BOM:
# UTF-8 bytes can "successfully" decode as UTF-16 garbage otherwise.)
def load(path):
    raw = open(path, "rb").read()
    if raw[:2] in (b"\xff\xfe", b"\xfe\xff"):
        text = raw.decode("utf-16")
    else:
        text = raw.decode("utf-8")
    text = re.sub(r",(\s*[\]}])", r"\1", text)
    return json.loads(text, strict=False)


def element_index():
    index = {}
    for path in glob.glob(GAME_ELEMENTS + "/*.json"):
        try:
            for element in load(path).get("elements", []):
                eid = element.get("ID") or element.get("id")
                if eid:
                    index[eid] = element
        except Exception:
            continue
    return index


def stacks_in(save):
    found = []

    def walk(sphere):
        for token in sphere.get("Tokens") or []:
            payload = token.get("Payload") or {}
            if payload.get("$type", "").startswith("ElementStackCreationCommand"):
                found.append(payload)
            for dominion in payload.get("Dominions") or []:
                for nested in dominion.get("Spheres") or []:
                    walk(nested)

    for sphere in save["RootPopulationCommand"]["Spheres"]:
        walk(sphere)
    return found


def main():
    save_path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SAVE
    index = element_index()
    save = load(save_path)

    def label(eid):
        return (index.get(eid) or {}).get("Label") or eid

    def skill_label(trigger_id):
        # Lesson xtrigger ids vary: x.<skill>, s.<skill>, or bare.
        bare = re.sub(r"^(x\.|s\.)", "", trigger_id)
        return label("s." + bare)

    NATIVE_LANGUAGES = {"greek", "latin", "sanskrit", "aramaic", "phrygian"}

    def language_label(aspect_key):
        bare = aspect_key[2:]  # strip "w."
        if bare in NATIVE_LANGUAGES:  # native languages aren't skill elements
            return bare.capitalize()
        return label(bare) if bare in index else label("s." + bare)

    books, skills = [], []
    for stack in stacks_in(save):
        eid = stack.get("EntityId") or ""
        if eid.startswith("t."):
            books.append(stack)
        elif eid.startswith("s."):
            skills.append(stack)

    print(f"save: {save_path}")
    print(f"books in save: {len(books)} | skills in save: {len(skills)}\n")

    print("=== BOOKS ===")
    print(f"{'TITLE':44} {'MYSTERY':<12} {'LANG':<12} {'LESSONS':<30} {'YIELDS':<26} STATE")
    for stack in sorted(books, key=lambda s: s.get("EntityId") or ""):
        eid = stack.get("EntityId")
        definition = index.get(eid) or {}
        aspects = definition.get("aspects") or {}
        mutations = {k: v for k, v in (stack.get("Mutations") or {}).items()
                     if k != "$type"}
        mystery = next((f"{k.split('.')[1].capitalize()} {v}"
                        for k, v in aspects.items() if k.startswith("mystery.")), "?")
        language = next((language_label(k) for k in aspects if k.startswith("w.")), "—")
        xtriggers = definition.get("xtriggers") or {}
        lessons = ", ".join(
            f"{skill_label(t['id'])}" + (f" x{t.get('level', 1)}"
                                         if t.get("level", 1) > 1 else "")
            for key, effects in xtriggers.items() if key.startswith("mastering")
            for t in effects) or "—"
        yields = next((label(t["id"])
                       for key, effects in xtriggers.items() if key.startswith("reading")
                       for t in effects), "—")
        mastered = any(k.startswith("mastery.") for k in mutations)
        contaminated = next((k.split(".")[1] for k in mutations
                             if k.startswith("contamination.")), None)
        state = "MASTERED" if mastered else "unread"
        if contaminated:
            state += f" [{contaminated}]"
        print(f"{(definition.get('Label') or eid)[:43]:44} {mystery:<12} "
              f"{language[:11]:<12} {lessons[:29]:<30} {yields[:25]:<26} {state}")

    print("\n=== SKILLS (levels from save mutations) ===")
    seen = set()
    for stack in sorted(skills, key=lambda s: s.get("EntityId") or ""):
        eid = stack.get("EntityId")
        definition = index.get(eid) or {}
        mutations = {k: v for k, v in (stack.get("Mutations") or {}).items()
                     if k != "$type"}
        level = 1 + mutations.get("skill", 0)
        wisdom = next((k[2:] for k in mutations if k.startswith("w.")), "")
        element = next((k[2:] for k in mutations if k.startswith("a.x")), "")
        commitment = f"-> {wisdom} / {element}" if wisdom or element else ""
        key = (eid, level, commitment)
        if key in seen:  # the save keeps duplicate committed/uncommitted copies
            continue
        seen.add(key)
        print(f"{(definition.get('Label') or eid)[:34]:34} level {level}  {commitment}")

    print("\nNOTE: 17 uncatbook stacks exist in this save (uncatalogued texts by deck")
    print("tier). The save does not record which book each one will become — decks")
    print("decide at catalogue time — so an importer should skip them.")


if __name__ == "__main__":
    main()