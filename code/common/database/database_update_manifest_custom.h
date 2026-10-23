/*	EQEmu: EQEmulator

	Copyright (C) 2001-2026 EQEmu Development Team

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/
#pragma once

#include "common/database/database_update.h"

#include <vector>

std::vector<ManifestEntry> manifest_entries_custom = {
	ManifestEntry{
		.version = 1,
		.description = "2025_05_16_new_database_check_test",
		.check = "SHOW TABLES LIKE 'new_table'",
		.condition = "empty",
		.match = "",
		.sql = R"(
CREATE TABLE `new_table`  (
  `id` int NOT NULL AUTO_INCREMENT,
  PRIMARY KEY (`id`)
);
)",
		.content_schema_update = false,
	},
	ManifestEntry{
		.version = 2,
		.description = "2026_10_16_item_serial_allocator",
		.check = "SHOW TABLES LIKE 'item_serial_allocator'",
		.condition = "empty",
		.match = "",
		.sql = R"(
CREATE TABLE `item_serial_allocator` (
  `id` tinyint unsigned NOT NULL,
  `next_serial` bigint unsigned NOT NULL,
  PRIMARY KEY (`id`)
);
UPDATE `inventory` i JOIN (
  SELECT `guid` FROM `inventory` WHERE `guid` <> 0 GROUP BY `guid` HAVING COUNT(*) > 1
) d ON i.`guid` = d.`guid` SET i.`guid` = 0;
UPDATE `inventory` SET `guid` = 0 WHERE `guid` > 2147483647;
UPDATE `sharedbank` s JOIN (
  SELECT `guid` FROM `sharedbank` WHERE `guid` <> 0 GROUP BY `guid` HAVING COUNT(*) > 1
) d ON s.`guid` = d.`guid` SET s.`guid` = 0;
UPDATE `sharedbank` SET `guid` = 0 WHERE `guid` > 2147483647;
INSERT INTO `item_serial_allocator` (`id`, `next_serial`)
SELECT 1, GREATEST(
  (SELECT IFNULL(MAX(`guid`), 0) FROM `inventory`),
  (SELECT IFNULL(MAX(`guid`), 0) FROM `sharedbank`)
) + 1;
)",
		.content_schema_update = false,
	},
	ManifestEntry{
		.version = 3,
		.description = "2026_10_16_character_achievements",
		.check = "SHOW TABLES LIKE 'character_achievements'",
		.condition = "empty",
		.match = "",
		.sql = R"(
CREATE TABLE `character_achievements` (
  `character_id` int unsigned NOT NULL,
  `achievement_id` int unsigned NOT NULL,
  `completed_at` int unsigned NOT NULL,
  PRIMARY KEY (`character_id`, `achievement_id`)
);
CREATE TABLE `character_achievement_components` (
  `character_id` int unsigned NOT NULL,
  `achievement_id` int unsigned NOT NULL,
  `component_type` tinyint unsigned NOT NULL,
  `component_id` int unsigned NOT NULL,
  `progress` int unsigned NOT NULL,
  PRIMARY KEY (`character_id`, `achievement_id`, `component_type`, `component_id`)
);
)",
		.content_schema_update = false,
	},
	ManifestEntry{
		.version = 4,
		.description = "2026_10_16_achievements",
		.check = "SHOW TABLES LIKE 'achievements'",
		.condition = "empty",
		.match = "",
		.sql = R"(
CREATE TABLE `achievements` (
  `id` int unsigned NOT NULL,
  `name` varchar(128) NOT NULL,
  PRIMARY KEY (`id`)
);
CREATE TABLE `achievement_components` (
  `achievement_id` int unsigned NOT NULL,
  `component_type` tinyint unsigned NOT NULL,
  `component_id` int unsigned NOT NULL,
  `sequence` int unsigned NOT NULL,
  `description` varchar(256) NOT NULL,
  `event_type` tinyint unsigned NOT NULL DEFAULT 0,
  `event_id` int unsigned NOT NULL DEFAULT 0,
  `event_count` int unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`achievement_id`, `component_type`, `component_id`)
);
)",
		.content_schema_update = true,
	},
	ManifestEntry{
		.version = 5,
		.description = "2026_10_17_character_tradeskill_depot",
		.check = "SHOW TABLES LIKE 'character_tradeskill_depot'",
		.condition = "empty",
		.match = "",
		.sql = R"(
CREATE TABLE `character_tradeskill_depot` (
  `character_id` int unsigned NOT NULL,
  `item_id` int unsigned NOT NULL,
  `quantity` int unsigned NOT NULL,
  PRIMARY KEY (`character_id`, `item_id`)
);
)",
		.content_schema_update = false,
	},
	ManifestEntry{
		.version = 6,
		.description = "2026_10_17_marketplace_offers",
		.check = "SHOW TABLES LIKE 'marketplace_offers'",
		.condition = "empty",
		.match = "",
		.sql = R"(
CREATE TABLE `marketplace_offers` (
  `id` int unsigned NOT NULL,
  `item_id` int unsigned NOT NULL,
  `category` varchar(128) NOT NULL,
  `price` int unsigned NOT NULL DEFAULT 0,
  `is_new` tinyint unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`)
);
)",
		.content_schema_update = true,
	},
	ManifestEntry{
		.version = 7,
		.description = "2026_10_18_marketplace_offers_enabled",
		.check = "SHOW COLUMNS FROM `marketplace_offers` LIKE 'enabled'",
		.condition = "empty",
		.match = "",
		.sql = R"(
ALTER TABLE `marketplace_offers` ADD COLUMN `enabled` tinyint unsigned NOT NULL DEFAULT 1 AFTER `is_new`;
)",
		.content_schema_update = true,
	},
	ManifestEntry{
		.version = 8,
		.description = "2026_10_18_account_marketplace_funds",
		.check = "SHOW TABLES LIKE 'account_marketplace_funds'",
		.condition = "empty",
		.match = "",
		.sql = R"(
CREATE TABLE `account_marketplace_funds` (
  `account_id` int unsigned NOT NULL,
  `funds` int unsigned NOT NULL,
  PRIMARY KEY (`account_id`)
);
)",
		.content_schema_update = false,
	},
	ManifestEntry{
		.version = 9,
		.description = "2026_10_18_marketplace_offers_description",
		.check = "SHOW COLUMNS FROM `marketplace_offers` LIKE 'description'",
		.condition = "empty",
		.match = "",
		.sql = R"(
ALTER TABLE `marketplace_offers` ADD COLUMN `description` varchar(1024) NOT NULL DEFAULT '' AFTER `category`;
)",
		.content_schema_update = true,
	},
	ManifestEntry{
		.version = 10,
		.description = "2026_10_18_marketplace_offers_tags",
		.check = "SHOW COLUMNS FROM `marketplace_offers` LIKE 'tags'",
		.condition = "empty",
		.match = "",
		.sql = R"(
ALTER TABLE `marketplace_offers` ADD COLUMN `tags` varchar(512) NOT NULL DEFAULT '' AFTER `description`;
)",
		.content_schema_update = true,
	},
	ManifestEntry{
		.version = 11,
		.description = "2026_10_19_marketplace_package_contents",
		.check = "SHOW TABLES LIKE 'marketplace_package_contents'",
		.condition = "empty",
		.match = "",
		.sql = R"(
CREATE TABLE `marketplace_package_contents` (
  `package_item_id` int unsigned NOT NULL,
  `item_id` int unsigned NOT NULL,
  `quantity` int unsigned NOT NULL DEFAULT 1,
  `min_level` tinyint unsigned NOT NULL DEFAULT 0,
  `max_level` tinyint unsigned NOT NULL DEFAULT 0,
  `classes` int unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`package_item_id`, `item_id`, `min_level`)
);
)",
		.content_schema_update = true,
	},
	ManifestEntry{
		.version = 12,
		.description = "2026_10_19_marketplace_offers_featured",
		.check = "SHOW COLUMNS FROM `marketplace_offers` LIKE 'is_featured'",
		.condition = "empty",
		.match = "",
		.sql = R"(
ALTER TABLE `marketplace_offers` ADD COLUMN `is_featured` tinyint unsigned NOT NULL DEFAULT 0 AFTER `is_new`;
)",
		.content_schema_update = true,
	},
	ManifestEntry{
		.version = 13,
		.description = "2026_10_19_account_marketplace_ledger",
		.check = "SHOW TABLES LIKE 'account_marketplace_ledger'",
		.condition = "empty",
		.match = "",
		.sql = R"(
CREATE TABLE `account_marketplace_ledger` (
  `id` bigint unsigned NOT NULL AUTO_INCREMENT,
  `account_id` int unsigned NOT NULL,
  `character_id` int unsigned NOT NULL,
  `amount` int NOT NULL,
  `balance` int unsigned NOT NULL,
  `reason` varchar(32) NOT NULL,
  `detail` varchar(255) NOT NULL,
  `created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`id`),
  KEY `account_created` (`account_id`, `created_at`)
);
)",
		.content_schema_update = false,
	},
	ManifestEntry{
		.version = 14,
		.description = "2026_10_19_fellowships",
		.check = "SHOW TABLES LIKE 'fellowships'",
		.condition = "empty",
		.match = "",
		.sql = R"(
CREATE TABLE `fellowships` (
  `id` int unsigned NOT NULL AUTO_INCREMENT,
  `leader_id` int unsigned NOT NULL,
  `motd` varchar(1023) NOT NULL DEFAULT '',
  `created_at` int unsigned NOT NULL,
  PRIMARY KEY (`id`),
  UNIQUE KEY `leader_id` (`leader_id`)
);
CREATE TABLE `fellowship_members` (
  `character_id` int unsigned NOT NULL,
  `fellowship_id` int unsigned NOT NULL,
  `joined_at` int unsigned NOT NULL,
  `is_exp_sharing` tinyint unsigned NOT NULL DEFAULT 0,
  PRIMARY KEY (`character_id`),
  KEY `fellowship_id` (`fellowship_id`)
);
CREATE TABLE `fellowship_campsites` (
  `fellowship_id` int unsigned NOT NULL,
  `zone_id` smallint unsigned NOT NULL,
  `instance_id` smallint unsigned NOT NULL,
  `x` float NOT NULL,
  `y` float NOT NULL,
  `z` float NOT NULL,
  `heading` float NOT NULL,
  `campfire_type` int unsigned NOT NULL,
  `placed_at` int unsigned NOT NULL,
  `expires_at` int unsigned NOT NULL,
  PRIMARY KEY (`fellowship_id`)
);
)",
		.content_schema_update = false,
	},
	ManifestEntry{
		.version = 15,
		.description = "2026_10_19_character_personas",
		.check = "SHOW TABLES LIKE 'character_personas'",
		.condition = "empty",
		.match = "",
		.sql = R"(
CREATE TABLE IF NOT EXISTS `character_persona_spells` (
  `character_id` int unsigned NOT NULL,
  `class_id` tinyint unsigned NOT NULL,
  `slot_id` smallint unsigned NOT NULL,
  `spell_id` int NOT NULL,
  PRIMARY KEY (`character_id`, `class_id`, `slot_id`)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS `character_persona_memmed_spells` (
  `character_id` int unsigned NOT NULL,
  `class_id` tinyint unsigned NOT NULL,
  `slot_id` smallint unsigned NOT NULL,
  `spell_id` int NOT NULL,
  PRIMARY KEY (`character_id`, `class_id`, `slot_id`)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS `character_persona_disciplines` (
  `character_id` int unsigned NOT NULL,
  `class_id` tinyint unsigned NOT NULL,
  `slot_id` smallint unsigned NOT NULL,
  `disc_id` smallint unsigned NOT NULL,
  PRIMARY KEY (`character_id`, `class_id`, `slot_id`)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS `character_persona_skills` (
  `character_id` int unsigned NOT NULL,
  `class_id` tinyint unsigned NOT NULL,
  `skill_id` smallint unsigned NOT NULL,
  `value` smallint unsigned NOT NULL,
  PRIMARY KEY (`character_id`, `class_id`, `skill_id`)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS `character_persona_alternate_abilities` (
  `character_id` int unsigned NOT NULL,
  `class_id` tinyint unsigned NOT NULL,
  `aa_id` smallint unsigned NOT NULL,
  `aa_value` smallint unsigned NOT NULL,
  `charges` smallint unsigned NOT NULL,
  PRIMARY KEY (`character_id`, `class_id`, `aa_id`)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS `character_persona_bind` (
  `character_id` int unsigned NOT NULL,
  `class_id` tinyint unsigned NOT NULL,
  `slot` int NOT NULL,
  `zone_id` smallint unsigned NOT NULL,
  `instance_id` mediumint unsigned NOT NULL,
  `x` float NOT NULL,
  `y` float NOT NULL,
  `z` float NOT NULL,
  `heading` float NOT NULL,
  PRIMARY KEY (`character_id`, `class_id`, `slot`)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS `character_persona_buffs` (
  `character_id` int unsigned NOT NULL,
  `class_id` tinyint unsigned NOT NULL,
  `slot_id` tinyint unsigned NOT NULL,
  `spell_id` int NOT NULL,
  `caster_level` tinyint unsigned NOT NULL,
  `caster_name` varchar(64) NOT NULL,
  `ticsremaining` int NOT NULL,
  `initialduration` int NOT NULL,
  `counters` int unsigned NOT NULL,
  `numhits` int unsigned NOT NULL,
  `melee_rune` int unsigned NOT NULL,
  `magic_rune` int unsigned NOT NULL,
  `persistent` tinyint unsigned NOT NULL,
  `dot_rune` int unsigned NOT NULL,
  `caston_x` int NOT NULL,
  `caston_y` int NOT NULL,
  `caston_z` int NOT NULL,
  `ExtraDIChance` int NOT NULL,
  `instrument_mod` int NOT NULL,
  PRIMARY KEY (`character_id`, `class_id`, `slot_id`)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS `character_persona_bandolier` (
  `character_id` int unsigned NOT NULL,
  `class_id` tinyint unsigned NOT NULL,
  `bandolier_id` tinyint unsigned NOT NULL,
  `bandolier_slot` tinyint unsigned NOT NULL,
  `item_id` int unsigned NOT NULL,
  `icon` int unsigned NOT NULL,
  `bandolier_name` varchar(32) NOT NULL,
  PRIMARY KEY (`character_id`, `class_id`, `bandolier_id`, `bandolier_slot`)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS `character_persona_timers` (
  `character_id` int unsigned NOT NULL,
  `class_id` tinyint unsigned NOT NULL,
  `type` mediumint unsigned NOT NULL,
  `start` int unsigned NOT NULL,
  `duration` int unsigned NOT NULL,
  `enable` tinyint NOT NULL,
  PRIMARY KEY (`character_id`, `class_id`, `type`)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS `character_persona_pet_info` (
  `character_id` int unsigned NOT NULL,
  `class_id` tinyint unsigned NOT NULL,
  `pet` int NOT NULL,
  `petname` varchar(64) NOT NULL,
  `petpower` int NOT NULL,
  `spell_id` int NOT NULL,
  `hp` int NOT NULL,
  `mana` int NOT NULL,
  `size` float NOT NULL,
  `taunting` tinyint NOT NULL,
  PRIMARY KEY (`character_id`, `class_id`, `pet`)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS `character_persona_pet_buffs` (
  `character_id` int unsigned NOT NULL,
  `class_id` tinyint unsigned NOT NULL,
  `pet` int NOT NULL,
  `slot_id` tinyint unsigned NOT NULL,
  `spell_id` int NOT NULL,
  `caster_level` tinyint unsigned NOT NULL,
  `caster_name` varchar(64) NOT NULL,
  `ticsremaining` int NOT NULL,
  `initialduration` int NOT NULL,
  `counters` int unsigned NOT NULL,
  `numhits` int unsigned NOT NULL,
  `melee_rune` int unsigned NOT NULL,
  `magic_rune` int unsigned NOT NULL,
  `persistent` tinyint unsigned NOT NULL,
  `dot_rune` int unsigned NOT NULL,
  `caston_x` int NOT NULL,
  `caston_y` int NOT NULL,
  `caston_z` int NOT NULL,
  `ExtraDIChance` int NOT NULL,
  `instrument_mod` int unsigned NOT NULL,
  PRIMARY KEY (`character_id`, `class_id`, `pet`, `slot_id`)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS `character_persona_pet_inventory` (
  `character_id` int unsigned NOT NULL,
  `class_id` tinyint unsigned NOT NULL,
  `pet` int NOT NULL,
  `slot` int NOT NULL,
  `item_id` int NOT NULL,
  PRIMARY KEY (`character_id`, `class_id`, `pet`, `slot`)
) ENGINE=InnoDB;
CREATE TABLE IF NOT EXISTS `character_persona_data_buckets` (
  `character_id` int unsigned NOT NULL,
  `class_id` tinyint unsigned NOT NULL,
  `key` varchar(100) NOT NULL,
  `value` text,
  `expires` int unsigned NOT NULL,
  PRIMARY KEY (`character_id`, `class_id`, `key`)
) ENGINE=InnoDB;
CREATE TABLE `character_personas` (
  `character_id` int unsigned NOT NULL,
  `class_id` tinyint unsigned NOT NULL,
  `level` int unsigned NOT NULL,
  `level2` tinyint unsigned NOT NULL,
  `exp` int unsigned NOT NULL,
  `points` int unsigned NOT NULL,
  `aa_points` int unsigned NOT NULL,
  `aa_exp` int unsigned NOT NULL,
  `aa_points_spent` int unsigned NOT NULL,
  `e_aa_effects` int unsigned NOT NULL,
  `e_percent_to_aa` int unsigned NOT NULL,
  `e_expended_aa_spent` int unsigned NOT NULL,
  `deity` int unsigned NOT NULL,
  `gender` tinyint unsigned NOT NULL,
  `face` int unsigned NOT NULL,
  `hair_color` tinyint unsigned NOT NULL,
  `hair_style` tinyint unsigned NOT NULL,
  `beard` tinyint unsigned NOT NULL,
  `beard_color` tinyint unsigned NOT NULL,
  `eye_color_1` tinyint unsigned NOT NULL,
  `eye_color_2` tinyint unsigned NOT NULL,
  `drakkin_heritage` int unsigned NOT NULL,
  `drakkin_tattoo` int unsigned NOT NULL,
  `drakkin_details` int unsigned NOT NULL,
  `str` int unsigned NOT NULL,
  `sta` int unsigned NOT NULL,
  `cha` int unsigned NOT NULL,
  `dex` int unsigned NOT NULL,
  `int` int unsigned NOT NULL,
  `agi` int unsigned NOT NULL,
  `wis` int unsigned NOT NULL,
  `cur_hp` int unsigned NOT NULL,
  `mana` int unsigned NOT NULL,
  `endurance` int unsigned NOT NULL,
  `ability_number` tinyint unsigned NOT NULL,
  `ability_time_seconds` tinyint unsigned NOT NULL,
  `ability_time_minutes` tinyint unsigned NOT NULL,
  `ability_time_hours` tinyint unsigned NOT NULL,
  `created_at` datetime NOT NULL DEFAULT CURRENT_TIMESTAMP,
  PRIMARY KEY (`character_id`, `class_id`)
) ENGINE=InnoDB;
)",
		.content_schema_update = false,
	},
	ManifestEntry{
		.version = 16,
		.description = "2026_10_19_character_personas_race",
		.check = "SHOW COLUMNS FROM `character_personas` LIKE 'race'",
		.condition = "empty",
		.match = "",
		.sql = R"(
ALTER TABLE `character_personas` ADD COLUMN `race` smallint unsigned NOT NULL AFTER `class_id`;
UPDATE `character_personas` p JOIN `character_data` d ON d.`id` = p.`character_id` SET p.`race` = d.`race`;
)",
		.content_schema_update = false,
	},
	ManifestEntry{
		.version = 17,
		.description = "2026_10_20_character_equipment_sets",
		.check = "SHOW TABLES LIKE 'character_equipment_sets'",
		.condition = "empty",
		.match = "",
		.sql = R"(
CREATE TABLE `character_equipment_sets` (
  `character_id` int unsigned NOT NULL,
  `class_id` tinyint unsigned NOT NULL,
  `slot_id` smallint unsigned NOT NULL,
  `item_serial` int unsigned NOT NULL,
  PRIMARY KEY (`character_id`, `class_id`, `slot_id`)
) ENGINE=InnoDB;
)",
		.content_schema_update = false,
	},
	ManifestEntry{
		.version = 18,
		.description = "2026_10_21_god_hot_native_aa_level65",
		.check = "SELECT n.id FROM aa_ranks n JOIN aa_ranks u ON u.id = n.id + 20000 WHERE u.id BETWEEN 20000 AND 39999 AND u.expansion = 17 AND u.level_req = 65 AND n.expansion = 17 AND n.level_req = 70 LIMIT 1",
		.condition = "not_empty",
		.match = "",
		.sql = R"(
UPDATE aa_ranks n JOIN aa_ranks u ON u.id = n.id + 20000
SET n.level_req = 65
WHERE u.id BETWEEN 20000 AND 39999 AND u.expansion = 17 AND u.level_req = 65
  AND n.expansion = 17 AND n.level_req = 70;
)",
		.content_schema_update = true,
	},
	ManifestEntry{
		.version = 19,
		.description = "2026_10_21_aa_swarm_pets_runspeed",
		.check = "SELECT id FROM npc_types WHERE runspeed = 0 AND id IN (675,684,954,955,956,957,958,959,960,961,962,963,1318,1319,1320,1321,1322,1323,1324,1325,1326,1327,1328,1329,1330,1331,1332,1333,1334,1335,1336,1337,1338,1339,1340,1341,1342,1343,1344,1345,1346,1347,1348,1349,1350,1351,1352,1353,1354,1355,1356,1357,1358,1359,1360,1361,1362,1363,1364,1365,1366,1367,1368,1369,1370,1371,1372,1373,1374,1375,1376,1377,1378,1379,1380,1381,1382,1383,1384,1385,1386,1387,1388,1389,1408,1409,1410,1411,1412,1413,1414,1415,1416,1417,1418,1419) LIMIT 1",
		.condition = "not_empty",
		.match = "",
		.sql = R"(
UPDATE npc_types SET runspeed = 1.25 WHERE runspeed = 0 AND id IN (675,684,954,955,956,957,958,959,960,961,962,963,1318,1319,1320,1321,1322,1323,1324,1325,1326,1327,1328,1329,1330,1331,1332,1333,1334,1335,1336,1337,1338,1339,1340,1341,1342,1343,1344,1345,1346,1347,1348,1349,1350,1351,1352,1353,1354,1355,1356,1357,1358,1359,1360,1361,1362,1363,1364,1365,1366,1367,1368,1369,1370,1371,1372,1373,1374,1375,1376,1377,1378,1379,1380,1381,1382,1383,1384,1385,1386,1387,1388,1389,1408,1409,1410,1411,1412,1413,1414,1415,1416,1417,1418,1419);
)",
		.content_schema_update = true,
	},
	ManifestEntry{
		.version = 20,
		.description = "2026_10_21_bard_swarm_pet_no_mez",
		.check = "SELECT id FROM npc_types WHERE id = 654 AND npc_spells_id = 11",
		.condition = "not_empty",
		.match = "",
		.sql = R"(
UPDATE npc_types SET npc_spells_id = 0 WHERE id = 654;
)",
		.content_schema_update = true,
	},
	ManifestEntry{
		.version = 21,
		.description = "2026_10_21_marks_vendor_morphemic_staff",
		.check = "SELECT item FROM merchantlist WHERE merchantid = 344046 AND item = 66617",
		.condition = "empty",
		.match = "",
		.sql = R"(
INSERT INTO merchantlist (merchantid, slot, item, faction_required, level_required, min_status, max_status, alt_currency_cost, classes_required, probability, bucket_name, bucket_value, bucket_comparison, min_expansion, max_expansion)
VALUES (344046, 13, 66617, -1100, 0, 0, 255, 5, 65535, 100, '', '', 0, -1, -1);
UPDATE items SET ldonsellbackrate = 0 WHERE id = 66617;
)",
		.content_schema_update = true,
	},
	ManifestEntry{
		.version = 22,
		.description = "2026_10_21_bag_tier_slots_over_10",
		.check = "SELECT t.id FROM items t JOIN items b ON t.id = b.id + 300000 WHERE b.id < 200000 AND b.bagslots > 10 AND t.bagslots < b.bagslots + 2 UNION ALL SELECT t.id FROM items t JOIN items b ON t.id = b.id + 500000 WHERE b.id < 200000 AND b.bagslots > 10 AND t.bagslots < b.bagslots + 4 UNION ALL SELECT t.id FROM items t JOIN items b ON t.id = b.id + 700000 WHERE b.id < 200000 AND b.bagslots > 10 AND t.bagslots < b.bagslots + 6 LIMIT 1",
		.condition = "not_empty",
		.match = "",
		.sql = R"(
CREATE TABLE IF NOT EXISTS items_bagslots_bak_20261021 (id INT NOT NULL PRIMARY KEY, bagslots INT NOT NULL);
INSERT IGNORE INTO items_bagslots_bak_20261021 (id, bagslots)
SELECT t.id, t.bagslots FROM items t JOIN items b ON t.id = b.id + 300000 WHERE b.id < 200000 AND b.bagslots > 10
UNION ALL SELECT t.id, t.bagslots FROM items t JOIN items b ON t.id = b.id + 500000 WHERE b.id < 200000 AND b.bagslots > 10
UNION ALL SELECT t.id, t.bagslots FROM items t JOIN items b ON t.id = b.id + 700000 WHERE b.id < 200000 AND b.bagslots > 10;
UPDATE items t JOIN items b ON t.id = b.id + 300000 SET t.bagslots = b.bagslots + 2 WHERE b.id < 200000 AND b.bagslots > 10;
UPDATE items t JOIN items b ON t.id = b.id + 500000 SET t.bagslots = b.bagslots + 4 WHERE b.id < 200000 AND b.bagslots > 10;
UPDATE items t JOIN items b ON t.id = b.id + 700000 SET t.bagslots = b.bagslots + 6 WHERE b.id < 200000 AND b.bagslots > 10;
)",
		.content_schema_update = true,
	},
// Used for testing
//	ManifestEntry{
//		.version = 9229,
//		.description = "new_database_check_test",
//		.check = "SHOW TABLES LIKE 'new_table'",
//		.condition = "empty",
//		.match = "",
//		.sql = R"(
//CREATE TABLE `new_table`  (
//  `id` int NOT NULL AUTO_INCREMENT,
//  PRIMARY KEY (`id`)
//);
//CREATE TABLE `new_table1`  (
//  `id` int NOT NULL AUTO_INCREMENT,
//  PRIMARY KEY (`id`)
//);
//CREATE TABLE `new_table2`  (
//  `id` int NOT NULL AUTO_INCREMENT,
//  PRIMARY KEY (`id`)
//);
//CREATE TABLE `new_table3`  (
//  `id` int NOT NULL AUTO_INCREMENT,
//  PRIMARY KEY (`id`)
//);
//)",
//	}

};

// see struct definitions for what each field does
// struct ManifestEntry {
// 	int         version{};     // database version of the migration
// 	std::string description{}; // description of the migration ex: "add_new_table" or "add_index_to_table"
// 	std::string check{};       // query that checks against the condition
// 	std::string condition{};   // condition or "match_type" - Possible values [contains|match|missing|empty|not_empty]
// 	std::string match{};       // match field that is not always used, but works in conjunction with "condition" values [missing|match|contains]
// 	std::string sql{};         // the SQL DDL that gets ran when the condition is true
// };
