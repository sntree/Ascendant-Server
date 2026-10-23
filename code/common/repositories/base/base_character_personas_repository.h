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
/**
 * DO NOT MODIFY THIS FILE
 *
 * This repository was automatically generated and is NOT to be modified directly.
 * Any repository modifications are meant to be made to the repository extending the base.
 * Any modifications to base repositories are to be made by the generator only
 *
 * @generator ./utils/scripts/generators/repository-generator.pl
 * @docs https://docs.eqemu.dev/developer/repositories
 */

#pragma once

#include "common/database.h"
#include "common/strings.h"

#include <ctime>

class BaseCharacterPersonasRepository {
public:
	struct CharacterPersonas {
		uint32_t character_id;
		uint8_t  class_id;
		uint16_t race;
		uint32_t level;
		uint8_t  level2;
		uint32_t exp;
		uint32_t points;
		uint32_t aa_points;
		uint32_t aa_exp;
		uint32_t aa_points_spent;
		uint32_t e_aa_effects;
		uint32_t e_percent_to_aa;
		uint32_t e_expended_aa_spent;
		uint32_t deity;
		uint8_t  gender;
		uint32_t face;
		uint8_t  hair_color;
		uint8_t  hair_style;
		uint8_t  beard;
		uint8_t  beard_color;
		uint8_t  eye_color_1;
		uint8_t  eye_color_2;
		uint32_t drakkin_heritage;
		uint32_t drakkin_tattoo;
		uint32_t drakkin_details;
		uint32_t str;
		uint32_t sta;
		uint32_t cha;
		uint32_t dex;
		uint32_t int_;
		uint32_t agi;
		uint32_t wis;
		uint32_t cur_hp;
		uint32_t mana;
		uint32_t endurance;
		uint8_t  ability_number;
		uint8_t  ability_time_seconds;
		uint8_t  ability_time_minutes;
		uint8_t  ability_time_hours;
		time_t   created_at;
	};

	static std::string PrimaryKey()
	{
		return std::string("character_id");
	}

	static std::vector<std::string> Columns()
	{
		return {
			"character_id",
			"class_id",
			"race",
			"level",
			"level2",
			"exp",
			"points",
			"aa_points",
			"aa_exp",
			"aa_points_spent",
			"e_aa_effects",
			"e_percent_to_aa",
			"e_expended_aa_spent",
			"deity",
			"gender",
			"face",
			"hair_color",
			"hair_style",
			"beard",
			"beard_color",
			"eye_color_1",
			"eye_color_2",
			"drakkin_heritage",
			"drakkin_tattoo",
			"drakkin_details",
			"str",
			"sta",
			"cha",
			"dex",
			"`int`",
			"agi",
			"wis",
			"cur_hp",
			"mana",
			"endurance",
			"ability_number",
			"ability_time_seconds",
			"ability_time_minutes",
			"ability_time_hours",
			"created_at",
		};
	}

	static std::vector<std::string> SelectColumns()
	{
		return {
			"character_id",
			"class_id",
			"race",
			"level",
			"level2",
			"exp",
			"points",
			"aa_points",
			"aa_exp",
			"aa_points_spent",
			"e_aa_effects",
			"e_percent_to_aa",
			"e_expended_aa_spent",
			"deity",
			"gender",
			"face",
			"hair_color",
			"hair_style",
			"beard",
			"beard_color",
			"eye_color_1",
			"eye_color_2",
			"drakkin_heritage",
			"drakkin_tattoo",
			"drakkin_details",
			"str",
			"sta",
			"cha",
			"dex",
			"`int`",
			"agi",
			"wis",
			"cur_hp",
			"mana",
			"endurance",
			"ability_number",
			"ability_time_seconds",
			"ability_time_minutes",
			"ability_time_hours",
			"UNIX_TIMESTAMP(created_at)",
		};
	}

	static std::string ColumnsRaw()
	{
		return std::string(Strings::Implode(", ", Columns()));
	}

	static std::string SelectColumnsRaw()
	{
		return std::string(Strings::Implode(", ", SelectColumns()));
	}

	static std::string TableName()
	{
		return std::string("character_personas");
	}

	static std::string BaseSelect()
	{
		return fmt::format(
			"SELECT {} FROM {}",
			SelectColumnsRaw(),
			TableName()
		);
	}

	static std::string BaseInsert()
	{
		return fmt::format(
			"INSERT INTO {} ({}) ",
			TableName(),
			ColumnsRaw()
		);
	}

