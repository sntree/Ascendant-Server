#include "common/marketplace.h"
#include "common/repositories/account_marketplace_funds_repository.h"
#include "common/rulesys.h"
#include "common/spdat.h"

#include "zone/client.h"
#include "zone/horse.h"
#include "zone/string_ids.h"
#include "zone/zonedb.h"

#include <algorithm>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// TOB+ Marketplace window: offers from marketplace_offers, paid with the account's Marketplace Coins

namespace {
	// the window never asks for more than this in one purchase
	constexpr uint32 MAX_PURCHASE_QUANTITY = 10;

	// model preview values Live sends: neutral gender, size 1, texture 2 for an illusion on the player
	constexpr uint8 MODEL_GENDER_DEFAULT = 2;
	constexpr float MODEL_SIZE_DEFAULT   = 1.0f;
	constexpr uint8 MODEL_TEXTURE_SELF   = 2;
}

bool Client::IsMarketplaceEnabled()
{
	return RuleB(Character, EnableMarketplace) && ClientVersion() >= EQ::versions::ClientVersion::TOB;
}

uint32 Client::GetMarketplaceMaxCoins()
{
	return static_cast<uint32>(std::max(RuleI(Character, MarketplaceMaxCoins), 0));
}

uint32 Client::GetMarketplaceCoins()
{
	return AccountMarketplaceFundsRepository::GetFunds(database, AccountID());
}

void Client::LogMarketplaceCoins(int64 amount, const std::string &reason, const std::string &detail)
{
	AccountMarketplaceFundsRepository::AddLedgerEntry(
		database,
		AccountID(),
		CharacterID(),
		amount,
		GetMarketplaceCoins(),
		reason,
		detail
	);
}

bool Client::AddMarketplaceCoins(uint32 amount, const std::string &reason, const std::string &detail)
{
	const uint32 before = GetMarketplaceCoins();
	const bool   added  = AccountMarketplaceFundsRepository::AddFunds(database, AccountID(), amount, GetMarketplaceMaxCoins());
	if (added) {
		LogMarketplaceCoins(static_cast<int64>(GetMarketplaceCoins()) - before, reason, detail);
		if (IsMarketplaceEnabled()) {
			SendMarketplaceBalance();
		}
	}

	return added;
}

bool Client::RemoveMarketplaceCoins(uint32 amount, const std::string &reason, const std::string &detail)
{
	const bool removed = AccountMarketplaceFundsRepository::RemoveFunds(database, AccountID(), amount);
	if (removed) {
		LogMarketplaceCoins(-static_cast<int64>(amount), reason, detail);
		if (IsMarketplaceEnabled()) {
			SendMarketplaceBalance();
		}
	}

	return removed;
}

bool Client::SetMarketplaceCoins(uint32 amount, const std::string &reason, const std::string &detail)
{
	const uint32 before = GetMarketplaceCoins();
	const bool   set    = AccountMarketplaceFundsRepository::SetFunds(database, AccountID(), amount, GetMarketplaceMaxCoins());
	if (set) {
		LogMarketplaceCoins(static_cast<int64>(GetMarketplaceCoins()) - before, reason, detail);
		if (IsMarketplaceEnabled()) {
			SendMarketplaceBalance();
		}
	}

	return set;
}

void Client::SendMarketplaceCatalog()
{
	auto outapp = Marketplace::Instance()->CatalogPacket().release();
	FastQueuePacket(&outapp);
}

void Client::SendMarketplaceBalance()
{
	auto outapp = new EQApplicationPacket(OP_MarketplaceBalance, sizeof(MarketplaceBalance_Struct));
	auto out    = reinterpret_cast<MarketplaceBalance_Struct *>(outapp->pBuffer);
	out->balance = static_cast<int32>(std::min(GetMarketplaceCoins(), AccountMarketplaceFundsRepository::MAX_FUNDS));
	FastQueuePacket(&outapp);
}

