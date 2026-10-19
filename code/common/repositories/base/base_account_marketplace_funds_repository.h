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

class BaseAccountMarketplaceFundsRepository {
public:
	struct AccountMarketplaceFunds {
		uint32_t account_id;
		uint32_t funds;
	};

	static std::string PrimaryKey()
	{
		return std::string("account_id");
	}

	static std::vector<std::string> Columns()
	{
		return {
			"account_id",
			"funds",
		};
	}

	static std::vector<std::string> SelectColumns()
	{
		return {
			"account_id",
			"funds",
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
		return std::string("account_marketplace_funds");
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

	static AccountMarketplaceFunds NewEntity()
	{
		AccountMarketplaceFunds e{};

		e.account_id = 0;
		e.funds      = 0;

		return e;
	}

	static AccountMarketplaceFunds GetAccountMarketplaceFunds(
		const std::vector<AccountMarketplaceFunds> &account_marketplace_fundss,
		int account_marketplace_funds_id
	)
	{
		for (auto &account_marketplace_funds : account_marketplace_fundss) {
			if (account_marketplace_funds.account_id == account_marketplace_funds_id) {
				return account_marketplace_funds;
			}
		}

		return NewEntity();
	}

	static AccountMarketplaceFunds FindOne(
		Database& db,
		int account_marketplace_funds_id
	)
	{
		auto results = db.QueryDatabase(
			fmt::format(
				"{} WHERE {} = {} LIMIT 1",
				BaseSelect(),
				PrimaryKey(),
				account_marketplace_funds_id
			)
		);

		auto row = results.begin();
		if (results.RowCount() == 1) {
			AccountMarketplaceFunds e{};

			e.account_id = row[0] ? static_cast<uint32_t>(strtoul(row[0], nullptr, 10)) : 0;
			e.funds      = row[1] ? static_cast<uint32_t>(strtoul(row[1], nullptr, 10)) : 0;

			return e;
		}

		return NewEntity();
	}

	static int DeleteOne(
		Database& db,
		int account_marketplace_funds_id
	)
	{
		auto results = db.QueryDatabase(
			fmt::format(
				"DELETE FROM {} WHERE {} = {}",
				TableName(),
				PrimaryKey(),
				account_marketplace_funds_id
			)
		);

		return (results.Success() ? results.RowsAffected() : 0);
	}

	static int UpdateOne(
		Database& db,
		const AccountMarketplaceFunds &e
	)
	{
		std::vector<std::string> v;

		auto columns = Columns();

		v.push_back(columns[0] + " = " + std::to_string(e.account_id));
		v.push_back(columns[1] + " = " + std::to_string(e.funds));

		auto results = db.QueryDatabase(
			fmt::format(
				"UPDATE {} SET {} WHERE {} = {}",
				TableName(),
				Strings::Implode(", ", v),
				PrimaryKey(),
				e.account_id
			)
		);

		return (results.Success() ? results.RowsAffected() : 0);
	}

	static AccountMarketplaceFunds InsertOne(
		Database& db,
		AccountMarketplaceFunds e
	)
	{
		std::vector<std::string> v;

		v.push_back(std::to_string(e.account_id));
		v.push_back(std::to_string(e.funds));

		auto results = db.QueryDatabase(
			fmt::format(
				"{} VALUES ({})",
				BaseInsert(),
				Strings::Implode(",", v)
			)
		);

		if (results.Success()) {
			e.account_id = results.LastInsertedID();
			return e;
		}

		e = NewEntity();

		return e;
	}

	static int InsertMany(
		Database& db,
		const std::vector<AccountMarketplaceFunds> &entries
	)
	{
		std::vector<std::string> insert_chunks;

		for (auto &e: entries) {
			std::vector<std::string> v;

			v.push_back(std::to_string(e.account_id));
			v.push_back(std::to_string(e.funds));

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

	static std::vector<AccountMarketplaceFunds> All(Database& db)
	{
		std::vector<AccountMarketplaceFunds> all_entries;

		auto results = db.QueryDatabase(
			fmt::format(
				"{}",
				BaseSelect()
			)
		);

		all_entries.reserve(results.RowCount());

		for (auto row = results.begin(); row != results.end(); ++row) {
			AccountMarketplaceFunds e{};

			e.account_id = row[0] ? static_cast<uint32_t>(strtoul(row[0], nullptr, 10)) : 0;
			e.funds      = row[1] ? static_cast<uint32_t>(strtoul(row[1], nullptr, 10)) : 0;

			all_entries.push_back(e);
		}

		return all_entries;
	}

	static std::vector<AccountMarketplaceFunds> GetWhere(Database& db, const std::string &where_filter)
	{
		std::vector<AccountMarketplaceFunds> all_entries;

		auto results = db.QueryDatabase(
			fmt::format(
				"{} WHERE {}",
				BaseSelect(),
				where_filter
			)
		);

		all_entries.reserve(results.RowCount());

		for (auto row = results.begin(); row != results.end(); ++row) {
			AccountMarketplaceFunds e{};

			e.account_id = row[0] ? static_cast<uint32_t>(strtoul(row[0], nullptr, 10)) : 0;
			e.funds      = row[1] ? static_cast<uint32_t>(strtoul(row[1], nullptr, 10)) : 0;

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
		const AccountMarketplaceFunds &e
	)
	{
		std::vector<std::string> v;

		v.push_back(std::to_string(e.account_id));
		v.push_back(std::to_string(e.funds));

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
		const std::vector<AccountMarketplaceFunds> &entries
	)
	{
		std::vector<std::string> insert_chunks;

		for (auto &e: entries) {
			std::vector<std::string> v;

			v.push_back(std::to_string(e.account_id));
			v.push_back(std::to_string(e.funds));

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