	static CharacterPersonas NewEntity()
	{
		CharacterPersonas e{};

		e.character_id         = 0;
		e.class_id             = 0;
		e.race                 = 0;
		e.level                = 0;
		e.level2               = 0;
		e.exp                  = 0;
		e.points               = 0;
		e.aa_points            = 0;
		e.aa_exp               = 0;
		e.aa_points_spent      = 0;
		e.e_aa_effects         = 0;
		e.e_percent_to_aa      = 0;
		e.e_expended_aa_spent  = 0;
		e.deity                = 0;
		e.gender               = 0;
		e.face                 = 0;
		e.hair_color           = 0;
		e.hair_style           = 0;
		e.beard                = 0;
		e.beard_color          = 0;
		e.eye_color_1          = 0;
		e.eye_color_2          = 0;
		e.drakkin_heritage     = 0;
		e.drakkin_tattoo       = 0;
		e.drakkin_details      = 0;
		e.str                  = 0;
		e.sta                  = 0;
		e.cha                  = 0;
		e.dex                  = 0;
		e.int_                 = 0;
		e.agi                  = 0;
		e.wis                  = 0;
		e.cur_hp               = 0;
		e.mana                 = 0;
		e.endurance            = 0;
		e.ability_number       = 0;
		e.ability_time_seconds = 0;
		e.ability_time_minutes = 0;
		e.ability_time_hours   = 0;
		e.created_at           = std::time(nullptr);

		return e;
	}

	static CharacterPersonas GetCharacterPersonas(
		const std::vector<CharacterPersonas> &character_personass,
		int character_personas_id
	)
	{
		for (auto &character_personas : character_personass) {
			if (character_personas.character_id == character_personas_id) {
				return character_personas;
			}
		}

		return NewEntity();
	}

	static CharacterPersonas FindOne(
		Database& db,
		int character_personas_id
	)
	{
		auto results = db.QueryDatabase(
			fmt::format(
				"{} WHERE {} = {} LIMIT 1",
				BaseSelect(),
				PrimaryKey(),
				character_personas_id
			)
		);

		auto row = results.begin();
		if (results.RowCount() == 1) {
			CharacterPersonas e{};

			e.character_id         = row[0] ? static_cast<uint32_t>(strtoul(row[0], nullptr, 10)) : 0;
			e.class_id             = row[1] ? static_cast<uint8_t>(strtoul(row[1], nullptr, 10)) : 0;
			e.race                 = row[2] ? static_cast<uint16_t>(strtoul(row[2], nullptr, 10)) : 0;
			e.level                = row[3] ? static_cast<uint32_t>(strtoul(row[3], nullptr, 10)) : 0;
			e.level2               = row[4] ? static_cast<uint8_t>(strtoul(row[4], nullptr, 10)) : 0;
			e.exp                  = row[5] ? static_cast<uint32_t>(strtoul(row[5], nullptr, 10)) : 0;
			e.points               = row[6] ? static_cast<uint32_t>(strtoul(row[6], nullptr, 10)) : 0;
			e.aa_points            = row[7] ? static_cast<uint32_t>(strtoul(row[7], nullptr, 10)) : 0;
			e.aa_exp               = row[8] ? static_cast<uint32_t>(strtoul(row[8], nullptr, 10)) : 0;
			e.aa_points_spent      = row[9] ? static_cast<uint32_t>(strtoul(row[9], nullptr, 10)) : 0;
			e.e_aa_effects         = row[10] ? static_cast<uint32_t>(strtoul(row[10], nullptr, 10)) : 0;
			e.e_percent_to_aa      = row[11] ? static_cast<uint32_t>(strtoul(row[11], nullptr, 10)) : 0;
			e.e_expended_aa_spent  = row[12] ? static_cast<uint32_t>(strtoul(row[12], nullptr, 10)) : 0;
			e.deity                = row[13] ? static_cast<uint32_t>(strtoul(row[13], nullptr, 10)) : 0;
			e.gender               = row[14] ? static_cast<uint8_t>(strtoul(row[14], nullptr, 10)) : 0;
			e.face                 = row[15] ? static_cast<uint32_t>(strtoul(row[15], nullptr, 10)) : 0;
			e.hair_color           = row[16] ? static_cast<uint8_t>(strtoul(row[16], nullptr, 10)) : 0;
			e.hair_style           = row[17] ? static_cast<uint8_t>(strtoul(row[17], nullptr, 10)) : 0;
			e.beard                = row[18] ? static_cast<uint8_t>(strtoul(row[18], nullptr, 10)) : 0;
			e.beard_color          = row[19] ? static_cast<uint8_t>(strtoul(row[19], nullptr, 10)) : 0;
			e.eye_color_1          = row[20] ? static_cast<uint8_t>(strtoul(row[20], nullptr, 10)) : 0;
			e.eye_color_2          = row[21] ? static_cast<uint8_t>(strtoul(row[21], nullptr, 10)) : 0;
			e.drakkin_heritage     = row[22] ? static_cast<uint32_t>(strtoul(row[22], nullptr, 10)) : 0;
			e.drakkin_tattoo       = row[23] ? static_cast<uint32_t>(strtoul(row[23], nullptr, 10)) : 0;
			e.drakkin_details      = row[24] ? static_cast<uint32_t>(strtoul(row[24], nullptr, 10)) : 0;
			e.str                  = row[25] ? static_cast<uint32_t>(strtoul(row[25], nullptr, 10)) : 0;
			e.sta                  = row[26] ? static_cast<uint32_t>(strtoul(row[26], nullptr, 10)) : 0;
			e.cha                  = row[27] ? static_cast<uint32_t>(strtoul(row[27], nullptr, 10)) : 0;
			e.dex                  = row[28] ? static_cast<uint32_t>(strtoul(row[28], nullptr, 10)) : 0;
			e.int_                 = row[29] ? static_cast<uint32_t>(strtoul(row[29], nullptr, 10)) : 0;
			e.agi                  = row[30] ? static_cast<uint32_t>(strtoul(row[30], nullptr, 10)) : 0;
			e.wis                  = row[31] ? static_cast<uint32_t>(strtoul(row[31], nullptr, 10)) : 0;
			e.cur_hp               = row[32] ? static_cast<uint32_t>(strtoul(row[32], nullptr, 10)) : 0;
			e.mana                 = row[33] ? static_cast<uint32_t>(strtoul(row[33], nullptr, 10)) : 0;
			e.endurance            = row[34] ? static_cast<uint32_t>(strtoul(row[34], nullptr, 10)) : 0;
			e.ability_number       = row[35] ? static_cast<uint8_t>(strtoul(row[35], nullptr, 10)) : 0;
			e.ability_time_seconds = row[36] ? static_cast<uint8_t>(strtoul(row[36], nullptr, 10)) : 0;
			e.ability_time_minutes = row[37] ? static_cast<uint8_t>(strtoul(row[37], nullptr, 10)) : 0;
			e.ability_time_hours   = row[38] ? static_cast<uint8_t>(strtoul(row[38], nullptr, 10)) : 0;
			e.created_at           = strtoll(row[39] ? row[39] : "-1", nullptr, 10);

			return e;
		}

		return NewEntity();
	}