void Client::SendMarketplaceItemPreview(uint32 item_id)
{
	const auto item = database.GetItem(item_id);
	if (!item) {
		return;
	}

	if (IsValidSpell(item->Click.Effect) && SendMarketplaceModelPreview(static_cast<uint16>(item->Click.Effect))) {
		return;
	}

	std::unique_ptr<EQ::ItemInstance> inst(database.CreateItem(item));
	if (!inst) {
		return;
	}

	const std::string serialized = inst->Serialize(EQ::invslot::SLOT_INVALID);
	EQApplicationPacket outapp(OP_MarketplaceItemPreview, reinterpret_cast<const unsigned char *>(serialized.data()), serialized.size());
	QueuePacket(&outapp);
}

bool Client::SendMarketplaceModelPreview(uint16 spell_id)
{
	const auto &spell = spells[spell_id];

	MarketplaceModelPreview_Struct model{};
	model.gender = MODEL_GENDER_DEFAULT;
	model.size   = MODEL_SIZE_DEFAULT;

	bool found = false;
	for (int index = 0; index < EFFECT_COUNT && !found; ++index) {
		switch (spell.effect_id[index]) {
			case SpellEffect::Illusion: {
				model.race = static_cast<uint32>(std::max(spell.base_value[index], 0));
				if (spell.target_type != ST_Pet) {
					model.spawn_id     = GetID();
					model.texture      = MODEL_TEXTURE_SELF;
					model.helm_texture = MODEL_TEXTURE_SELF;
					strn0cpy(model.name, GetName(), sizeof(model.name));
				}
				found = model.race != 0;
				break;
			}
			case SpellEffect::Familiar: {
				PetRecord record{};
				if (!content_db.GetPetEntry(spell.teleport_zone, &record)) {
					break;
				}

				const auto npc = content_db.LoadNPCTypesData(record.npc_type);
				if (npc) {
					model.race         = npc->race;
					model.gender       = npc->gender;
					model.texture      = npc->texture;
					model.helm_texture = npc->helmtexture;
					model.size         = npc->size > 0.0f ? npc->size : MODEL_SIZE_DEFAULT;
					found              = true;
				}
				break;
			}
			case SpellEffect::SummonHorse: {
				const auto horse = Horse::GetHorseType(spell_id);
				if (horse) {
					model.race         = horse->race;
					model.gender       = horse->gender;
					model.texture      = horse->texture;
					model.helm_texture = horse->helmtexture;
					found              = true;
				}
				break;
			}
			default:
				break;
		}
	}

	if (!found) {
		return false;
	}

	EQApplicationPacket outapp(OP_MarketplaceModelPreview, sizeof(MarketplaceModelPreview_Struct));
	memcpy(outapp.pBuffer, &model, sizeof(model));
	QueuePacket(&outapp);

	return true;
}

void Client::SendMarketplacePurchaseResult(uint32 offer_id, MarketplacePurchaseResult result)
{
	auto outapp = new EQApplicationPacket(OP_MarketplacePurchaseResult, sizeof(MarketplacePurchaseResult_Struct));
	auto out    = reinterpret_cast<MarketplacePurchaseResult_Struct *>(outapp->pBuffer);
	out->offer_id = offer_id;
	out->result   = result;
	FastQueuePacket(&outapp);
}

// gives quantity of an item on the cursor, as stacks when it stacks; returns how many were given
uint32 Client::SummonMarketplaceItems(const EQ::ItemData *item, uint32 quantity)
{
	uint32 granted = 0;
	if (item->Stackable) {
		const uint32 stack_size = static_cast<uint32>(std::max<int16>(item->StackSize, 1));
		while (granted < quantity) {
			const uint32 stack = std::min(stack_size, quantity - granted);
			if (!SummonItem(item->ID, static_cast<int16>(stack))) {
				break;
			}

			granted += stack;
		}

		return granted;
	}

	for (; granted < quantity; ++granted) {
		if (!SummonItem(item->ID)) {
			break;
		}
	}

	return granted;
}

