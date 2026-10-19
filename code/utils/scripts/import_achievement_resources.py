#!/usr/bin/env python3
"""Build the achievement content tables from a TOB client's Resources/Achievements folder.

    python3 import_achievement_resources.py <client>/Resources/Achievements > achievements.sql
    mysql peq < achievements.sql

The server sends achievement state in the client's file order and sizes every record from the client's component
lists, so load the files from the client build your players use, and reload after a client update.

Components the files describe precisely get their event here: levels, skills, spent AA points, other achievements,
and the named monsters of hunts, raids and conquests. The SQL then links what needs the server's own data by exact
name: zones to visit, quests (tasks) to complete, and items to collect, equip or obtain. The counts it prints at the
end show what is left for quests or hand edits.
"""

import argparse
import re
import sys
from pathlib import Path

# Achievement::EventType in common/achievements.h
REACH_LEVEL = 1
SKILL_VALUE = 2
SKILL_MAX_FOR_LEVEL = 3
SPEND_AA_POINTS = 4
COMPLETE_ACHIEVEMENT = 5
COLLECT_ITEM = 8
COMPLETE_TASK = 9
ENTER_ZONE = 10
OWN_ITEM = 11
KILL_NAMED_NPC = 12

# Achievement::ComponentType in common/achievements.h
REQUIRED = 1

# EQ::skills::SkillType, by the names the achievement files use
SKILLS = {
    "1H Blunt": 0, "1H Slashing": 1, "2H Blunt": 2, "2H Slashing": 3, "Abjuration": 4, "Alteration": 5,
    "Apply Poison": 6, "Archery": 7, "Backstab": 8, "Bind Wound": 9, "Bash": 10, "Block": 11,
    "Brass Instruments": 12, "Channelling": 13, "Conjuration": 14, "Defense": 15, "Disarm": 16,
    "Disarm Traps": 17, "Divination": 18, "Dodge": 19, "Double Attack": 20, "Dragon Punch": 21,
    "Dual Wield": 22, "Eagle Strike": 23, "Evocation": 24, "Feign Death": 25, "Flying Kick": 26, "Forage": 27,
    "Hand To Hand": 28, "Hide": 29, "Kick": 30, "Meditate": 31, "Mend": 32, "Offense": 33, "Parry": 34,
    "Pick Lock": 35, "1H Piercing": 36, "Riposte": 37, "Round Kick": 38, "Safe Fall": 39, "Sense Heading": 40,
    "Singing": 41, "Sneak": 42, "Pick Pockets": 48, "Stringed Instruments": 49, "Swimming": 50, "Throwing": 51,
    "Tiger Claw": 52, "Tracking": 53, "Wind Instruments": 54, "Fishing": 55, "Poisonmaking": 56, "Tinkering": 57,
    "Research": 58, "Alchemy": 59, "Baking": 60, "Tailoring": 61, "Sense Traps": 62, "Smithing": 63,
    "Fletching": 64, "Brewing": 65, "Alcohol Tolerance": 66, "Begging": 67, "Jewelcrafting": 68, "Pottery": 69,
    "Percussion Instruments": 70, "Intimidation": 71, "Taunt": 73, "Frenzy": 74, "Triple Attack": 76,
    "2H Piercing": 77,
}

REACH_LEVEL_TEXT = re.compile(r"^Reach Level (\d+)\.?$")
SKILL_MAX_TEXT = re.compile(r"^Reach the maximum skill in (.+) at level (\d+)\.?$")
SKILL_VALUE_TEXT = re.compile(r"^Reach (\d+) skill in (.+?)\.?$")
SPEND_AA_TEXT = re.compile(r"^Spend (\d+) Alternate Advancement Points?\.?$")
COMPLETE_TEXT = re.compile(r'^Complete the achievement "(.+)"\.?$')
VISIT_TEXT = re.compile(r"^Visit (.+?)\.?$")
TASK_TEXT = re.compile(r'^Complete "(.+?)"\.?$')
ITEM_TEXT = re.compile(r"^(?:Equip|Obtain|Collect)(?: an?| the)? (.+?)\.?$")

COLLECTION_CATEGORIES = {"Collections"}
NAMED_NPC_CATEGORIES = {"Hunter", "Hunts", "Raids", "Conquest", "Conquests"}
INSERT_BATCH = 500
ACHIEVEMENT_FILES = (
    "AchievementsClient.txt", "AchievementComponentsClient.txt", "AchievementCategories.txt",
    "AchievementCategoryAssociationsClient.txt", "AchievementAssociationsClient.txt",
)