	static int DeleteOne(
		Database& db,
		int character_personas_id
	)
	{
		auto results = db.QueryDatabase(
			fmt::format(
				"DELETE FROM {} WHERE {} = {}",
				TableName(),
				PrimaryKey(),
				character_personas_id
			)
		);

		return (results.Success() ? results.RowsAffected() : 0);
	}

	static int UpdateOne(
		Database& db,
		const CharacterPersonas &e
	)
	{
		std::vector<std::string> v;

		auto columns = Columns();

		v.push_back(columns[0] + " = " + std::to_string(e.character_id));
		v.push_back(columns[1] + " = " + std::to_string(e.class_id));
		v.push_back(columns[2] + " = " + std::to_string(e.race));
		v.push_back(columns[3] + " = " + std::to_string(e.level));
		v.push_back(columns[4] + " = " + std::to_string(e.level2));
		v.push_back(columns[5] + " = " + std::to_string(e.exp));
		v.push_back(columns[6] + " = " + std::to_string(e.points));
		v.push_back(columns[7] + " = " + std::to_string(e.aa_points));
		v.push_back(columns[8] + " = " + std::to_string(e.aa_exp));
		v.push_back(columns[9] + " = " + std::to_string(e.aa_points_spent));
		v.push_back(columns[10] + " = " + std::to_string(e.e_aa_effects));
		v.push_back(columns[11] + " = " + std::to_string(e.e_percent_to_aa));
		v.push_back(columns[12] + " = " + std::to_string(e.e_expended_aa_spent));
		v.push_back(columns[13] + " = " + std::to_string(e.deity));
		v.push_back(columns[14] + " = " + std::to_string(e.gender));
		v.push_back(columns[15] + " = " + std::to_string(e.face));
		v.push_back(columns[16] + " = " + std::to_string(e.hair_color));
		v.push_back(columns[17] + " = " + std::to_string(e.hair_style));
		v.push_back(columns[18] + " = " + std::to_string(e.beard));
		v.push_back(columns[19] + " = " + std::to_string(e.beard_color));
		v.push_back(columns[20] + " = " + std::to_string(e.eye_color_1));
		v.push_back(columns[21] + " = " + std::to_string(e.eye_color_2));
		v.push_back(columns[22] + " = " + std::to_string(e.drakkin_heritage));
		v.push_back(columns[23] + " = " + std::to_string(e.drakkin_tattoo));
		v.push_back(columns[24] + " = " + std::to_string(e.drakkin_details));
		v.push_back(columns[25] + " = " + std::to_string(e.str));
		v.push_back(columns[26] + " = " + std::to_string(e.sta));
		v.push_back(columns[27] + " = " + std::to_string(e.cha));
		v.push_back(columns[28] + " = " + std::to_string(e.dex));
		v.push_back(columns[29] + " = " + std::to_string(e.int_));
		v.push_back(columns[30] + " = " + std::to_string(e.agi));
		v.push_back(columns[31] + " = " + std::to_string(e.wis));
		v.push_back(columns[32] + " = " + std::to_string(e.cur_hp));
		v.push_back(columns[33] + " = " + std::to_string(e.mana));
		v.push_back(columns[34] + " = " + std::to_string(e.endurance));
		v.push_back(columns[35] + " = " + std::to_string(e.ability_number));
		v.push_back(columns[36] + " = " + std::to_string(e.ability_time_seconds));
		v.push_back(columns[37] + " = " + std::to_string(e.ability_time_minutes));
		v.push_back(columns[38] + " = " + std::to_string(e.ability_time_hours));
		v.push_back(columns[39] + " = FROM_UNIXTIME(" + (e.created_at > 0 ? std::to_string(e.created_at) : "null") + ")");

		auto results = db.QueryDatabase(
			fmt::format(
				"UPDATE {} SET {} WHERE {} = {}",
				TableName(),
				Strings::Implode(", ", v),
				PrimaryKey(),
				e.character_id
			)
		);

		return (results.Success() ? results.RowsAffected() : 0);
	}

