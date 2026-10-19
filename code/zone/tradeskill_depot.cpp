#include "common/repositories/character_tradeskill_depot_repository.h"
#include "common/rulesys.h"
#include "common/strings.h"

#include "zone/client.h"
#include "zone/string_ids.h"
#include "zone/zonedb.h"

#include <algorithm>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// TOB+ Tradeskill Depot: stackable tradeskill components stored as item id -> quantity. The client keys its rows
// by item id and pulls from the depot when a combine asks for it.

namespace {
	// a row is sent as one stack, so it can't pass what a stack holds
	constexpr uint32 MAX_DEPOT_QUANTITY = std::numeric_limits<int16>::max();

	// the depot name the client's messages expect; Live sends the same word
	constexpr const char *PERSONAL_DEPOT = "personal";

	// the client only offers stackable tradeskill items for the depot
	bool IsDepotComponent(const EQ::ItemData *item)
	{
		return item && item->Stackable && item->StackSize > 1 && item->Tradeskills && item->NoRent != 0;
	}

	bool CanDeposit(EQ::ItemInstance *inst)
	{
		return IsDepotComponent(inst->GetItem()) && !inst->IsAugmented() && !inst->IsAttuned();
	}
}

bool Client::IsTradeskillDepotEnabled()
{
	return ClientVersion() >= EQ::versions::ClientVersion::TOB && GetTradeskillDepotCapacity() > 0;
}

uint32 Client::GetTradeskillDepotCapacity()
{
	return static_cast<uint32>(std::max(RuleI(Character, TradeskillDepotSlots), 0));
}

uint32 Client::GetTradeskillDepotQuantity(uint32 item_id)
{
	LoadTradeskillDepot();

	const auto row = m_tradeskill_depot.find(item_id);
	return row == m_tradeskill_depot.end() ? 0 : row->second;
}

const std::map<uint32, uint32> &Client::GetTradeskillDepotItems()
{
	LoadTradeskillDepot();
	return m_tradeskill_depot;
}

bool Client::AddTradeskillDepotItem(uint32 item_id, uint32 quantity)
{
	const auto   item   = database.GetItem(item_id);
	const uint32 stored = GetTradeskillDepotQuantity(item_id);
	if (!IsDepotComponent(item) || !quantity || quantity > MAX_DEPOT_QUANTITY - stored) {
		return false;
	}

	if (!stored && m_tradeskill_depot.size() >= GetTradeskillDepotCapacity()) {
		return false;
	}

	return SetTradeskillDepotQuantity(item_id, stored + quantity);
}

bool Client::CanStoreInTradeskillDepot(EQ::ItemInstance *inst)
{
	return IsTradeskillDepotEnabled() && CanDeposit(inst);
}

// the same limits AddTradeskillDepotItem applies, so callers can ask before they commit
bool Client::TradeskillDepotHasRoomFor(EQ::ItemInstance *inst)
{
	if (!CanStoreInTradeskillDepot(inst)) {
		return false;
	}

	const uint32 item_id  = inst->GetItem()->ID;
	const uint32 quantity = static_cast<uint32>(std::max<int16>(inst->GetCharges(), 1));
	const uint32 stored   = GetTradeskillDepotQuantity(item_id);
	if (quantity > MAX_DEPOT_QUANTITY - stored) {
		return false;
	}

	return stored || m_tradeskill_depot.size() < GetTradeskillDepotCapacity();
}

bool Client::StoreInTradeskillDepot(EQ::ItemInstance *inst, bool say_full)
{
	if (!CanStoreInTradeskillDepot(inst)) {
		return false;
	}

	const auto   item     = inst->GetItem();
	const uint32 quantity = static_cast<uint32>(std::max<int16>(inst->GetCharges(), 1));

	if (!AddTradeskillDepotItem(item->ID, quantity)) {
		if (say_full) {
			MessageString(Chat::Red, TRADESKILL_DEPOT_FULL, item->Name, PERSONAL_DEPOT);
		}
		return false;
	}

	MessageString(Chat::White, TRADESKILL_DEPOT_DEPOSITED, std::to_string(quantity).c_str(), item->Name, PERSONAL_DEPOT);
	return true;
}

bool Client::RemoveTradeskillDepotItem(uint32 item_id, uint32 quantity)
{
	const uint32 stored = GetTradeskillDepotQuantity(item_id);
	if (!quantity || quantity > stored) {
		return false;
	}

	return SetTradeskillDepotQuantity(item_id, stored - quantity);
}

bool Client::SetTradeskillDepotQuantity(uint32 item_id, uint32 quantity)
{
	if (!database.SaveTradeskillDepotRows(CharacterID(), {{item_id, quantity}})) {
		return false;
	}

	if (quantity) {
		m_tradeskill_depot[item_id] = quantity;
	} else {
		m_tradeskill_depot.erase(item_id);
	}

	if (IsTradeskillDepotEnabled()) {
		SendTradeskillDepotList();
	}

	return true;
}