def read_rows(path, columns):
    rows = []
    with open(path, encoding="latin-1") as f:
        for line in f:
            fields = line.rstrip("\r\n").split("^")
            if len(fields) >= columns and fields[0].strip():
                rows.append(fields)
    return rows


def sql_string(value):
    return "'" + value.replace("\\", "\\\\").replace("'", "\\'") + "'"


def derive_event(text, achievement_ids_by_name):
    match = REACH_LEVEL_TEXT.match(text)
    if match:
        return REACH_LEVEL, 0, int(match.group(1))

    match = SKILL_MAX_TEXT.match(text)
    if match and match.group(1) in SKILLS:
        return SKILL_MAX_FOR_LEVEL, SKILLS[match.group(1)], int(match.group(2))

    match = SKILL_VALUE_TEXT.match(text)
    if match and match.group(2) in SKILLS:
        return SKILL_VALUE, SKILLS[match.group(2)], int(match.group(1))

    match = SPEND_AA_TEXT.match(text)
    if match:
        return SPEND_AA_POINTS, 0, int(match.group(1))

    match = COMPLETE_TEXT.match(text)
    name = match.group(1) if match else text
    ids = achievement_ids_by_name.get(name, [])
    if len(ids) == 1:
        return COMPLETE_ACHIEVEMENT, ids[0], 0

    return 0, 0, 0


# What the SQL should look up by name for a component the files alone can't resolve: (event type, name).
def name_link(text, category):
    if category in COLLECTION_CATEGORIES:
        match = ITEM_TEXT.match(text)
        return COLLECT_ITEM, match.group(1) if match else text

    match = VISIT_TEXT.match(text)
    if match:
        return ENTER_ZONE, match.group(1)

    match = TASK_TEXT.match(text)
    if match:
        return COMPLETE_TASK, match.group(1)

    match = ITEM_TEXT.match(text)
    return OWN_ITEM, match.group(1) if match else text