	static CharacterPersonas InsertOne(
		Database& db,
		CharacterPersonas e
	)
	{
		std::vector<std::string> v;

		v.push_back(std::to_string(e.character_id));
		v.push_back(std::to_string(e.class_id));
		v.push_back(std::to_string(e.race));
		v.push_back(std::to_string(e.level));
		v.push_back(std::to_string(e.level2));
		v.push_back(std::to_string(e.exp));
		v.push_back(std::to_string(e.points));
		v.push_back(std::to_string(e.aa_points));
		v.push_back(std::to_string(e.aa_exp));
		v.push_back(std::to_string(e.aa_points_spent));
		v.push_back(std::to_string(e.e_aa_effects));
		v.push_back(std::to_string(e.e_percent_to_aa));
		v.push_back(std::to_string(e.e_expended_aa_spent));
		v.push_back(std::to_string(e.deity));
		v.push_back(std::to_string(e.gender));
		v.push_back(std::to_string(e.face));
		v.push_back(std::to_string(e.hair_color));
		v.push_back(std::to_string(e.hair_style));
		v.push_back(std::to_string(e.beard));
		v.push_back(std::to_string(e.beard_color));
		v.push_back(std::to_string(e.eye_color_1));
		v.push_back(std::to_string(e.eye_color_2));
		v.push_back(std::to_string(e.drakkin_heritage));
		v.push_back(std::to_string(e.drakkin_tattoo));
		v.push_back(std::to_string(e.drakkin_details));
		v.push_back(std::to_string(e.str));
		v.push_back(std::to_string(e.sta));
		v.push_back(std::to_string(e.cha));
		v.push_back(std::to_string(e.dex));
		v.push_back(std::to_string(e.int_));
		v.push_back(std::to_string(e.agi));
		v.push_back(std::to_string(e.wis));
		v.push_back(std::to_string(e.cur_hp));
		v.push_back(std::to_string(e.mana));
		v.push_back(std::to_string(e.endurance));
		v.push_back(std::to_string(e.ability_number));
		v.push_back(std::to_string(e.ability_time_seconds));
		v.push_back(std::to_string(e.ability_time_minutes));
		v.push_back(std::to_string(e.ability_time_hours));
		v.push_back("FROM_UNIXTIME(" + (e.created_at > 0 ? std::to_string(e.created_at) : "null") + ")");

		auto results = db.QueryDatabase(
			fmt::format(
				"{} VALUES ({})",
				BaseInsert(),
				Strings::Implode(",", v)
			)
		);

		if (results.Success()) {
			e.character_id = results.LastInsertedID();
			return e;
		}

		e = NewEntity();

		return e;
	}

