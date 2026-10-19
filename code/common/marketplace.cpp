#include "common/marketplace.h"

#include "common/eq_packet.h"
#include "common/eq_packet_structs.h"
#include "common/eqemu_logsys.h"
#include "common/strings.h"

#include <algorithm>

Marketplace *Marketplace::SetContentDatabase(Database *db)
{
	m_content_database = db;

	return this;
}

void Marketplace::LoadOffers()
{
	if (!m_content_database) {
		return;
	}

	m_offers = MarketplaceOffersRepository::GetCatalog(*m_content_database);

	m_packages.clear();
	for (const auto &row : MarketplacePackageContentsRepository::All(*m_content_database)) {
		m_packages[row.package_item_id].push_back(row);
	}

	LogInfo(
		"Loaded [{}] Marketplace offers and [{}] packages",
		Strings::Commify(static_cast<uint32>(m_offers.size())),
		Strings::Commify(static_cast<uint32>(m_packages.size()))
	);
}

const Marketplace::Offer *Marketplace::FindOffer(uint32 offer_id) const
{
	const auto it = std::lower_bound(
		m_offers.begin(),
		m_offers.end(),
		offer_id,
		[](const Offer &offer, uint32 id) { return offer.offer.id < id; }
	);

	return it != m_offers.end() && it->offer.id == offer_id ? &*it : nullptr;
}

const std::vector<Marketplace::PackageContent> *Marketplace::FindPackage(uint32 item_id) const
{
	const auto it = m_packages.find(item_id);
	return it != m_packages.end() ? &it->second : nullptr;
}

std::unique_ptr<EQApplicationPacket> Marketplace::CatalogPacket() const
{
	auto outapp = std::make_unique<EQApplicationPacket>(
		OP_MarketplaceCatalog,
		sizeof(MarketplaceCatalog_Struct) + m_offers.size() * sizeof(MarketplaceOffer_Struct)
	);

	auto catalog = reinterpret_cast<MarketplaceCatalog_Struct *>(outapp->pBuffer);
	catalog->offer_count = static_cast<uint32>(m_offers.size());

	for (size_t index = 0; index < m_offers.size(); ++index) {
		const auto &offer = m_offers[index];
		auto       &out   = catalog->offers[index];

		out.offer_id = offer.offer.id;
		out.item_id  = offer.offer.item_id;
		out.icon     = offer.icon;
		out.price    = offer.offer.price;
		out.is_new   = offer.offer.is_new ? 1 : 0;
		out.is_featured = offer.offer.is_featured ? 1 : 0;
		strn0cpy(out.name, offer.item_name.c_str(), sizeof(out.name));
		strn0cpy(out.category, offer.offer.category.c_str(), sizeof(out.category));
		strn0cpy(out.description, offer.offer.description.c_str(), sizeof(out.description));
	}

	return outapp;
}

std::unique_ptr<EQApplicationPacket> Marketplace::OfferStatePacket() const
{
	auto outapp = std::make_unique<EQApplicationPacket>(
		OP_MarketplaceOfferState,
		sizeof(MarketplaceOfferState_Struct) + m_offers.size() * sizeof(MarketplaceOfferStateEntry_Struct)
	);

	auto state = reinterpret_cast<MarketplaceOfferState_Struct *>(outapp->pBuffer);
	state->offer_count = static_cast<uint32>(m_offers.size());

	for (size_t index = 0; index < m_offers.size(); ++index) {
		state->offers[index].offer_id        = m_offers[index].offer.id;
		state->offers[index].preview_item_id = m_offers[index].offer.item_id;
	}

	return outapp;
}