bool Client::OpenMarketplacePackage(int16 inventory_slot)
{
	if (!RuleB(Character, EnableMarketplace)) {
		return false;
	}

	const auto inst = m_inv.GetItem(inventory_slot);
	if (!inst || !inst->GetItem()) {
		return false;
	}

	const auto contents = Marketplace::Instance()->FindPackage(inst->GetID());
	if (!contents) {
		return false;
	}

	const uint32 package_id = inst->GetID();
	const uint8  level      = GetLevel();
	const uint32 class_bit  = GetPlayerClassBit(GetClass());

	std::vector<std::pair<const EQ::ItemData *, uint32>> gifts;
	for (const auto &row : *contents) {
		const bool level_ok = (!row.min_level || level >= row.min_level) && (!row.max_level || level <= row.max_level);
		const bool class_ok = !row.classes || row.classes == Class::ALL_CLASSES_BITMASK || (row.classes & class_bit);
		const auto item     = database.GetItem(row.item_id);
		if (level_ok && class_ok && item && row.quantity) {
			gifts.emplace_back(item, row.quantity);
		}
	}

	// the package stays when it has nothing for this character
	if (gifts.empty()) {
		MessageString(Chat::Red, CANNOT_USE_ITEM);
		return true;
	}

	DeleteItemInInventory(inventory_slot, 1, true);
	for (const auto &[item, quantity] : gifts) {
		SummonMarketplaceItems(item, quantity);
	}

	LogInventory("[{}] opened Marketplace package [{}] for [{}] items", GetCleanName(), package_id, gifts.size());

	return true;
}

void Client::PurchaseMarketplaceOffer(uint32 offer_id, uint32 quantity)
{
	const auto offer = Marketplace::Instance()->FindOffer(offer_id);
	const auto item  = offer ? database.GetItem(offer->offer.item_id) : nullptr;
	if (!item) {
		SendMarketplacePurchaseResult(offer_id, MarketplacePurchaseInvalidItem);
		return;
	}

	quantity = std::clamp<uint32>(quantity, 1, MAX_PURCHASE_QUANTITY);
	if (item->LoreFlag && (quantity > 1 || CheckLoreConflict(item))) {
		SendMarketplacePurchaseResult(offer_id, MarketplacePurchaseDuplicateLore);
		return;
	}

	const uint64 unit_price = offer->offer.price;
	const uint64 cost       = unit_price * quantity;
	if (cost > AccountMarketplaceFundsRepository::MAX_FUNDS ||
		!AccountMarketplaceFundsRepository::RemoveFunds(database, AccountID(), static_cast<uint32>(cost))) {
		SendMarketplacePurchaseResult(offer_id, MarketplacePurchaseInsufficientFunds);
		return;
	}

	const uint32 granted = SummonMarketplaceItems(item, quantity);

	const uint64 paid = unit_price * granted;
	if (paid < cost) {
		AccountMarketplaceFundsRepository::AddFunds(database, AccountID(), static_cast<uint32>(cost - paid));
	}

	if (!granted) {
		SendMarketplacePurchaseResult(offer_id, MarketplacePurchaseInventoryFull);
		SendMarketplaceBalance();
		return;
	}

	LogMarketplaceCoins(
		-static_cast<int64>(paid),
		"purchase",
		fmt::format("bought {} x {} (item {}, offer {})", granted, item->Name, item->ID, offer_id)
	);

	LogInventory(
		"[{}] bought [{}] x [{}] ([{}]) from the Marketplace for [{}] funds",
		GetCleanName(),
		granted,
		item->Name,
		item->ID,
		paid
	);

	SendMarketplacePurchaseResult(offer_id, MarketplacePurchaseSuccess);
	SendMarketplaceBalance();
}
