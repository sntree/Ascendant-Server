#pragma once

#include "common/repositories/base/base_marketplace_offers_repository.h"

#include "common/database.h"
#include "common/strings.h"

class MarketplaceOffersRepository: public BaseMarketplaceOffersRepository {
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
     * MarketplaceOffersRepository::GetByZoneAndVersion(int zone_id, int zone_version)
     * MarketplaceOffersRepository::GetWhereNeverExpires()
     * MarketplaceOffersRepository::GetWhereXAndY()
     * MarketplaceOffersRepository::DeleteWhereXAndY()
     *
     * Most of the above could be covered by base methods, but if you as a developer
     * find yourself re-using logic for other parts of the code, its best to just make a
     * method that can be re-used easily elsewhere especially if it can use a base repository
     * method and encapsulate filters there
     */

	// Custom extended repository methods here

	struct CatalogOffer {
		MarketplaceOffers offer;
		std::string       item_name;
		uint32_t          icon;
	};

	// enabled offers whose item exists, with the item's name and icon, in offer id order
	static std::vector<CatalogOffer> GetCatalog(Database &db)
	{
		std::vector<CatalogOffer> entries;

		auto results = db.QueryDatabase(
			fmt::format(
				"SELECT o.`id`, o.`item_id`, o.`category`, o.`description`, o.`price`, o.`is_new`, o.`is_featured`, i.`Name`, i.`icon` "
				"FROM `{}` o INNER JOIN `items` i ON i.`id` = o.`item_id` WHERE o.`enabled` = 1 ORDER BY o.`id`",
				TableName()
			)
		);

		entries.reserve(results.RowCount());

		for (auto row = results.begin(); row != results.end(); ++row) {
			CatalogOffer e{};

			e.offer.id          = row[0] ? static_cast<uint32_t>(strtoul(row[0], nullptr, 10)) : 0;
			e.offer.item_id     = row[1] ? static_cast<uint32_t>(strtoul(row[1], nullptr, 10)) : 0;
			e.offer.category    = row[2] ? row[2] : "";
			e.offer.description = row[3] ? row[3] : "";
			e.offer.price       = row[4] ? static_cast<uint32_t>(strtoul(row[4], nullptr, 10)) : 0;
			e.offer.is_new      = row[5] ? static_cast<uint8_t>(strtoul(row[5], nullptr, 10)) : 0;
			e.offer.is_featured = row[6] ? static_cast<uint8_t>(strtoul(row[6], nullptr, 10)) : 0;
			e.item_name         = row[7] ? row[7] : "";
			e.icon              = row[8] ? static_cast<uint32_t>(strtoul(row[8], nullptr, 10)) : 0;

			entries.push_back(e);
		}

		return entries;
	}
};
