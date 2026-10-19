#pragma once

#include "common/repositories/base/base_account_marketplace_funds_repository.h"

#include "common/database.h"
#include "common/strings.h"

#include <algorithm>
#include <limits>

class AccountMarketplaceFundsRepository: public BaseAccountMarketplaceFundsRepository {
public:

    /**
     * This file was auto generated and can be modified and extended upon
     *
     * Base repository methods are automatically
     * generated in the "base" version of this repository. The base repository
     * is immutable and to be left untouched, while methods in this class
     * are used as extension methods for more specific persistence-layer
     * accessors or mutators.
     *
     * Base Methods (Subject to be expanded upon in time)
     *
     * Note: Not all tables are designed appropriately to fit functionality with all base methods
     *
     * InsertOne
     * UpdateOne
     * DeleteOne
     * FindOne
     * GetWhere(std::string where_filter)
     * DeleteWhere(std::string where_filter)
     * InsertMany
     * All
     *
     * Example custom methods in a repository
     *
     * AccountMarketplaceFundsRepository::GetByZoneAndVersion(int zone_id, int zone_version)
     * AccountMarketplaceFundsRepository::GetWhereNeverExpires()
     * AccountMarketplaceFundsRepository::GetWhereXAndY()
     * AccountMarketplaceFundsRepository::DeleteWhereXAndY()
     *
     * Most of the above could be covered by base methods, but if you as a developer
     * find yourself re-using logic for other parts of the code, its best to just make a
     * method that can be re-used easily elsewhere especially if it can use a base repository
     * method and encapsulate filters there
     */

	// Custom extended repository methods here

	// the Marketplace window shows funds as a signed 32 bit value
	static constexpr uint32_t MAX_FUNDS = std::numeric_limits<int32_t>::max();

	static uint32_t GetFunds(Database &db, uint32_t account_id)
	{
		return FindOne(db, account_id).funds;
	}

	// max_funds: the server's own limit, never above MAX_FUNDS
	static bool SetFunds(Database &db, uint32_t account_id, uint32_t funds, uint32_t max_funds = MAX_FUNDS)
	{
		auto e = NewEntity();
		e.account_id = account_id;
		e.funds      = std::min({funds, max_funds, MAX_FUNDS});

		return ReplaceOne(db, e) > 0;
	}

	// single statements, so two characters of one account can't spend the same funds
	static bool AddFunds(Database &db, uint32_t account_id, uint32_t amount, uint32_t max_funds = MAX_FUNDS)
	{
		const uint32_t cap = std::min(max_funds, MAX_FUNDS);
		const auto results = db.QueryDatabase(
			fmt::format(
				"INSERT INTO `{}` (`account_id`, `funds`) VALUES ({}, {}) "
				"ON DUPLICATE KEY UPDATE `funds` = LEAST(`funds` + {}, {})",
				TableName(),
				account_id,
				std::min(amount, cap),
				amount,
				cap
			)
		);

		return results.Success();
	}

	// account_marketplace_ledger: one row per coin change, for auditing balances
	static bool AddLedgerEntry(
		Database &db,
		uint32_t account_id,
		uint32_t character_id,
		int64_t amount,
		uint32_t balance,
		const std::string &reason,
		const std::string &detail
	)
	{
		const auto results = db.QueryDatabase(
			fmt::format(
				"INSERT INTO `account_marketplace_ledger` (`account_id`, `character_id`, `amount`, `balance`, `reason`, `detail`) "
				"VALUES ({}, {}, {}, {}, '{}', '{}')",
				account_id,
				character_id,
				std::clamp<int64_t>(amount, std::numeric_limits<int32_t>::min(), std::numeric_limits<int32_t>::max()),
				balance,
				Strings::Escape(reason.substr(0, 32)),
				Strings::Escape(detail.substr(0, 255))
			)
		);

		return results.Success();
	}

	static bool RemoveFunds(Database &db, uint32_t account_id, uint32_t amount)
	{
		if (!amount) {
			return true;
		}

		const auto results = db.QueryDatabase(
			fmt::format(
				"UPDATE `{}` SET `funds` = `funds` - {} WHERE `account_id` = {} AND `funds` >= {}",
				TableName(),
				amount,
				account_id,
				amount
			)
		);

		return results.Success() && results.RowsAffected() == 1;
	}
};