	static int InsertMany(
		Database& db,
		const std::vector<CharacterPersonas> &entries
	)
	{
		std::vector<std::string> insert_chunks;

		for (auto &e: entries) {
			std::vector<std::string> v;

			v.push_back(std::to_string(e.character_id));
			v.push_back(std::to_string(e.class_id));
			v.push_back(std::to_string(e.race));
			v.push_back(std::to_string(e.level));
			v.push_back(std::to_string(e.level2));
			v.push_back(std::to_string(e.exp));
			v.push_back(std::to_string(e.points));
			v.push_back(std::to_string(e.aa_points));
			v.push_back(std::to_string(e.aa_exp));
			v.push_back(std::to_string(e.aa_points_spent));
			v.push_back(std::to_string(e.e_aa_effects));
			v.push_back(std::to_string(e.e_percent_to_aa));
			v.push_back(std::to_string(e.e_expended_aa_spent));
			v.push_back(std::to_string(e.deity));
			v.push_back(std::to_string(e.gender));
			v.push_back(std::to_string(e.face));
			v.push_back(std::to_string(e.hair_color));
			v.push_back(std::to_string(e.hair_style));
			v.push_back(std::to_string(e.beard));
			v.push_back(std::to_string(e.beard_color));
			v.push_back(std::to_string(e.eye_color_1));
			v.push_back(std::to_string(e.eye_color_2));
			v.push_back(std::to_string(e.drakkin_heritage));
			v.push_back(std::to_string(e.drakkin_tattoo));
			v.push_back(std::to_string(e.drakkin_details));
			v.push_back(std::to_string(e.str));
			v.push_back(std::to_string(e.sta));
			v.push_back(std::to_string(e.cha));
			v.push_back(std::to_string(e.dex));
			v.push_back(std::to_string(e.int_));
			v.push_back(std::to_string(e.agi));
			v.push_back(std::to_string(e.wis));
			v.push_back(std::to_string(e.cur_hp));
			v.push_back(std::to_string(e.mana));
			v.push_back(std::to_string(e.endurance));
			v.push_back(std::to_string(e.ability_number));
			v.push_back(std::to_string(e.ability_time_seconds));
			v.push_back(std::to_string(e.ability_time_minutes));
			v.push_back(std::to_string(e.ability_time_hours));
			v.push_back("FROM_UNIXTIME(" + (e.created_at > 0 ? std::to_string(e.created_at) : "null") + ")");

			insert_chunks.push_back("(" + Strings::Implode(",", v) + ")");
		}

		std::vector<std::string> v;

		auto results = db.QueryDatabase(
			fmt::format(
				"{} VALUES {}",
				BaseInsert(),
				Strings::Implode(",", insert_chunks)
			)
		);

		return (results.Success() ? results.RowsAffected() : 0);
	}

	static std::vector<CharacterPersonas> All(Database& db)
	{
		std::vector<CharacterPersonas> all_entries;

		auto results = db.QueryDatabase(
			fmt::format(
				"{}",
				BaseSelect()
			)
		);

		all_entries.reserve(results.RowCount());

		for (auto row = results.begin(); row != results.end(); ++row) {
			CharacterPersonas e{};

			e.character_id         = row[0] ? static_cast<uint32_t>(strtoul(row[0], nullptr, 10)) : 0;
			e.class_id             = row[1] ? static_cast<uint8_t>(strtoul(row[1], nullptr, 10)) : 0;
			e.race                 = row[2] ? static_cast<uint16_t>(strtoul(row[2], nullptr, 10)) : 0;
			e.level                = row[3] ? static_cast<uint32_t>(strtoul(row[3], nullptr, 10)) : 0;
			e.level2               = row[4] ? static_cast<uint8_t>(strtoul(row[4], nullptr, 10)) : 0;
			e.exp                  = row[5] ? static_cast<uint32_t>(strtoul(row[5], nullptr, 10)) : 0;
			e.points               = row[6] ? static_cast<uint32_t>(strtoul(row[6], nullptr, 10)) : 0;
			e.aa_points            = row[7] ? static_cast<uint32_t>(strtoul(row[7], nullptr, 10)) : 0;
			e.aa_exp               = row[8] ? static_cast<uint32_t>(strtoul(row[8], nullptr, 10)) : 0;
			e.aa_points_spent      = row[9] ? static_cast<uint32_t>(strtoul(row[9], nullptr, 10)) : 0;
			e.e_aa_effects         = row[10] ? static_cast<uint32_t>(strtoul(row[10], nullptr, 10)) : 0;
			e.e_percent_to_aa      = row[11] ? static_cast<uint32_t>(strtoul(row[11], nullptr, 10)) : 0;
			e.e_expended_aa_spent  = row[12] ? static_cast<uint32_t>(strtoul(row[12], nullptr, 10)) : 0;
			e.deity                = row[13] ? static_cast<uint32_t>(strtoul(row[13], nullptr, 10)) : 0;
			e.gender               = row[14] ? static_cast<uint8_t>(strtoul(row[14], nullptr, 10)) : 0;
			e.face                 = row[15] ? static_cast<uint32_t>(strtoul(row[15], nullptr, 10)) : 0;
			e.hair_color           = row[16] ? static_cast<uint8_t>(strtoul(row[16], nullptr, 10)) : 0;
			e.hair_style           = row[17] ? static_cast<uint8_t>(strtoul(row[17], nullptr, 10)) : 0;
			e.beard                = row[18] ? static_cast<uint8_t>(strtoul(row[18], nullptr, 10)) : 0;
			e.beard_color          = row[19] ? static_cast<uint8_t>(strtoul(row[19], nullptr, 10)) : 0;
			e.eye_color_1          = row[20] ? static_cast<uint8_t>(strtoul(row[20], nullptr, 10)) : 0;
			e.eye_color_2          = row[21] ? static_cast<uint8_t>(strtoul(row[21], nullptr, 10)) : 0;
			e.drakkin_heritage     = row[22] ? static_cast<uint32_t>(strtoul(row[22], nullptr, 10)) : 0;
			e.drakkin_tattoo       = row[23] ? static_cast<uint32_t>(strtoul(row[23], nullptr, 10)) : 0;
			e.drakkin_details      = row[24] ? static_cast<uint32_t>(strtoul(row[24], nullptr, 10)) : 0;
			e.str                  = row[25] ? static_cast<uint32_t>(strtoul(row[25], nullptr, 10)) : 0;
			e.sta                  = row[26] ? static_cast<uint32_t>(strtoul(row[26], nullptr, 10)) : 0;
			e.cha                  = row[27] ? static_cast<uint32_t>(strtoul(row[27], nullptr, 10)) : 0;
			e.dex                  = row[28] ? static_cast<uint32_t>(strtoul(row[28], nullptr, 10)) : 0;
			e.int_                 = row[29] ? static_cast<uint32_t>(strtoul(row[29], nullptr, 10)) : 0;
			e.agi                  = row[30] ? static_cast<uint32_t>(strtoul(row[30], nullptr, 10)) : 0;
			e.wis                  = row[31] ? static_cast<uint32_t>(strtoul(row[31], nullptr, 10)) : 0;
			e.cur_hp               = row[32] ? static_cast<uint32_t>(strtoul(row[32], nullptr, 10)) : 0;
			e.mana                 = row[33] ? static_cast<uint32_t>(strtoul(row[33], nullptr, 10)) : 0;
			e.endurance            = row[34] ? static_cast<uint32_t>(strtoul(row[34], nullptr, 10)) : 0;
			e.ability_number       = row[35] ? static_cast<uint8_t>(strtoul(row[35], nullptr, 10)) : 0;
			e.ability_time_seconds = row[36] ? static_cast<uint8_t>(strtoul(row[36], nullptr, 10)) : 0;
			e.ability_time_minutes = row[37] ? static_cast<uint8_t>(strtoul(row[37], nullptr, 10)) : 0;
			e.ability_time_hours   = row[38] ? static_cast<uint8_t>(strtoul(row[38], nullptr, 10)) : 0;
			e.created_at           = strtoll(row[39] ? row[39] : "-1", nullptr, 10);

			all_entries.push_back(e);
		}

		return all_entries;
	}

