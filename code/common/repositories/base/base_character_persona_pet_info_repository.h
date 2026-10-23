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

class BaseCharacterPersonaPetInfoRepository {
public:
	struct CharacterPersonaPetInfo {
		uint32_t    character_id;
		uint8_t     class_id;
		int32_t     pet;
		std::string petname;
		int32_t     petpower;
		int32_t     spell_id;
		int32_t     hp;
		int32_t     mana;
		float       size;
		int8_t      taunting;
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
			"pet",
			"petname",
			"petpower",
			"spell_id",
			"hp",
			"mana",
			"size",
			"taunting",
		};
	}

	static std::vector<std::string> SelectColumns()
	{
		return {
			"character_id",
			"class_id",
			"pet",
			"petname",
			"petpower",
			"spell_id",
			"hp",
			"mana",
			"size",
			"taunting",
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
		return std::string("character_persona_pet_info");
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

	static CharacterPersonaPetInfo NewEntity()
	{
		CharacterPersonaPetInfo e{};

		e.character_id = 0;
		e.class_id     = 0;
		e.pet          = 0;
		e.petname      = "";
		e.petpower     = 0;
		e.spell_id     = 0;
		e.hp           = 0;
		e.mana         = 0;
		e.size         = 0;
		e.taunting     = 0;

		return e;
	}

	static CharacterPersonaPetInfo GetCharacterPersonaPetInfo(
		const std::vector<CharacterPersonaPetInfo> &character_persona_pet_infos,
		int character_persona_pet_info_id
	)
	{
		for (auto &character_persona_pet_info : character_persona_pet_infos) {
			if (character_persona_pet_info.character_id == character_persona_pet_info_id) {
				return character_persona_pet_info;
			}
		}

		return NewEntity();
	}

	static CharacterPersonaPetInfo FindOne(
		Database& db,
		int character_persona_pet_info_id
	)
	{
		auto results = db.QueryDatabase(
			fmt::format(
				"{} WHERE {} = {} LIMIT 1",
				BaseSelect(),
				PrimaryKey(),
				character_persona_pet_info_id
			)
		);

		auto row = results.begin();
		if (results.RowCount() == 1) {
			CharacterPersonaPetInfo e{};

			e.character_id = row[0] ? static_cast<uint32_t>(strtoul(row[0], nullptr, 10)) : 0;
			e.class_id     = row[1] ? static_cast<uint8_t>(strtoul(row[1], nullptr, 10)) : 0;
			e.pet          = row[2] ? static_cast<int32_t>(atoi(row[2])) : 0;
			e.petname      = row[3] ? row[3] : "";
			e.petpower     = row[4] ? static_cast<int32_t>(atoi(row[4])) : 0;
			e.spell_id     = row[5] ? static_cast<int32_t>(atoi(row[5])) : 0;
			e.hp           = row[6] ? static_cast<int32_t>(atoi(row[6])) : 0;
			e.mana         = row[7] ? static_cast<int32_t>(atoi(row[7])) : 0;
			e.size         = row[8] ? strtof(row[8], nullptr) : 0;
			e.taunting     = row[9] ? static_cast<int8_t>(atoi(row[9])) : 0;

			return e;
		}

		return NewEntity();
	}

	static int DeleteOne(
		Database& db,
		int character_persona_pet_info_id
	)
	{
		auto results = db.QueryDatabase(
			fmt::format(
				"DELETE FROM {} WHERE {} = {}",
				TableName(),
				PrimaryKey(),
				character_persona_pet_info_id
			)
		);

		return (results.Success() ? results.RowsAffected() : 0);
	}

	static int UpdateOne(
		Database& db,
		const CharacterPersonaPetInfo &e
	)
	{
		std::vector<std::string> v;

		auto columns = Columns();

		v.push_back(columns[0] + " = " + std::to_string(e.character_id));
		v.push_back(columns[1] + " = " + std::to_string(e.class_id));
		v.push_back(columns[2] + " = " + std::to_string(e.pet));
		v.push_back(columns[3] + " = '" + Strings::Escape(e.petname) + "'");
		v.push_back(columns[4] + " = " + std::to_string(e.petpower));
		v.push_back(columns[5] + " = " + std::to_string(e.spell_id));
		v.push_back(columns[6] + " = " + std::to_string(e.hp));
		v.push_back(columns[7] + " = " + std::to_string(e.mana));
		v.push_back(columns[8] + " = " + std::to_string(e.size));
		v.push_back(columns[9] + " = " + std::to_string(e.taunting));

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

	static CharacterPersonaPetInfo InsertOne(
		Database& db,
		CharacterPersonaPetInfo e
	)
	{
		std::vector<std::string> v;

		v.push_back(std::to_string(e.character_id));
		v.push_back(std::to_string(e.class_id));
		v.push_back(std::to_string(e.pet));
		v.push_back("'" + Strings::Escape(e.petname) + "'");
		v.push_back(std::to_string(e.petpower));
		v.push_back(std::to_string(e.spell_id));
		v.push_back(std::to_string(e.hp));
		v.push_back(std::to_string(e.mana));
		v.push_back(std::to_string(e.size));
		v.push_back(std::to_string(e.taunting));

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
		const std::vector<CharacterPersonaPetInfo> &entries
	)
	{
		std::vector<std::string> insert_chunks;

		for (auto &e: entries) {
			std::vector<std::string> v;

			v.push_back(std::to_string(e.character_id));
			v.push_back(std::to_string(e.class_id));
			v.push_back(std::to_string(e.pet));
			v.push_back("'" + Strings::Escape(e.petname) + "'");
			v.push_back(std::to_string(e.petpower));
			v.push_back(std::to_string(e.spell_id));
			v.push_back(std::to_string(e.hp));
			v.push_back(std::to_string(e.mana));
			v.push_back(std::to_string(e.size));
			v.push_back(std::to_string(e.taunting));

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

	static std::vector<CharacterPersonaPetInfo> All(Database& db)
	{
		std::vector<CharacterPersonaPetInfo> all_entries;

		auto results = db.QueryDatabase(
			fmt::format(
				"{}",
				BaseSelect()
			)
		);

		all_entries.reserve(results.RowCount());

		for (auto row = results.begin(); row != results.end(); ++row) {
			CharacterPersonaPetInfo e{};

			e.character_id = row[0] ? static_cast<uint32_t>(strtoul(row[0], nullptr, 10)) : 0;
			e.class_id     = row[1] ? static_cast<uint8_t>(strtoul(row[1], nullptr, 10)) : 0;
			e.pet          = row[2] ? static_cast<int32_t>(atoi(row[2])) : 0;
			e.petname      = row[3] ? row[3] : "";
			e.petpower     = row[4] ? static_cast<int32_t>(atoi(row[4])) : 0;
			e.spell_id     = row[5] ? static_cast<int32_t>(atoi(row[5])) : 0;
			e.hp           = row[6] ? static_cast<int32_t>(atoi(row[6])) : 0;
			e.mana         = row[7] ? static_cast<int32_t>(atoi(row[7])) : 0;
			e.size         = row[8] ? strtof(row[8], nullptr) : 0;
			e.taunting     = row[9] ? static_cast<int8_t>(atoi(row[9])) : 0;

			all_entries.push_back(e);
		}

		return all_entries;
	}

	static std::vector<CharacterPersonaPetInfo> GetWhere(Database& db, const std::string &where_filter)
	{
		std::vector<CharacterPersonaPetInfo> all_entries;

		auto results = db.QueryDatabase(
			fmt::format(
				"{} WHERE {}",
				BaseSelect(),
				where_filter
			)
		);

		all_entries.reserve(results.RowCount());

		for (auto row = results.begin(); row != results.end(); ++row) {
			CharacterPersonaPetInfo e{};

			e.character_id = row[0] ? static_cast<uint32_t>(strtoul(row[0], nullptr, 10)) : 0;
			e.class_id     = row[1] ? static_cast<uint8_t>(strtoul(row[1], nullptr, 10)) : 0;
			e.pet          = row[2] ? static_cast<int32_t>(atoi(row[2])) : 0;
			e.petname      = row[3] ? row[3] : "";
			e.petpower     = row[4] ? static_cast<int32_t>(atoi(row[4])) : 0;
			e.spell_id     = row[5] ? static_cast<int32_t>(atoi(row[5])) : 0;
			e.hp           = row[6] ? static_cast<int32_t>(atoi(row[6])) : 0;
			e.mana         = row[7] ? static_cast<int32_t>(atoi(row[7])) : 0;
			e.size         = row[8] ? strtof(row[8], nullptr) : 0;
			e.taunting     = row[9] ? static_cast<int8_t>(atoi(row[9])) : 0;

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
		const CharacterPersonaPetInfo &e
	)
	{
		std::vector<std::string> v;

		v.push_back(std::to_string(e.character_id));
		v.push_back(std::to_string(e.class_id));
		v.push_back(std::to_string(e.pet));
		v.push_back("'" + Strings::Escape(e.petname) + "'");
		v.push_back(std::to_string(e.petpower));
		v.push_back(std::to_string(e.spell_id));
		v.push_back(std::to_string(e.hp));
		v.push_back(std::to_string(e.mana));
		v.push_back(std::to_string(e.size));
		v.push_back(std::to_string(e.taunting));

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
		const std::vector<CharacterPersonaPetInfo> &entries
	)
	{
		std::vector<std::string> insert_chunks;

		for (auto &e: entries) {
			std::vector<std::string> v;

			v.push_back(std::to_string(e.character_id));
			v.push_back(std::to_string(e.class_id));
			v.push_back(std::to_string(e.pet));
			v.push_back("'" + Strings::Escape(e.petname) + "'");
			v.push_back(std::to_string(e.petpower));
			v.push_back(std::to_string(e.spell_id));
			v.push_back(std::to_string(e.hp));
			v.push_back(std::to_string(e.mana));
			v.push_back(std::to_string(e.size));
			v.push_back(std::to_string(e.taunting));

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
