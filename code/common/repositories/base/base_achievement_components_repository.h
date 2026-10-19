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

class BaseAchievementComponentsRepository {
public:
	struct AchievementComponents {
		uint32_t    achievement_id;
		uint8_t     component_type;
		uint32_t    component_id;
		uint32_t    sequence;
		std::string description;
		uint8_t     event_type;
		uint32_t    event_id;
		uint32_t    event_count;
	};

	static std::string PrimaryKey()
	{
		return std::string("achievement_id");
	}

	static std::vector<std::string> Columns()
	{
		return {
			"achievement_id",
			"component_type",
			"component_id",
			"sequence",
			"description",
			"event_type",
			"event_id",
			"event_count",
		};
	}

	static std::vector<std::string> SelectColumns()
	{
		return {
			"achievement_id",
			"component_type",
			"component_id",
			"sequence",
			"description",
			"event_type",
			"event_id",
			"event_count",
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
		return std::string("achievement_components");
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

	static AchievementComponents NewEntity()
	{
		AchievementComponents e{};

		e.achievement_id = 0;
		e.component_type = 0;
		e.component_id   = 0;
		e.sequence       = 0;
		e.description    = "";
		e.event_type     = 0;
		e.event_id       = 0;
		e.event_count    = 0;

		return e;
	}

	static AchievementComponents GetAchievementComponents(
		const std::vector<AchievementComponents> &achievement_componentss,
		int achievement_components_id
	)
	{
		for (auto &achievement_components : achievement_componentss) {
			if (achievement_components.achievement_id == achievement_components_id) {
				return achievement_components;
			}
		}

		return NewEntity();
	}

	static AchievementComponents FindOne(
		Database& db,
		int achievement_components_id
	)
	{
		auto results = db.QueryDatabase(
			fmt::format(
				"{} WHERE {} = {} LIMIT 1",
				BaseSelect(),
				PrimaryKey(),
				achievement_components_id
			)
		);

		auto row = results.begin();
		if (results.RowCount() == 1) {
			AchievementComponents e{};

			e.achievement_id = row[0] ? static_cast<uint32_t>(strtoul(row[0], nullptr, 10)) : 0;
			e.component_type = row[1] ? static_cast<uint8_t>(strtoul(row[1], nullptr, 10)) : 0;
			e.component_id   = row[2] ? static_cast<uint32_t>(strtoul(row[2], nullptr, 10)) : 0;
			e.sequence       = row[3] ? static_cast<uint32_t>(strtoul(row[3], nullptr, 10)) : 0;
			e.description    = row[4] ? row[4] : "";
			e.event_type     = row[5] ? static_cast<uint8_t>(strtoul(row[5], nullptr, 10)) : 0;
			e.event_id       = row[6] ? static_cast<uint32_t>(strtoul(row[6], nullptr, 10)) : 0;
			e.event_count    = row[7] ? static_cast<uint32_t>(strtoul(row[7], nullptr, 10)) : 0;

			return e;
		}

		return NewEntity();
	}

	static int DeleteOne(
		Database& db,
		int achievement_components_id
	)
	{
		auto results = db.QueryDatabase(
			fmt::format(
				"DELETE FROM {} WHERE {} = {}",
				TableName(),
				PrimaryKey(),
				achievement_components_id
			)
		);

		return (results.Success() ? results.RowsAffected() : 0);
	}

	static int UpdateOne(
		Database& db,
		const AchievementComponents &e
	)
	{
		std::vector<std::string> v;

		auto columns = Columns();

		v.push_back(columns[0] + " = " + std::to_string(e.achievement_id));
		v.push_back(columns[1] + " = " + std::to_string(e.component_type));
		v.push_back(columns[2] + " = " + std::to_string(e.component_id));
		v.push_back(columns[3] + " = " + std::to_string(e.sequence));
		v.push_back(columns[4] + " = '" + Strings::Escape(e.description) + "'");
		v.push_back(columns[5] + " = " + std::to_string(e.event_type));
		v.push_back(columns[6] + " = " + std::to_string(e.event_id));
		v.push_back(columns[7] + " = " + std::to_string(e.event_count));

		auto results = db.QueryDatabase(
			fmt::format(
				"UPDATE {} SET {} WHERE {} = {}",
				TableName(),
				Strings::Implode(", ", v),
				PrimaryKey(),
				e.achievement_id
			)
		);

		return (results.Success() ? results.RowsAffected() : 0);
	}

	static AchievementComponents InsertOne(
		Database& db,
		AchievementComponents e
	)
	{
		std::vector<std::string> v;

		v.push_back(std::to_string(e.achievement_id));
		v.push_back(std::to_string(e.component_type));
		v.push_back(std::to_string(e.component_id));
		v.push_back(std::to_string(e.sequence));
		v.push_back("'" + Strings::Escape(e.description) + "'");
		v.push_back(std::to_string(e.event_type));
		v.push_back(std::to_string(e.event_id));
		v.push_back(std::to_string(e.event_count));

		auto results = db.QueryDatabase(
			fmt::format(
				"{} VALUES ({})",
				BaseInsert(),
				Strings::Implode(",", v)
			)
		);

		if (results.Success()) {
			e.achievement_id = results.LastInsertedID();
			return e;
		}

		e = NewEntity();

		return e;
	}

	static int InsertMany(
		Database& db,
		const std::vector<AchievementComponents> &entries
	)
	{
		std::vector<std::string> insert_chunks;

		for (auto &e: entries) {
			std::vector<std::string> v;

			v.push_back(std::to_string(e.achievement_id));
			v.push_back(std::to_string(e.component_type));
			v.push_back(std::to_string(e.component_id));
			v.push_back(std::to_string(e.sequence));
			v.push_back("'" + Strings::Escape(e.description) + "'");
			v.push_back(std::to_string(e.event_type));
			v.push_back(std::to_string(e.event_id));
			v.push_back(std::to_string(e.event_count));

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

	static std::vector<AchievementComponents> All(Database& db)
	{
		std::vector<AchievementComponents> all_entries;

		auto results = db.QueryDatabase(
			fmt::format(
				"{}",
				BaseSelect()
			)
		);

		all_entries.reserve(results.RowCount());

		for (auto row = results.begin(); row != results.end(); ++row) {
			AchievementComponents e{};

			e.achievement_id = row[0] ? static_cast<uint32_t>(strtoul(row[0], nullptr, 10)) : 0;
			e.component_type = row[1] ? static_cast<uint8_t>(strtoul(row[1], nullptr, 10)) : 0;
			e.component_id   = row[2] ? static_cast<uint32_t>(strtoul(row[2], nullptr, 10)) : 0;
			e.sequence       = row[3] ? static_cast<uint32_t>(strtoul(row[3], nullptr, 10)) : 0;
			e.description    = row[4] ? row[4] : "";
			e.event_type     = row[5] ? static_cast<uint8_t>(strtoul(row[5], nullptr, 10)) : 0;
			e.event_id       = row[6] ? static_cast<uint32_t>(strtoul(row[6], nullptr, 10)) : 0;
			e.event_count    = row[7] ? static_cast<uint32_t>(strtoul(row[7], nullptr, 10)) : 0;

			all_entries.push_back(e);
		}

		return all_entries;
	}

	static std::vector<AchievementComponents> GetWhere(Database& db, const std::string &where_filter)
	{
		std::vector<AchievementComponents> all_entries;

		auto results = db.QueryDatabase(
			fmt::format(
				"{} WHERE {}",
				BaseSelect(),
				where_filter
			)
		);

		all_entries.reserve(results.RowCount());

		for (auto row = results.begin(); row != results.end(); ++row) {
			AchievementComponents e{};

			e.achievement_id = row[0] ? static_cast<uint32_t>(strtoul(row[0], nullptr, 10)) : 0;
			e.component_type = row[1] ? static_cast<uint8_t>(strtoul(row[1], nullptr, 10)) : 0;
			e.component_id   = row[2] ? static_cast<uint32_t>(strtoul(row[2], nullptr, 10)) : 0;
			e.sequence       = row[3] ? static_cast<uint32_t>(strtoul(row[3], nullptr, 10)) : 0;
			e.description    = row[4] ? row[4] : "";
			e.event_type     = row[5] ? static_cast<uint8_t>(strtoul(row[5], nullptr, 10)) : 0;
			e.event_id       = row[6] ? static_cast<uint32_t>(strtoul(row[6], nullptr, 10)) : 0;
			e.event_count    = row[7] ? static_cast<uint32_t>(strtoul(row[7], nullptr, 10)) : 0;

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
		const AchievementComponents &e
	)
	{
		std::vector<std::string> v;

		v.push_back(std::to_string(e.achievement_id));
		v.push_back(std::to_string(e.component_type));
		v.push_back(std::to_string(e.component_id));
		v.push_back(std::to_string(e.sequence));
		v.push_back("'" + Strings::Escape(e.description) + "'");
		v.push_back(std::to_string(e.event_type));
		v.push_back(std::to_string(e.event_id));
		v.push_back(std::to_string(e.event_count));

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
		const std::vector<AchievementComponents> &entries
	)
	{
		std::vector<std::string> insert_chunks;

		for (auto &e: entries) {
			std::vector<std::string> v;

			v.push_back(std::to_string(e.achievement_id));
			v.push_back(std::to_string(e.component_type));
			v.push_back(std::to_string(e.component_id));
			v.push_back(std::to_string(e.sequence));
			v.push_back("'" + Strings::Escape(e.description) + "'");
			v.push_back(std::to_string(e.event_type));
			v.push_back(std::to_string(e.event_id));
			v.push_back(std::to_string(e.event_count));

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