	static std::vector<CharacterPersonas> GetWhere(Database& db, const std::string &where_filter)
	{
		std::vector<CharacterPersonas> all_entries;

		auto results = db.QueryDatabase(
			fmt::format(
				"{} WHERE {}",
				BaseSelect(),
				where_filter
			)
		);

		all_entries.reserve(results.RowCount());

		for (auto row = results.begin(); row != results.end(); ++row) {
			CharacterPersonas e{};

			e.character_id         = row[0] ? static_cast<uint32_t>(strtoul(row[0], nullptr, 10)) : 0;
			e.class_id             = row[1] ? static_cast<uint8_t>(strtoul(row[1], nullptr, 10)) : 0;
			e.race                 = row[2] ? static_cast<uint16_t>(strtoul(row[2], nullptr, 10)) : 0;
			e.level                = row[3] ? static_cast<uint32_t>(strtoul(row[3], nullptr, 10)) : 0;
			e.level2               = row[4] ? static_cast<uint8_t>(strtoul(row[4], nullptr, 10)) : 0;
			e.exp                  = row[5] ? static_cast<uint32_t>(strtoul(row[5], nullptr, 10)) : 0;
			e.points               = row[6] ? static_cast<uint32_t>(strtoul(row[6], nullptr, 10)) : 0;
			e.aa_points            = row[7] ? static_cast<uint32_t>(strtoul(row[7], nullptr, 10)) : 0;
			e.aa_exp               = row[8] ? static_cast<uint32_t>(strtoul(row[8], nullptr, 10)) : 0;
			e.aa_points_spent      = row[9] ? static_cast<uint32_t>(strtoul(row[9], nullptr, 10)) : 0;
			e.e_aa_effects         = row[10] ? static_cast<uint32_t>(strtoul(row[10], nullptr, 10)) : 0;
			e.e_percent_to_aa      = row[11] ? static_cast<uint32_t>(strtoul(row[11], nullptr, 10)) : 0;
			e.e_expended_aa_spent  = row[12] ? static_cast<uint32_t>(strtoul(row[12], nullptr, 10)) : 0;
			e.deity                = row[13] ? static_cast<uint32_t>(strtoul(row[13], nullptr, 10)) : 0;
			e.gender               = row[14] ? static_cast<uint8_t>(strtoul(row[14], nullptr, 10)) : 0;
			e.face                 = row[15] ? static_cast<uint32_t>(strtoul(row[15], nullptr, 10)) : 0;
			e.hair_color           = row[16] ? static_cast<uint8_t>(strtoul(row[16], nullptr, 10)) : 0;
			e.hair_style           = row[17] ? static_cast<uint8_t>(strtoul(row[17], nullptr, 10)) : 0;
			e.beard                = row[18] ? static_cast<uint8_t>(strtoul(row[18], nullptr, 10)) : 0;
			e.beard_color          = row[19] ? static_cast<uint8_t>(strtoul(row[19], nullptr, 10)) : 0;
			e.eye_color_1          = row[20] ? static_cast<uint8_t>(strtoul(row[20], nullptr, 10)) : 0;
			e.eye_color_2          = row[21] ? static_cast<uint8_t>(strtoul(row[21], nullptr, 10)) : 0;
			e.drakkin_heritage     = row[22] ? static_cast<uint32_t>(strtoul(row[22], nullptr, 10)) : 0;
			e.drakkin_tattoo       = row[23] ? static_cast<uint32_t>(strtoul(row[23], nullptr, 10)) : 0;
			e.drakkin_details      = row[24] ? static_cast<uint32_t>(strtoul(row[24], nullptr, 10)) : 0;
			e.str                  = row[25] ? static_cast<uint32_t>(strtoul(row[25], nullptr, 10)) : 0;
			e.sta                  = row[26] ? static_cast<uint32_t>(strtoul(row[26], nullptr, 10)) : 0;
			e.cha                  = row[27] ? static_cast<uint32_t>(strtoul(row[27], nullptr, 10)) : 0;
			e.dex                  = row[28] ? static_cast<uint32_t>(strtoul(row[28], nullptr, 10)) : 0;
			e.int_                 = row[29] ? static_cast<uint32_t>(strtoul(row[29], nullptr, 10)) : 0;
			e.agi                  = row[30] ? static_cast<uint32_t>(strtoul(row[30], nullptr, 10)) : 0;
			e.wis                  = row[31] ? static_cast<uint32_t>(strtoul(row[31], nullptr, 10)) : 0;
			e.cur_hp               = row[32] ? static_cast<uint32_t>(strtoul(row[32], nullptr, 10)) : 0;
			e.mana                 = row[33] ? static_cast<uint32_t>(strtoul(row[33], nullptr, 10)) : 0;
			e.endurance            = row[34] ? static_cast<uint32_t>(strtoul(row[34], nullptr, 10)) : 0;
			e.ability_number       = row[35] ? static_cast<uint8_t>(strtoul(row[35], nullptr, 10)) : 0;
			e.ability_time_seconds = row[36] ? static_cast<uint8_t>(strtoul(row[36], nullptr, 10)) : 0;
			e.ability_time_minutes = row[37] ? static_cast<uint8_t>(strtoul(row[37], nullptr, 10)) : 0;
			e.ability_time_hours   = row[38] ? static_cast<uint8_t>(strtoul(row[38], nullptr, 10)) : 0;
			e.created_at           = strtoll(row[39] ? row[39] : "-1", nullptr, 10);

			all_entries.push_back(e);
		}

		return all_entries;
	}