def write_inserts(out, table, columns, rows):
    for start in range(0, len(rows), INSERT_BATCH):
        out.write(f"INSERT INTO `{table}` ({', '.join(f'`{c}`' for c in columns)}) VALUES\n")
        out.write(",\n".join(f"({row})" for row in rows[start:start + INSERT_BATCH]))
        out.write(";\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("folder", type=Path, help="the client's Resources/Achievements folder")
    parser.add_argument("--output", type=Path, help="write the SQL here instead of stdout")
    args = parser.parse_args()

    achievements = read_rows(args.folder / "AchievementsClient.txt", 7)
    components = read_rows(args.folder / "AchievementComponentsClient.txt", 5)
    categories = read_rows(args.folder / "AchievementCategories.txt", 6)
    category_links = read_rows(args.folder / "AchievementCategoryAssociationsClient.txt", 3)

    names = {}
    achievement_ids_by_name = {}
    for row in achievements:
        achievement_id = int(row[0])
        names[achievement_id] = row[1]
        achievement_ids_by_name.setdefault(row[1], []).append(achievement_id)

    category_names = {int(row[2]): row[3] for row in categories}
    category_of = {int(row[2]): category_names.get(int(row[0]), "") for row in category_links}

    achievement_rows = [f"{achievement_id}, {sql_string(name)}" for achievement_id, name in sorted(names.items())]

    component_rows = []
    link_rows = []
    derived = {}
    seen = set()
    for row in components:
        achievement_id, sequence, component_type, component_id = (int(v) for v in row[:4])
        text = row[4]
        if achievement_id not in names or (achievement_id, component_type, component_id) in seen:
            continue

        seen.add((achievement_id, component_type, component_id))
        event_type, event_id, event_count = 0, 0, 0
        if component_type == REQUIRED:
            category = category_of.get(achievement_id, "")
            event_type, event_id, event_count = derive_event(text, achievement_ids_by_name)
            if event_type == COMPLETE_ACHIEVEMENT and event_id == achievement_id:
                event_type, event_id = 0, 0

            if not event_type and category in NAMED_NPC_CATEGORIES:
                event_type, event_count = KILL_NAMED_NPC, 1
            elif not event_type:
                link_type, link_name = name_link(text, category)
                link_rows.append(f"{achievement_id}, {component_id}, {link_type}, {sql_string(link_name)}")

        derived[event_type] = derived.get(event_type, 0) + 1
        component_rows.append(
            f"{achievement_id}, {component_type}, {component_id}, {sequence}, {sql_string(text)}, "
            f"{event_type}, {event_id}, {event_count}"
        )

    if not args.output:
        sys.stdout.reconfigure(encoding="utf-8", newline="\n")
    out = open(args.output, "w", encoding="utf-8", newline="\n") if args.output else sys.stdout
    out.write("-- generated by utils/scripts/import_achievement_resources.py\n")
    out.write("SET NAMES utf8mb4;\n")
    out.write("DELETE FROM `achievement_components`;\n")
    out.write("DELETE FROM `achievements`;\n")
    write_inserts(out, "achievements", ["id", "name"], achievement_rows)
    write_inserts(
        out,
        "achievement_components",
        ["achievement_id", "component_type", "component_id", "sequence", "description", "event_type", "event_id",
         "event_count"],
        component_rows,
    )

    out.write(f"""
-- Components linked by name to the server's items, zones and tasks. Where several rows share a name, the lowest id
-- is used.
CREATE TEMPORARY TABLE `achievement_name_links` (
  `achievement_id` int unsigned NOT NULL,
  `component_id` int unsigned NOT NULL,
  `event_type` tinyint unsigned NOT NULL,
  `name` varchar(256) NOT NULL,
  KEY `name` (`name`)
);
""")
    write_inserts(out, "achievement_name_links", ["achievement_id", "component_id", "event_type", "name"], link_rows)
    out.write(f"""
CREATE TEMPORARY TABLE `achievement_item_names` (
  `name` varchar(256) NOT NULL,
  `id` int NOT NULL,
  PRIMARY KEY (`name`)
);
INSERT IGNORE INTO `achievement_item_names` (`name`, `id`)
SELECT `Name`, MIN(`id`) FROM `items` GROUP BY `Name`;

UPDATE `achievement_components` ac
JOIN `achievement_name_links` l ON l.`achievement_id` = ac.`achievement_id` AND l.`component_id` = ac.`component_id`
JOIN `achievement_item_names` i ON i.`name` = l.`name`
SET ac.`event_type` = l.`event_type`, ac.`event_id` = i.`id`
WHERE ac.`component_type` = {REQUIRED} AND l.`event_type` IN ({COLLECT_ITEM}, {OWN_ITEM});

UPDATE `achievement_components` ac
JOIN `achievement_name_links` l ON l.`achievement_id` = ac.`achievement_id` AND l.`component_id` = ac.`component_id`
JOIN (SELECT `long_name`, MIN(`zoneidnumber`) AS `id` FROM `zone` GROUP BY `long_name`) z ON z.`long_name` = l.`name`
SET ac.`event_type` = {ENTER_ZONE}, ac.`event_id` = z.`id`
WHERE ac.`component_type` = {REQUIRED} AND l.`event_type` = {ENTER_ZONE};

UPDATE `achievement_components` ac
JOIN `achievement_name_links` l ON l.`achievement_id` = ac.`achievement_id` AND l.`component_id` = ac.`component_id`
JOIN (SELECT `title`, MIN(`id`) AS `id` FROM `tasks` GROUP BY `title`) t ON t.`title` = l.`name`
SET ac.`event_type` = {COMPLETE_TASK}, ac.`event_id` = t.`id`
WHERE ac.`component_type` = {REQUIRED} AND l.`event_type` = {COMPLETE_TASK};

DROP TEMPORARY TABLE `achievement_item_names`;
DROP TEMPORARY TABLE `achievement_name_links`;

SELECT `event_type`, COUNT(*) AS `components` FROM `achievement_components`
WHERE `component_type` = {REQUIRED} GROUP BY `event_type`;
""")
    if args.output:
        out.close()

    file_time = max(int((args.folder / name).stat().st_mtime) for name in ACHIEVEMENT_FILES)
    print(
        f"{len(achievement_rows)} achievements, {len(component_rows)} components; events from the files: "
        + ", ".join(f"type {k}: {v}" for k, v in sorted(derived.items()))
        + f"; {len(link_rows)} left to link by name",
        file=sys.stderr,
    )
    print(
        f"Newest file time in this folder: {file_time}. Clients load the files only when rule "
        "Character:AchievementFileTime equals the newest file time of their own copy.",
        file=sys.stderr,
    )


if __name__ == "__main__":
    main()
