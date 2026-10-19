#pragma once

#include "common/repositories/marketplace_offers_repository.h"
#include "common/repositories/marketplace_package_contents_repository.h"
#include "common/types.h"

#include <memory>
#include <unordered_map>
#include <vector>

class EQApplicationPacket;

// TOB+ Marketplace window: the offers are content, loaded once at boot and sent to every TOB client that asks
class Marketplace {
public:
	using Offer = MarketplaceOffersRepository::CatalogOffer;

	void LoadOffers();
	const Offer *FindOffer(uint32 offer_id) const;

	// Live packages: items whose click spell is a placeholder; opening one hands out these rows
	using PackageContent = MarketplacePackageContentsRepository::MarketplacePackageContents;
	const std::vector<PackageContent> *FindPackage(uint32 item_id) const;
	std::unique_ptr<EQApplicationPacket> CatalogPacket() const;
	std::unique_ptr<EQApplicationPacket> OfferStatePacket() const;

	Marketplace *SetContentDatabase(Database *db);

	static Marketplace *Instance()
	{
		static Marketplace instance;
		return &instance;
	}

private:
	Database           *m_content_database{};
	std::vector<Offer> m_offers; // sorted by offer id
	std::unordered_map<uint32, std::vector<PackageContent>> m_packages;
};