	static int DeleteWhere(Database& db, const std::string &where_filter)
	{
		auto results = db.QueryDatabase(
			fmt::format(
				"DELETE FROM {} WHERE {}",
				TableName(),
				where_filter
			)
		);

		return (results.Success() ? results.RowsAffected() : 0);
	}

	static int Truncate(Database& db)
	{
		auto results = db.QueryDatabase(
			fmt::format(
				"TRUNCATE TABLE {}",
				TableName()
			)
		);

		return (results.Success() ? results.RowsAffected() : 0);
	}

	static int64 GetMaxId(Database& db)
	{
		auto results = db.QueryDatabase(
			fmt::format(
				"SELECT COALESCE(MAX({}), 0) FROM {}",
				PrimaryKey(),
				TableName()
			)
		);

		return (results.Success() && results.begin()[0] ? strtoll(results.begin()[0], nullptr, 10) : 0);
	}

	static int64 Count(Database& db, const std::string &where_filter = "")
	{
		auto results = db.QueryDatabase(
			fmt::format(
				"SELECT COUNT(*) FROM {} {}",
				TableName(),
				(where_filter.empty() ? "" : "WHERE " + where_filter)
			)
		);

		return (results.Success() && results.begin()[0] ? strtoll(results.begin()[0], nullptr, 10) : 0);
	}

	static std::string BaseReplace()
	{
		return fmt::format(
			"REPLACE INTO {} ({}) ",
			TableName(),
			ColumnsRaw()
		);
	}

