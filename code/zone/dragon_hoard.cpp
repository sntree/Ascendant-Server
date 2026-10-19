#include "common/features.h"
#include "common/rulesys.h"
#include "common/strings.h"

#include "zone/client.h"
#include "zone/string_ids.h"
#include "zone/zonedb.h"

#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// TOB+ Dragon's Hoard: extra storage used at a banker. Rows live in the inventory table and in the inventory
// profile at DRAGON_HOARD_BEGIN + row, so they save and load like any other slot. The client names a row by the
// item's serial number.

namespace {
	// the client refuses Krono, so the server does too
	constexpr uint32 KRONO_ITEM_ID = 88888;

	bool HasSerial(const EQ::ItemInstance *inst, uint64 item_index)
	{
		return inst && static_cast<uint32>(inst->GetSerialNumber()) == item_index;
	}

	bool CanDeposit(EQ::ItemInstance *inst)
	{
		const auto item = inst->GetItem();
		if (!item) {
			return false;
		}

		const bool has_contents = inst->IsNoneEmptyContainer();
		const bool has_augments = inst->IsAugmented();
		const bool stays_in_inventory = item->NoRent == 0 || item->EvolvingItem || item->ID == KRONO_ITEM_ID;

		return !has_contents && !has_augments && !stays_in_inventory;
	}

	bool HoardHoldsLore(const std::map<int16, EQ::ItemInstance *> &rows, const EQ::ItemData *item)
	{
		if (!item->LoreFlag) {
			return false;
		}

		for (const auto &[slot_id, row] : rows) {
			if (!row || !row->GetItem()) {
				continue;
			}

			const bool same_item  = item->LoreGroup == -1 && row->GetID() == item->ID;
			const bool same_group = item->LoreGroup > 0 && row->GetItem()->LoreGroup == item->LoreGroup;
			if (same_item || same_group) {
				return true;
			}
		}

		return false;
	}

	// a stack joins a row of the same item that has room for it, anything else takes the first free row
	bool FindHoardRow(
		const std::map<int16, EQ::ItemInstance *> &rows,
		EQ::ItemInstance *inst,
		int capacity,
		EQ::ItemInstance *&merge_row,
		int16 &hoard_slot
	)
	{
		const auto *item          = inst->GetItem();
		const int   deposit_count = inst->IsStackable() ? std::max<int>(inst->GetCharges(), 1) : 1;

		merge_row  = nullptr;
		hoard_slot = INVALID_INDEX;
		if (inst->IsStackable()) {
			for (const auto &[slot_id, row] : rows) {
				if (row && row->GetID() == item->ID && row->GetCharges() + deposit_count <= item->StackSize) {
					merge_row  = row;
					hoard_slot = slot_id;
					return true;
				}
			}
		}

		for (int index = 0; index < capacity; index++) {
			if (!rows.contains(EQ::invslot::DragonHoardSlot(index))) {
				hoard_slot = EQ::invslot::DragonHoardSlot(index);
				return true;
			}
		}

		return false;
	}
}

bool Client::IsDragonHoardEnabled()
{
	return ClientVersion() >= EQ::versions::ClientVersion::TOB && GetDragonHoardCapacity() > 0;
}

int Client::GetDragonHoardCapacity()
{
	return std::clamp(RuleI(Character, DragonHoardSlots), 0, static_cast<int>(EQ::invslot::DRAGON_HOARD_SIZE));
}

bool Client::IsInBankerRange()
{
	uint32 distance = 0;
	return entity_list.GetClosestBanker(this, distance) && distance <= USE_NPC_RANGE2;
}

void Client::SendDragonHoardAction(uint32 action, uint64 item_index, uint32 value)
{
	auto outapp = new EQApplicationPacket(OP_DragonHoardAction, sizeof(DragonHoard_Struct));
	auto out    = reinterpret_cast<DragonHoard_Struct *>(outapp->pBuffer);

	out->action     = action;
	out->item_index = item_index;
	out->value      = value;

	QueuePacket(outapp);
	safe_delete(outapp);
}

void Client::SendDragonHoardList()
{
	// the window shows rows in the order they arrive
	std::vector<std::pair<std::string, int16>> rows;
	for (const auto &[slot_id, row] : m_inv.GetDragonHoardItems()) {
		if (row && row->GetItem()) {
			rows.emplace_back(Strings::ToLower(row->GetItem()->Name), slot_id);
		}
	}

	std::ranges::sort(rows);

	for (const auto &[name, slot_id] : rows) {
		SendItemPacket(slot_id, m_inv[slot_id], ItemPacketDragonHoard);
	}

	SendDragonHoardAction(DragonHoardListEnd);
}

void Client::FinishDragonHoardRequest()
{
	SendDragonHoardAction(DragonHoardDone);
	SendDragonHoardList();
}

bool Client::CanStoreInDragonHoard(EQ::ItemInstance *inst)
{
	return IsDragonHoardEnabled() && CanDeposit(inst);
}

// everything StoreInDragonHoard checks before it writes, so callers can ask first
bool Client::DragonHoardHasRoomFor(EQ::ItemInstance *inst)
{
	if (!CanStoreInDragonHoard(inst)) {
		return false;
	}

	const auto &rows = m_inv.GetDragonHoardItems();
	if (HoardHoldsLore(rows, inst->GetItem())) {
		return false;
	}

	EQ::ItemInstance *merge_row  = nullptr;
	int16             hoard_slot = INVALID_INDEX;
	return FindHoardRow(rows, inst, GetDragonHoardCapacity(), merge_row, hoard_slot);
}

