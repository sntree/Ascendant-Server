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