void Client::LoadTradeskillDepot()
{
	if (m_tradeskill_depot_loaded) {
		return;
	}

	const auto &rows = CharacterTradeskillDepotRepository::GetWhere(
		database,
		fmt::format("`character_id` = {}", CharacterID())
	);

	for (const auto &e : rows) {
		if (e.quantity) {
			m_tradeskill_depot[e.item_id] = std::min(e.quantity, MAX_DEPOT_QUANTITY);
		}
	}

	m_tradeskill_depot_loaded = true;
}

void Client::SendTradeskillDepotAction(uint32 action, uint32 value)
{
	auto outapp = std::make_unique<EQApplicationPacket>(OP_TradeskillDepot, sizeof(TradeskillDepot_Struct));
	auto out    = reinterpret_cast<TradeskillDepot_Struct *>(outapp->pBuffer);

	out->action = action;
	out->value  = value;

	QueuePacket(outapp.get());
}

void Client::SendTradeskillDepotItems(uint32 action, const std::vector<TradeskillDepotItem_Struct> &items)
{
	const uint32 size = sizeof(TradeskillDepot_Struct) + items.size() * sizeof(TradeskillDepotItem_Struct);

	auto outapp = std::make_unique<EQApplicationPacket>(OP_TradeskillDepot, size);
	auto out    = reinterpret_cast<TradeskillDepot_Struct *>(outapp->pBuffer);

	out->action     = action;
	out->item_count = static_cast<uint32>(items.size());
	std::ranges::copy(items, out->items);

	QueuePacket(outapp.get());
}

void Client::SendTradeskillDepotList()
{
	// the list carries item pointers that only live until it is encoded, so it can't wait in the zone-in queue
	if (!Connected()) {
		return;
	}

	LoadTradeskillDepot();

	// the window shows rows in the order they arrive
	std::vector<std::pair<std::string, std::unique_ptr<EQ::ItemInstance>>> rows;
	std::vector<TradeskillDepotItem_Struct>                              items;
	rows.reserve(m_tradeskill_depot.size());
	items.reserve(m_tradeskill_depot.size());

	for (const auto &[item_id, quantity] : m_tradeskill_depot) {
		std::unique_ptr<EQ::ItemInstance> inst(database.CreateItem(item_id, static_cast<int16>(quantity)));
		if (!inst || !inst->GetItem()) {
			continue;
		}

		// the window adds a row for every serial it has not seen, so a row keeps its first serial
		const auto serial = m_tradeskill_depot_serials.try_emplace(item_id, inst->GetSerialNumber()).first;
		inst->SetSerialNumber(serial->second);

		rows.emplace_back(Strings::ToLower(inst->GetItem()->Name), std::move(inst));
		items.push_back({item_id, quantity});
	}

	std::ranges::sort(rows, {}, &decltype(rows)::value_type::first);

	const uint32 size = sizeof(ItemList_Struct) + rows.size() * sizeof(EQ::InternalSerializedItem_Struct);

	auto outapp = std::make_unique<EQApplicationPacket>(OP_ItemListWithSlotId, size);
	auto header = reinterpret_cast<ItemList_Struct *>(outapp->pBuffer);
	auto list   = reinterpret_cast<EQ::InternalSerializedItem_Struct *>(outapp->pBuffer + sizeof(ItemList_Struct));

	header->packet_type = ItemPacketTradeskillDepot;
	header->item_count  = static_cast<uint32>(rows.size());
	for (size_t index = 0; index < rows.size(); ++index) {
		list[index].slot_id = INVALID_INDEX;
		list[index].inst    = rows[index].second.get();
	}

	// the patch serializes the rows while queueing, so they only need to live until then
	QueuePacket(outapp.get());

	SendTradeskillDepotItems(TradeskillDepotItems, items);
}

