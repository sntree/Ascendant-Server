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

#include "zone/client.h"
#include "zone/masterentity.h"
#include "zone/string_ids.h"

#include "common/rulesys.h"

// TOB+ Merc tab of the Inventory window: four slots (ear, neck, shoulders, waist) for gear the active mercenary
// wears. Merc slot n is server slot MERC_EQUIPMENT_BEGIN + n, kept in the inventory table and in the inventory
// profile's merc bucket, so it saves and loads like any other slot. The client moves items with OP_MoveItem
// between the cursor and {type 25, n}, and checks the item's Classes and worn slots itself; the same checks run
// here. The gear belongs to the character and applies to whichever mercenary is active.

bool Client::IsMercEquipmentEnabled()
{
	return ClientVersion() >= EQ::versions::ClientVersion::TOB &&
		RuleB(Mercs, AllowMercs) && RuleB(Mercs, AllowMercEquipment);
}

// 0 when the item may go in the merc slot, else the client string that says why not
uint32 Client::GetMercEquipmentRefusal(int16 merc_slot_id, const EQ::ItemInstance *inst)
{
	if (!inst || !inst->GetItem() || !EQ::invslot::IsMercEquipmentSlot(merc_slot_id)) {
		return MERC_ITEM_WRONG_SLOT;
	}

	const auto item = inst->GetItem();

	if (!item->IsClassCommon() || inst->IsStackable()) {
		return MERC_ITEM_WRONG_SLOT;
	}

	// the item and every augment in it need the mercenary class bit
	if (!(item->Classes & EQ::invslot::MERC_EQUIPMENT_CLASS_MASK)) {
		return MERC_ITEM_CLASS_ONLY;
	}

	for (int i = EQ::invaug::SOCKET_BEGIN; i <= EQ::invaug::SOCKET_END; ++i) {
		const auto augment = inst->GetAugment(i);
		if (augment && augment->GetItem() && !(augment->GetItem()->Classes & EQ::invslot::MERC_EQUIPMENT_CLASS_MASK)) {
			return MERC_ITEM_CLASS_ONLY;
		}
	}

	const int worn_slot = EQ::invslot::MERC_EQUIPMENT_WORN_SLOTS[EQ::invslot::MercEquipmentIndex(merc_slot_id)];
	if (!(item->Slots & (1u << worn_slot))) {
		return MERC_ITEM_WRONG_SLOT;
	}

	return 0;
}

bool Client::MoveMercEquipmentItem(MoveItem_Struct *move_in)
{
	const int32 from_slot = static_cast<int32>(move_in->from_slot);
	const int32 to_slot   = static_cast<int32>(move_in->to_slot);

	// show the client what the server really holds in both slots
	auto refuse = [this, from_slot, to_slot]() {
		for (const int32 slot_id : { from_slot, to_slot }) {
			if (slot_id == EQ::invslot::slotCursor) {
				if (const auto cursor = m_inv.GetCursorItem()) {
					SendItemPacket(EQ::invslot::slotCursor, cursor, ItemPacketLimbo);
				}
			} else if (EQ::invslot::IsMercEquipmentSlot(slot_id)) {
				if (const auto inst = m_inv[slot_id]) {
					SendItemPacket(static_cast<int16>(slot_id), inst, ItemPacketCharInventory);
				} else {
					DeleteItemInInventory(static_cast<int16>(slot_id), 0, true);
				}
			}
		}

		return false;
	};

	// the client only drops from the cursor into a merc slot, or picks a merc slot up to the cursor
	const bool cursor_to_merc = from_slot == EQ::invslot::slotCursor && EQ::invslot::IsMercEquipmentSlot(to_slot);
	const bool merc_to_cursor = to_slot == EQ::invslot::slotCursor && EQ::invslot::IsMercEquipmentSlot(from_slot);

	if (!IsMercEquipmentEnabled() || (!cursor_to_merc && !merc_to_cursor)) {
		LogInventory("Refused merc equipment move from slot [{}] to slot [{}]", from_slot, to_slot);
		return refuse();
	}

	if (cursor_to_merc) {
		const auto cursor = m_inv.GetCursorItem();
		if (!cursor) {
			return refuse();
		}

		if (const uint32 string_id = GetMercEquipmentRefusal(static_cast<int16>(to_slot), cursor)) {
			MessageString(Chat::Red, string_id);
			return refuse();
		}
	} else {
		if (!m_inv[from_slot]) {
			return refuse();
		}

		if (!m_inv.CursorEmpty()) {
			MessageString(Chat::Red, CURSOR_ITEM_BLOCKS);
			return refuse();
		}
	}

	const int16 merc_slot_id = static_cast<int16>(cursor_to_merc ? to_slot : from_slot);
	const bool  had_item     = m_inv[merc_slot_id] != nullptr;

	EQ::InventoryProfile::SwapItemFailState fail_state = EQ::InventoryProfile::swapInvalid;
	if (!m_inv.SwapItem(static_cast<int16>(from_slot), static_cast<int16>(to_slot), fail_state)) {
		return refuse();
	}

	// the item the player now holds on the cursor was taken out of the merc slot, or the merc slot was empty
	if (cursor_to_merc && !had_item) {
		SendCursorBuffer();
	}

	database.SaveInventory(character_id, m_inv[merc_slot_id], merc_slot_id);

	auto s = m_inv.cursor_cbegin(), e = m_inv.cursor_cend();
	database.SaveCursor(character_id, s, e);

	LogInventory("[{}] moved merc equipment from slot [{}] to slot [{}]", GetCleanName(), from_slot, to_slot);

	if (const auto merc = GetMerc()) {
		merc->RecalculateEquipmentBonuses();
		merc->SendHPUpdate();
	}

	return true;
}

// The mercenary wears the Merc tab gear of its owner. A merc that has no owner yet gets it from
// Client::SetMerc, which recalculates once the owner is linked.
void Merc::AddMercEquipmentBonuses(StatBonuses *b)
{
	if (!GetOwnerID()) {
		return;
	}

	const auto owner = entity_list.GetClientByID(GetOwnerID());
	if (!owner || !owner->IsMercEquipmentEnabled()) {
		return;
	}

	for (const auto &[slot_id, inst] : owner->GetInv().GetMercEquipmentItems()) {
		if (inst) {
			AddItemBonuses(inst, b);
		}
	}
}

void Merc::RecalculateEquipmentBonuses()
{
	// a merc at full health, mana or endurance stays full when the gear raises the maximum
	const bool full_hp   = GetHP() >= GetMaxHP();
	const bool full_mana = GetMana() >= GetMaxMana();
	const bool full_end  = GetEndurance() >= GetMaxEndurance();

	CalcBonuses();

	if (full_hp) {
		SetHP(GetMaxHP());
	}

	if (full_mana) {
		SetMana(GetMaxMana());
	}

	if (full_end) {
		SetEndurance(static_cast<int32>(GetMaxEndurance()));
	}
}