bool Client::StoreInDragonHoard(EQ::ItemInstance *inst, bool from_cursor)
{
	if (!CanDeposit(inst)) {
		return false;
	}

	const auto item = inst->GetItem();
	const auto &rows = m_inv.GetDragonHoardItems();

	if (HoardHoldsLore(rows, item)) {
		MessageString(Chat::Red, DRAGON_HOARD_LORE);
		return false;
	}

	const int deposit_count = inst->IsStackable() ? std::max<int>(inst->GetCharges(), 1) : 1;

	EQ::ItemInstance *merge_row  = nullptr;
	int16             hoard_slot = INVALID_INDEX;
	if (!FindHoardRow(rows, inst, GetDragonHoardCapacity(), merge_row, hoard_slot)) {
		MessageString(Chat::Red, DRAGON_HOARD_FULL);
		return false;
	}

	// the row after the move
	std::unique_ptr<EQ::ItemInstance> merged;
	if (merge_row) {
		merged.reset(merge_row->Clone());
		merged->SetCharges(merge_row->GetCharges() + deposit_count);
	}

	const EQ::ItemInstance *hoard_row = merge_row ? merged.get() : inst;

	bool saved = false;
	if (from_cursor) {
		// written together with the cursor behind the deposited item
		std::list<EQ::ItemInstance *> queue(std::next(m_inv.cursor_cbegin()), m_inv.cursor_cend());
		auto queue_begin = queue.cbegin();
		auto queue_end   = queue.cend();

		saved = database.SaveDragonHoardMove(CharacterID(), hoard_slot, hoard_row, queue_begin, queue_end);
	} else {
		saved = database.SaveInventory(CharacterID(), hoard_row, hoard_slot);
	}

	if (!saved) {
		return false;
	}

	LogInventory(
		"[{}] deposited [{}] x [{}] ([{}]) into Dragon's Hoard slot [{}]",
		GetCleanName(),
		deposit_count,
		item->Name,
		item->ID,
		hoard_slot
	);

	if (merge_row) {
		merge_row->SetCharges(merged->GetCharges());
	} else {
		m_inv.PutItem(hoard_slot, *inst);
	}

	return true;
}

void Client::DepositDragonHoardItem(uint64 item_index)
{
	// a refusal still has to answer, or the client stays busy
	const auto refuse = [this]() {
		SendDragonHoardAction(DragonHoardDone);
	};

	const auto cursor = m_inv.GetCursorItem();
	if (!IsInBankerRange() || !cursor || !HasSerial(cursor, item_index) || !StoreInDragonHoard(cursor, true)) {
		return refuse();
	}

	// the cursor item is gone after this
	DeleteItemInInventory(EQ::invslot::slotCursor, 0, true, false);
	SendCursorBuffer();

	FinishDragonHoardRequest();
}

void Client::WithdrawDragonHoardItem(uint64 item_index, uint32 quantity)
{
	const auto refuse = [this]() {
		SendDragonHoardAction(DragonHoardDone);
	};

	int16 hoard_slot = INVALID_INDEX;
	for (const auto &[slot_id, row] : m_inv.GetDragonHoardItems()) {
		if (HasSerial(row, item_index)) {
			hoard_slot = slot_id;
			break;
		}
	}

	const auto row = hoard_slot == INVALID_INDEX ? nullptr : m_inv.GetItem(hoard_slot);
	if (!IsInBankerRange() || !row || !row->GetItem()) {
		return refuse();
	}

	const auto item = row->GetItem();

	if (!m_inv.CursorEmpty()) {
		MessageString(Chat::Red, CURSOR_ITEM_BLOCKS);
		return refuse();
	}

	if (CheckLoreConflict(item)) {
		MessageString(Chat::Red, DUP_LORE);
		return refuse();
	}

	const int row_count = row->IsStackable() ? std::max<int>(row->GetCharges(), 1) : 1;
	if (row->IsStackable() && quantity == 0) {
		return refuse();
	}

	const int  take_count = row->IsStackable() ? std::min<int>(quantity, row_count) : 1;
	const bool whole_row  = take_count >= row_count;

	// a whole row keeps its serial, a part of a stack is a new item
	std::unique_ptr<EQ::ItemInstance> taken(whole_row ? row->Clone() : database.CreateItem(item, take_count));
	if (!taken) {
		return refuse();
	}

	std::unique_ptr<EQ::ItemInstance> remaining;
	if (!whole_row) {
		remaining.reset(row->Clone());
		remaining->SetCharges(row_count - take_count);
	}

	std::list<EQ::ItemInstance *> queue{taken.get()};
	auto queue_begin = queue.cbegin();
	auto queue_end   = queue.cend();

	if (!database.SaveDragonHoardMove(CharacterID(), hoard_slot, remaining.get(), queue_begin, queue_end)) {
		return refuse();
	}

	LogInventory(
		"[{}] withdrew [{}] x [{}] ([{}]) from Dragon's Hoard slot [{}]",
		GetCleanName(),
		take_count,
		item->Name,
		item->ID,
		hoard_slot
	);

	if (whole_row) {
		m_inv.DeleteItem(hoard_slot);
	} else {
		row->SetCharges(remaining->GetCharges());
	}

	m_inv.PushCursor(*taken);
	SendItemPacket(EQ::invslot::slotCursor, m_inv.GetCursorItem(), ItemPacketTrade);
	SendDragonHoardAction(DragonHoardRetrieve, item_index, take_count);

	FinishDragonHoardRequest();
}
