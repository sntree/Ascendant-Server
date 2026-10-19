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

class BaseMarketplaceOffersRepository {
public:
	struct MarketplaceOffers {
		uint32_t    id;
		uint32_t    item_id;
		std::string category;
		std::string description;
		std::string tags;
		uint32_t    price;
		uint8_t     is_new;
		uint8_t     is_featured;
		uint8_t     enabled;
	};

	static std::string PrimaryKey()
	{
		return std::string("id");
	}

	static std::vector<std::string> Columns()
	{
		return {
			"id",
			"item_id",
			"category",
			"description",
			"tags",
			"price",
			"is_new",
			"is_featured",
			"enabled",
		};
	}

	static std::vector<std::string> SelectColumns()
	{
		return {
			"id",
			"item_id",
			"category",
			"description",
			"tags",
			"price",
			"is_new",
			"is_featured",
			"enabled",
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
		return std::string("marketplace_offers");
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

	static MarketplaceOffers NewEntity()
	{
		MarketplaceOffers e{};

		e.id          = 0;
		e.item_id     = 0;
		e.category    = "";
		e.description = "";
		e.tags        = "";
		e.price       = 0;
		e.is_new      = 0;
		e.is_featured = 0;
		e.enabled     = 1;

		return e;
	}

	static MarketplaceOffers GetMarketplaceOffers(
		const std::vector<MarketplaceOffers> &marketplace_offerss,
		int marketplace_offers_id
	)
	{
		for (auto &marketplace_offers : marketplace_offerss) {
			if (marketplace_offers.id == marketplace_offers_id) {
				return marketplace_offers;
			}
		}

		return NewEntity();
	}

	static MarketplaceOffers FindOne(
		Database& db,
		int marketplace_offers_id
	)
	{
		auto results = db.QueryDatabase(
			fmt::format(
				"{} WHERE {} = {} LIMIT 1",
				BaseSelect(),
				PrimaryKey(),
				marketplace_offers_id
			)
		);

		auto row = results.begin();
		if (results.RowCount() == 1) {
			MarketplaceOffers e{};

			e.id          = row[0] ? static_cast<uint32_t>(strtoul(row[0], nullptr, 10)) : 0;
			e.item_id     = row[1] ? static_cast<uint32_t>(strtoul(row[1], nullptr, 10)) : 0;
			e.category    = row[2] ? row[2] : "";
			e.description = row[3] ? row[3] : "";
			e.tags        = row[4] ? row[4] : "";
			e.price       = row[5] ? static_cast<uint32_t>(strtoul(row[5], nullptr, 10)) : 0;
			e.is_new      = row[6] ? static_cast<uint8_t>(strtoul(row[6], nullptr, 10)) : 0;
			e.is_featured = row[7] ? static_cast<uint8_t>(strtoul(row[7], nullptr, 10)) : 0;
			e.enabled     = row[8] ? static_cast<uint8_t>(strtoul(row[8], nullptr, 10)) : 0;

			return e;
		}

		return NewEntity();
	}

	static int DeleteOne(
		Database& db,
		int marketplace_offers_id
	)
	{
		auto results = db.QueryDatabase(
			fmt::format(
				"DELETE FROM {} WHERE {} = {}",
				TableName(),
				PrimaryKey(),
				marketplace_offers_id
			)
		);

		return (results.Success() ? results.RowsAffected() : 0);
	}

	static int UpdateOne(
		Database& db,
		const MarketplaceOffers &e
	)
	{
		std::vector<std::string> v;

		auto columns = Columns();

		v.push_back(columns[0] + " = " + std::to_string(e.id));
		v.push_back(columns[1] + " = " + std::to_string(e.item_id));
		v.push_back(columns[2] + " = '" + Strings::Escape(e.category) + "'");
		v.push_back(columns[3] + " = '" + Strings::Escape(e.description) + "'");
		v.push_back(columns[4] + " = '" + Strings::Escape(e.tags) + "'");
		v.push_back(columns[5] + " = " + std::to_string(e.price));
		v.push_back(columns[6] + " = " + std::to_string(e.is_new));
		v.push_back(columns[7] + " = " + std::to_string(e.is_featured));
		v.push_back(columns[8] + " = " + std::to_string(e.enabled));

		auto results = db.QueryDatabase(
			fmt::format(
				"UPDATE {} SET {} WHERE {} = {}",
				TableName(),
				Strings::Implode(", ", v),
				PrimaryKey(),
				e.id
			)
		);

		return (results.Success() ? results.RowsAffected() : 0);
	}

	static MarketplaceOffers InsertOne(
		Database& db,
		MarketplaceOffers e
	)
	{
		std::vector<std::string> v;

		v.push_back(std::to_string(e.id));
		v.push_back(std::to_string(e.item_id));
		v.push_back("'" + Strings::Escape(e.category) + "'");
		v.push_back("'" + Strings::Escape(e.description) + "'");
		v.push_back("'" + Strings::Escape(e.tags) + "'");
		v.push_back(std::to_string(e.price));
		v.push_back(std::to_string(e.is_new));
		v.push_back(std::to_string(e.is_featured));
		v.push_back(std::to_string(e.enabled));

		auto results = db.QueryDatabase(
			fmt::format(
				"{} VALUES ({})",
				BaseInsert(),
				Strings::Implode(",", v)
			)
		);

		if (results.Success()) {
			e.id = results.LastInsertedID();
			return e;
		}

		e = NewEntity();

		return e;
	}

	static int InsertMany(
		Database& db,
		const std::vector<MarketplaceOffers> &entries
	)
	{
		std::vector<std::string> insert_chunks;

		for (auto &e: entries) {
			std::vector<std::string> v;

			v.push_back(std::to_string(e.id));
			v.push_back(std::to_string(e.item_id));
			v.push_back("'" + Strings::Escape(e.category) + "'");
			v.push_back("'" + Strings::Escape(e.description) + "'");
			v.push_back("'" + Strings::Escape(e.tags) + "'");
			v.push_back(std::to_string(e.price));
			v.push_back(std::to_string(e.is_new));
			v.push_back(std::to_string(e.is_featured));
			v.push_back(std::to_string(e.enabled));

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

	static std::vector<MarketplaceOffers> All(Database& db)
	{
		std::vector<MarketplaceOffers> all_entries;

		auto results = db.QueryDatabase(
			fmt::format(
				"{}",
				BaseSelect()
			)
		);

		all_entries.reserve(results.RowCount());

		for (auto row = results.begin(); row != results.end(); ++row) {
			MarketplaceOffers e{};

			e.id          = row[0] ? static_cast<uint32_t>(strtoul(row[0], nullptr, 10)) : 0;
			e.item_id     = row[1] ? static_cast<uint32_t>(strtoul(row[1], nullptr, 10)) : 0;
			e.category    = row[2] ? row[2] : "";
			e.description = row[3] ? row[3] : "";
			e.tags        = row[4] ? row[4] : "";
			e.price       = row[5] ? static_cast<uint32_t>(strtoul(row[5], nullptr, 10)) : 0;
			e.is_new      = row[6] ? static_cast<uint8_t>(strtoul(row[6], nullptr, 10)) : 0;
			e.is_featured = row[7] ? static_cast<uint8_t>(strtoul(row[7], nullptr, 10)) : 0;
			e.enabled     = row[8] ? static_cast<uint8_t>(strtoul(row[8], nullptr, 10)) : 0;

			all_entries.push_back(e);
		}

		return all_entries;
	}

	static std::vector<MarketplaceOffers> GetWhere(Database& db, const std::string &where_filter)
	{
		std::vector<MarketplaceOffers> all_entries;

		auto results = db.QueryDatabase(
			fmt::format(
				"{} WHERE {}",
				BaseSelect(),
				where_filter
			)
		);

		all_entries.reserve(results.RowCount());

		for (auto row = results.begin(); row != results.end(); ++row) {
			MarketplaceOffers e{};

			e.id          = row[0] ? static_cast<uint32_t>(strtoul(row[0], nullptr, 10)) : 0;
			e.item_id     = row[1] ? static_cast<uint32_t>(strtoul(row[1], nullptr, 10)) : 0;
			e.category    = row[2] ? row[2] : "";
			e.description = row[3] ? row[3] : "";
			e.tags        = row[4] ? row[4] : "";
			e.price       = row[5] ? static_cast<uint32_t>(strtoul(row[5], nullptr, 10)) : 0;
			e.is_new      = row[6] ? static_cast<uint8_t>(strtoul(row[6], nullptr, 10)) : 0;
			e.is_featured = row[7] ? static_cast<uint8_t>(strtoul(row[7], nullptr, 10)) : 0;
			e.enabled     = row[8] ? static_cast<uint8_t>(strtoul(row[8], nullptr, 10)) : 0;

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
		const MarketplaceOffers &e
	)
	{
		std::vector<std::string> v;

		v.push_back(std::to_string(e.id));
		v.push_back(std::to_string(e.item_id));
		v.push_back("'" + Strings::Escape(e.category) + "'");
		v.push_back("'" + Strings::Escape(e.description) + "'");
		v.push_back("'" + Strings::Escape(e.tags) + "'");
		v.push_back(std::to_string(e.price));
		v.push_back(std::to_string(e.is_new));
		v.push_back(std::to_string(e.is_featured));
		v.push_back(std::to_string(e.enabled));

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
		const std::vector<MarketplaceOffers> &entries
	)
	{
		std::vector<std::string> insert_chunks;

		for (auto &e: entries) {
			std::vector<std::string> v;

			v.push_back(std::to_string(e.id));
			v.push_back(std::to_string(e.item_id));
			v.push_back("'" + Strings::Escape(e.category) + "'");
			v.push_back("'" + Strings::Escape(e.description) + "'");
			v.push_back("'" + Strings::Escape(e.tags) + "'");
			v.push_back(std::to_string(e.price));
			v.push_back(std::to_string(e.is_new));
			v.push_back(std::to_string(e.is_featured));
			v.push_back(std::to_string(e.enabled));

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