	static int ReplaceOne(
		Database& db,
		const CharacterPersonas &e
	)
	{
		std::vector<std::string> v;

		v.push_back(std::to_string(e.character_id));
		v.push_back(std::to_string(e.class_id));
		v.push_back(std::to_string(e.race));
		v.push_back(std::to_string(e.level));
		v.push_back(std::to_string(e.level2));
		v.push_back(std::to_string(e.exp));
		v.push_back(std::to_string(e.points));
		v.push_back(std::to_string(e.aa_points));
		v.push_back(std::to_string(e.aa_exp));
		v.push_back(std::to_string(e.aa_points_spent));
		v.push_back(std::to_string(e.e_aa_effects));
		v.push_back(std::to_string(e.e_percent_to_aa));
		v.push_back(std::to_string(e.e_expended_aa_spent));
		v.push_back(std::to_string(e.deity));
		v.push_back(std::to_string(e.gender));
		v.push_back(std::to_string(e.face));
		v.push_back(std::to_string(e.hair_color));
		v.push_back(std::to_string(e.hair_style));
		v.push_back(std::to_string(e.beard));
		v.push_back(std::to_string(e.beard_color));
		v.push_back(std::to_string(e.eye_color_1));
		v.push_back(std::to_string(e.eye_color_2));
		v.push_back(std::to_string(e.drakkin_heritage));
		v.push_back(std::to_string(e.drakkin_tattoo));
		v.push_back(std::to_string(e.drakkin_details));
		v.push_back(std::to_string(e.str));
		v.push_back(std::to_string(e.sta));
		v.push_back(std::to_string(e.cha));
		v.push_back(std::to_string(e.dex));
		v.push_back(std::to_string(e.int_));
		v.push_back(std::to_string(e.agi));
		v.push_back(std::to_string(e.wis));
		v.push_back(std::to_string(e.cur_hp));
		v.push_back(std::to_string(e.mana));
		v.push_back(std::to_string(e.endurance));
		v.push_back(std::to_string(e.ability_number));
		v.push_back(std::to_string(e.ability_time_seconds));
		v.push_back(std::to_string(e.ability_time_minutes));
		v.push_back(std::to_string(e.ability_time_hours));
		v.push_back("FROM_UNIXTIME(" + (e.created_at > 0 ? std::to_string(e.created_at) : "null") + ")");

		auto results = db.QueryDatabase(
			fmt::format(
				"{} VALUES ({})",
				BaseReplace(),
				Strings::Implode(",", v)
			)
		);

		return (results.Success() ? results.RowsAffected() : 0);
	}

	static int ReplaceMany(
		Database& db,
		const std::vector<CharacterPersonas> &entries
	)
	{
		std::vector<std::string> insert_chunks;

		for (auto &e: entries) {
			std::vector<std::string> v;

			v.push_back(std::to_string(e.character_id));
			v.push_back(std::to_string(e.class_id));
			v.push_back(std::to_string(e.race));
			v.push_back(std::to_string(e.level));
			v.push_back(std::to_string(e.level2));
			v.push_back(std::to_string(e.exp));
			v.push_back(std::to_string(e.points));
			v.push_back(std::to_string(e.aa_points));
			v.push_back(std::to_string(e.aa_exp));
			v.push_back(std::to_string(e.aa_points_spent));
			v.push_back(std::to_string(e.e_aa_effects));
			v.push_back(std::to_string(e.e_percent_to_aa));
			v.push_back(std::to_string(e.e_expended_aa_spent));
			v.push_back(std::to_string(e.deity));
			v.push_back(std::to_string(e.gender));
			v.push_back(std::to_string(e.face));
			v.push_back(std::to_string(e.hair_color));
			v.push_back(std::to_string(e.hair_style));
			v.push_back(std::to_string(e.beard));
			v.push_back(std::to_string(e.beard_color));
			v.push_back(std::to_string(e.eye_color_1));
			v.push_back(std::to_string(e.eye_color_2));
			v.push_back(std::to_string(e.drakkin_heritage));
			v.push_back(std::to_string(e.drakkin_tattoo));
			v.push_back(std::to_string(e.drakkin_details));
			v.push_back(std::to_string(e.str));
			v.push_back(std::to_string(e.sta));
			v.push_back(std::to_string(e.cha));
			v.push_back(std::to_string(e.dex));
			v.push_back(std::to_string(e.int_));
			v.push_back(std::to_string(e.agi));
			v.push_back(std::to_string(e.wis));
			v.push_back(std::to_string(e.cur_hp));
			v.push_back(std::to_string(e.mana));
			v.push_back(std::to_string(e.endurance));
			v.push_back(std::to_string(e.ability_number));
			v.push_back(std::to_string(e.ability_time_seconds));
			v.push_back(std::to_string(e.ability_time_minutes));
			v.push_back(std::to_string(e.ability_time_hours));
			v.push_back("FROM_UNIXTIME(" + (e.created_at > 0 ? std::to_string(e.created_at) : "null") + ")");

			insert_chunks.push_back("(" + Strings::Implode(",", v) + ")");
		}

		std::vector<std::string> v;

		auto results = db.QueryDatabase(
			fmt::format(
				"{} VALUES {}",
				BaseReplace(),
				Strings::Implode(",", insert_chunks)
			)
		);

		return (results.Success() ? results.RowsAffected() : 0);
	}
};