void Client::DepositTradeskillDepotItem(uint64 item_index)
{
	// a refusal still has to answer, or the client stays busy
	const auto refuse = [this]() {
		SendTradeskillDepotAction(TradeskillDepotDone);
	};

	LoadTradeskillDepot();

	const auto cursor = m_inv.GetCursorItem();
	const bool cursor_matches = cursor && static_cast<uint32>(cursor->GetSerialNumber()) == static_cast<uint32>(item_index);
	if (!cursor_matches || !CanDeposit(cursor)) {
		return refuse();
	}

	const auto   item          = cursor->GetItem();
	const uint32 deposit_count = static_cast<uint32>(std::max<int16>(cursor->GetCharges(), 1));
	const uint32 stored        = GetTradeskillDepotQuantity(item->ID);
	const bool   new_row       = stored == 0;

	if (stored + deposit_count > MAX_DEPOT_QUANTITY || (new_row && m_tradeskill_depot.size() >= GetTradeskillDepotCapacity())) {
		MessageString(Chat::Red, TRADESKILL_DEPOT_FULL, item->Name, PERSONAL_DEPOT);
		return refuse();
	}

	const uint32 quantity = stored + deposit_count;

	std::list<EQ::ItemInstance *> queue(std::next(m_inv.cursor_cbegin()), m_inv.cursor_cend());
	auto queue_begin = queue.cbegin();
	auto queue_end   = queue.cend();

	if (!database.SaveTradeskillDepotMove(CharacterID(), item->ID, quantity, queue_begin, queue_end)) {
		return refuse();
	}

	LogInventory("[{}] deposited [{}] x [{}] ([{}]) into the Tradeskill Depot", GetCleanName(), deposit_count, item->Name, item->ID);

	m_tradeskill_depot[item->ID] = quantity;

	SendTradeskillDepotAction(TradeskillDepotDone);

	const std::string item_name = item->Name;

	// the cursor item is gone after this
	DeleteItemInInventory(EQ::invslot::slotCursor, 0, true, false);
	SendCursorBuffer();

	MessageString(Chat::White, TRADESKILL_DEPOT_DEPOSITED, std::to_string(deposit_count).c_str(), item_name.c_str(), PERSONAL_DEPOT);
	SendTradeskillDepotAction(TradeskillDepotDeposit);
	SendTradeskillDepotList();
}

void Client::WithdrawTradeskillDepotItem(uint32 item_id, uint32 quantity)
{
	const auto refuse = [this]() {
		SendTradeskillDepotAction(TradeskillDepotDone);
	};

	const uint32 stored = GetTradeskillDepotQuantity(item_id);
	const auto   item   = database.GetItem(item_id);
	if (!stored || !quantity || !item) {
		return refuse();
	}

	if (!m_inv.CursorEmpty()) {
		MessageString(Chat::Red, CURSOR_ITEM_BLOCKS);
		return refuse();
	}

	if (CheckLoreConflict(item)) {
		MessageString(Chat::Red, DUP_LORE);
		return refuse();
	}

	const uint32 take_count = std::min({quantity, stored, static_cast<uint32>(std::max<int16>(item->StackSize, 1))});

	std::unique_ptr<EQ::ItemInstance> taken(database.CreateItem(item, static_cast<int16>(take_count)));
	if (!taken) {
		return refuse();
	}

	const uint32 remaining = stored - take_count;

	std::list<EQ::ItemInstance *> queue{taken.get()};
	auto queue_begin = queue.cbegin();
	auto queue_end   = queue.cend();

	if (!database.SaveTradeskillDepotMove(CharacterID(), item_id, remaining, queue_begin, queue_end)) {
		return refuse();
	}

	LogInventory("[{}] withdrew [{}] x [{}] ([{}]) from the Tradeskill Depot", GetCleanName(), take_count, item->Name, item_id);

	if (remaining) {
		m_tradeskill_depot[item_id] = remaining;
	} else {
		m_tradeskill_depot.erase(item_id);
	}

	SendTradeskillDepotAction(TradeskillDepotDone);

	m_inv.PushCursor(*taken);
	SendItemPacket(EQ::invslot::slotCursor, m_inv.GetCursorItem(), ItemPacketTrade);

	SendTradeskillDepotItems(TradeskillDepotWithdraw, {{item_id, take_count}});
	MessageString(Chat::White, TRADESKILL_DEPOT_WITHDRAWN, std::to_string(take_count).c_str(), item->Name, PERSONAL_DEPOT);
	SendTradeskillDepotList();
}

bool Client::TakeTradeskillDepotComponents(const std::map<uint32, uint32> &components)
{
	if (!IsTradeskillDepotEnabled()) {
		return false;
	}

	std::map<uint32, uint32> remaining;
	for (const auto &[item_id, count] : components) {
		const uint32 stored = GetTradeskillDepotQuantity(item_id);
		if (stored < count) {
			return false;
		}

		remaining[item_id] = stored - count;
	}

	if (!database.SaveTradeskillDepotRows(CharacterID(), remaining)) {
		return false;
	}

	std::vector<TradeskillDepotItem_Struct> taken;
	taken.reserve(components.size());

	for (const auto &[item_id, count] : components) {
		const uint32 left = remaining[item_id];
		if (left) {
			m_tradeskill_depot[item_id] = left;
		} else {
			m_tradeskill_depot.erase(item_id);
		}

		taken.push_back({item_id, count});

		const auto item = database.GetItem(item_id);
		MessageString(
			Chat::Skills,
			TRADESKILL_DEPOT_CONSUMED,
			std::to_string(count).c_str(),
			item ? item->Name : std::to_string(item_id).c_str(),
			std::to_string(left).c_str(),
			PERSONAL_DEPOT
		);
	}

	SendTradeskillDepotItems(TradeskillDepotWithdraw, taken);

	return true;
}
