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
#include "zone/string_ids.h"
#include "zone/zonedb.h"

#include "common/rulesys.h"

#include <algorithm>
#include <set>

// TOB+ key rings: mount, illusion, familiar, Hero's Forge, teleportation and activated items.
// Ring r, index i is server slot KEYRING_BEGIN + r * KEYRING_STRIDE + i; the items live in the inventory
// table and in the inventory profile's key ring bucket, so item clicks and charges work like any other slot.
// The client adds with OP_MoveItem to {ring type, -1}, takes an item out with OP_MoveItem from
// {ring type, index} to the cursor, lists the rings from OP_CharInventory and uses an item with
// OP_ItemVerifyRequest on its ring slot.
//
// TODO: Stat Mount / Stat Illusion / Stat Familiar. The packet the window sends to pick the stat item is not
// known yet (needs a Live capture). Until then the stat containers are refused in DECODE(OP_MoveItem) and the
// profile sends no selection.

namespace {
	const char *KEY_RING_NAMES[EQ::invslot::KEYRING_COUNT] = {
		"mount", "illusion", "familiar", "Hero's Forge", "teleportation", "activated"
	};

	const char *EQUIPMENT_KEY_RING_NAME = "equipment";
}

bool Client::IsKeyRingEnabled()
{
	return ClientVersion() >= EQ::versions::ClientVersion::TOB && RuleB(Character, EnableKeyRings);
}

int Client::GetKeyRingCapacity(int ring)
{
	if (ring < 0 || ring >= EQ::invslot::KEYRING_COUNT) {
		return 0;
	}

	return std::clamp(RuleI(Character, KeyRingSlots), 0, static_cast<int>(EQ::invslot::KEYRING_CLIENT_SIZE[ring]));
}

bool Client::CanAddToKeyRing(int ring, const EQ::ItemInstance *inst)
{
	if (!inst || !inst->GetItem()) {
		return false;
	}

	const auto item = inst->GetItem();

	// a ring row has no bag contents, and evolving or No Rent items have to stay in the inventory
	if (inst->IsClassBag() || item->EvolvingItem || item->NoRent == 0) {
		return false;
	}

	// older item data (PEQ) leaves some mounts, illusions and familiars at item type 0, while the client takes
	// them by their click effect too, so accept either
	const bool clicky = item->Click.Effect > 0 && IsValidSpell(item->Click.Effect);
	const auto clicks = [&](int effect) { return clicky && IsEffectInSpell(item->Click.Effect, effect); };

	// same item types the client takes for each ring
	switch (ring) {
		case 0:
			return item->ItemType == EQ::item::ItemTypeMount || clicks(SpellEffect::SummonHorse);
		case 1:
			return item->ItemType == EQ::item::ItemTypeIllusion || clicks(SpellEffect::Illusion);
		case 2:
			return item->ItemType == EQ::item::ItemTypeFamiliar || clicks(SpellEffect::Familiar);
		case 3:
			// the client also takes ornament augments flagged as Hero's Forge; the model is the closest server field
			return item->ItemType == EQ::item::ItemTypeHeroForgeOrnament ||
				(item->ItemType == EQ::item::ItemTypeAugmentation && item->HerosForgeModel > 0);
		case 4:
			return item->ItemType == EQ::item::ItemTypeTeleportation;
		case 5:
			return item->Click.Effect > 0 &&
				item->Click.Type != EQ::item::ItemEffectExpendable &&
				item->ItemType != EQ::item::ItemTypePotion;
		default:
			return false;
	}
}

bool Client::MoveKeyRingItem(MoveItem_Struct *move_in)
{
	const int32 from_slot = static_cast<int32>(move_in->from_slot);
	const int32 to_slot   = static_cast<int32>(move_in->to_slot);
	const bool  from_ring = EQ::invslot::IsKeyRingRange(from_slot);
	const bool  to_ring   = EQ::invslot::IsKeyRingRange(to_slot);

	if (IsKeyRingEnabled() && !from_ring && to_ring) {
		return AddToKeyRing(static_cast<int16>(from_slot), static_cast<int16>(to_slot));
	}

	if (IsKeyRingEnabled() && from_ring && !to_ring && EQ::invslot::IsKeyRingSlot(from_slot)) {
		return RemoveFromKeyRing(static_cast<int16>(from_slot), static_cast<int16>(to_slot));
	}

	// the Equipment ring only trades with the cursor
	if (HasEquipmentKeyRing()) {
		if (from_slot == EQ::invslot::slotCursor && EQ::invslot::IsEquipmentKeyRingRange(to_slot)) {
			return AddToEquipmentKeyRing();
		}

		if (EQ::invslot::IsEquipmentKeyRingSlot(from_slot) && to_slot == EQ::invslot::slotCursor) {
			return RemoveFromKeyRing(static_cast<int16>(from_slot), static_cast<int16>(to_slot));
		}
	}

	// rings are off, or a ring to ring move (the client does not send those): put the item back
	if (EQ::invslot::IsKeyRingSlot(from_slot) || EQ::invslot::IsEquipmentKeyRingSlot(from_slot)) {
		if (const auto inst = m_inv[from_slot]) {
			SendItemPacket(static_cast<int16>(from_slot), inst, ItemPacketCharInventory);
		}
	} else if (from_slot == EQ::invslot::slotCursor) {
		if (const auto inst = m_inv.GetCursorItem()) {
			SendItemPacket(EQ::invslot::slotCursor, inst, ItemPacketLimbo);
		}
	} else if (IsValidSlot(from_slot)) {
		if (const auto inst = m_inv[from_slot]) {
			SendItemPacket(static_cast<int16>(from_slot), inst, ItemPacketTrade);
		}
	}

	return false;
}

bool Client::AddToKeyRing(int16 src_slot_id, int16 ring_slot_id)
{
	const int ring = EQ::invslot::KeyRingFromSlot(ring_slot_id);

	// put the item back where the client took it from. The client sends the ring row it put the item on (its
	// next index) and shows it there already, so that row is cleared again
	auto refuse = [this, src_slot_id, ring_slot_id]() {
		if (EQ::invslot::IsKeyRingSlot(ring_slot_id)) {
			if (const auto ring_inst = m_inv[ring_slot_id]) {
				SendItemPacket(ring_slot_id, ring_inst, ItemPacketCharInventory);
			} else {
				DeleteItemInInventory(ring_slot_id, 0, true, false);
			}
		}

		if (src_slot_id == EQ::invslot::slotCursor) {
			if (const auto inst = m_inv.GetCursorItem()) {
				SendItemPacket(EQ::invslot::slotCursor, inst, ItemPacketLimbo);
			}
		} else if (const auto inst = m_inv[src_slot_id]) {
			SendItemPacket(src_slot_id, inst, ItemPacketTrade);
		}

		return false;
	};

	// only from the general inventory, its bags and the cursor
	const bool from_inventory = (
		src_slot_id == EQ::invslot::slotCursor ||
		EQ::ValueWithin(src_slot_id, EQ::invslot::GENERAL_BEGIN, EQ::invslot::GENERAL_END) ||
		EQ::ValueWithin(src_slot_id, EQ::invbag::GENERAL_BAGS_BEGIN, EQ::invbag::GENERAL_BAGS_END)
	);

	const auto inst = from_inventory ? m_inv[src_slot_id] : nullptr;
	if (!inst || !inst->GetItem()) {
		return refuse();
	}

	const auto item = inst->GetItem();

	if (!CanAddToKeyRing(ring, inst)) {
		MessageString(Chat::Red, KEY_RING_WRONG_TYPE, KEY_RING_NAMES[ring]);
		return refuse();
	}

	// the client always lets the mount ring through, so a locked ring is enforced here
	if (!IsKeyRingUnlocked(ring)) {
		Message(Chat::Red, "You have not unlocked the %s key ring.", KEY_RING_NAMES[ring]);
		return refuse();
	}

	std::set<int> used;
	for (const auto &[slot_id, ring_inst] : m_inv.GetKeyRingItems()) {
		if (!ring_inst || EQ::invslot::KeyRingFromSlot(slot_id) != ring) {
			continue;
		}

		used.insert(EQ::invslot::KeyRingIndexFromSlot(slot_id));

		// one of each item per ring
		if (ring_inst->GetID() == item->ID) {
			Message(Chat::Red, "%s is already on your %s key ring.", item->Name, KEY_RING_NAMES[ring]);
			return refuse();
		}
	}

	const int capacity = GetKeyRingCapacity(ring);

	// the client sends -1; a real index is used only when it is free
	int index = EQ::invslot::IsKeyRingSlot(ring_slot_id) ? EQ::invslot::KeyRingIndexFromSlot(ring_slot_id) : -1;
	if (index < 0 || index >= capacity || used.contains(index)) {
		index = -1;
		for (int i = 0; i < capacity; i++) {
			if (!used.contains(i)) {
				index = i;
				break;
			}
		}
	}

	if (index < 0 || static_cast<int>(used.size()) >= capacity) {
		MessageString(Chat::Red, KEY_RING_FULL, item->Name, KEY_RING_NAMES[ring]);
		return refuse();
	}

	const int16 dst_slot_id = EQ::invslot::KeyRingSlot(ring, index);

	// store first, then take it out of the inventory
	if (m_inv.PutItem(dst_slot_id, *inst) == INVALID_INDEX) {
		return refuse();
	}

	const auto stored = m_inv[dst_slot_id];
	if (!stored || !database.SaveInventory(CharacterID(), stored, dst_slot_id)) {
		m_inv.DeleteItem(dst_slot_id);
		return refuse();
	}

	LogInventory(
		"[{}] added [{}] ([{}]) to key ring [{}] index [{}] from slot [{}]",
		GetCleanName(),
		item->Name,
		item->ID,
		ring,
		index,
		src_slot_id
	);

	// inst and item belong to the source slot, do not use them after this. The client already took the item out
	// of the source slot, so a delete echo would fail there ("MOVE ITEM CHARGE FAILED IN CLIENT APPLICATION")
	DeleteItemInInventory(src_slot_id, 0, false);
	if (src_slot_id == EQ::invslot::slotCursor) {
		SendCursorBuffer();
	}

	SendItemPacket(dst_slot_id, m_inv[dst_slot_id], ItemPacketCharInventory);

	return true;
}

bool Client::RemoveFromKeyRing(int16 ring_slot_id, int16 dst_slot_id)
{
	// put the item back on its ring row
	auto refuse = [this, ring_slot_id]() {
		if (const auto inst = m_inv[ring_slot_id]) {
			SendItemPacket(ring_slot_id, inst, ItemPacketCharInventory);
		} else {
			DeleteItemInInventory(ring_slot_id, 0, true);
		}

		return false;
	};

	const auto inst = m_inv[ring_slot_id];
	if (!inst || !inst->GetItem()) {
		return refuse();
	}

	// the client only takes items out to the cursor
	if (dst_slot_id != EQ::invslot::slotCursor) {
		return refuse();
	}

	if (!m_inv.CursorEmpty()) {
		MessageString(Chat::Red, CURSOR_ITEM_BLOCKS);
		if (const auto cursor = m_inv.GetCursorItem()) {
			SendItemPacket(EQ::invslot::slotCursor, cursor, ItemPacketLimbo);
		}

		return refuse();
	}

	if (CheckLoreConflict(inst->GetItem())) {
		MessageString(Chat::Red, DUP_LORE);
		// clear the copy the client put on its cursor
		DeleteItemInInventory(EQ::invslot::slotCursor, 0, true);
		return refuse();
	}

	LogInventory(
		"[{}] took [{}] ([{}]) out of key ring slot [{}]",
		GetCleanName(),
		inst->GetItem()->Name,
		inst->GetID(),
		ring_slot_id
	);

	// the client already moved it: cursor row first, then drop the ring row
	PutItemInInventory(EQ::invslot::slotCursor, *inst, false);
	DeleteItemInInventory(ring_slot_id, 0, false);

	if (EQ::invslot::IsEquipmentKeyRingSlot(ring_slot_id) && HasEquipmentKeyRing()) {
		SendEquipmentKeyRingState();
	}

	return true;
}

int Client::GetEquipmentKeyRingCapacity()
{
	if (ClientVersion() < EQ::versions::ClientVersion::TOB) {
		return 0;
	}

	return std::clamp(RuleI(Character, EquipmentKeyRingSlots), 0, static_cast<int>(EQ::invslot::EQUIPMENT_KEYRING_SIZE));
}

// gear only: something that has equipment slots and is not used up
bool Client::CanAddToEquipmentKeyRing(const EQ::ItemInstance *inst)
{
	if (!inst || !inst->GetItem()) {
		return false;
	}

	const auto item = inst->GetItem();

	if (!item->IsClassCommon() || item->EvolvingItem || item->NoRent == 0) {
		return false;
	}

	return item->Slots != 0 &&
		item->ItemType != EQ::item::ItemTypePotion &&
		item->Click.Type != EQ::item::ItemEffectExpendable;
}

int16 Client::FindFreeEquipmentKeyRingSlot()
{
	const auto &stored = m_inv.GetEquipmentKeyRingItems();
	const int capacity = GetEquipmentKeyRingCapacity();
	for (int index = 0; index < capacity; index++) {
		if (!stored.contains(EQ::invslot::EquipmentKeyRingSlot(index))) {
			return EQ::invslot::EquipmentKeyRingSlot(index);
		}
	}

	return INVALID_INDEX;
}

// gear taken off by a persona switch or equipment set load goes to the ring when the player ticked the box
namespace {
	// the client's Store Equipment Set items option, kept so a zone-in without the tab's hello still stores
	const std::string EQUIPMENT_KEY_RING_STORE_BUCKET = "equipment_keyring_store_sets";
}

// the ring can hold gear for a client that has not announced the tab yet; it sees the items after its hello
bool Client::IsStoringEquipmentSets()
{
	if (m_has_equipment_key_ring) {
		return m_store_equipment_sets;
	}

	return GetEquipmentKeyRingCapacity() > 0 && GetBucket(EQUIPMENT_KEY_RING_STORE_BUCKET) == "1";
}

bool Client::CanStoreInEquipmentKeyRing(const EQ::ItemInstance *inst)
{
	return IsStoringEquipmentSets() && CanAddToEquipmentKeyRing(inst) && FindFreeEquipmentKeyRingSlot() != INVALID_INDEX;
}

// copies inst into the first free ring slot and saves it; the caller owns inst and tells the client
int16 Client::StoreInEquipmentKeyRing(const EQ::ItemInstance &inst)
{
	const int16 slot_id = FindFreeEquipmentKeyRingSlot();
	if (slot_id == INVALID_INDEX || m_inv.PutItem(slot_id, inst) == INVALID_INDEX) {
		return INVALID_INDEX;
	}

	const auto stored = m_inv[slot_id];
	if (!stored || !database.SaveInventory(CharacterID(), stored, slot_id)) {
		m_inv.DeleteItem(slot_id);
		return INVALID_INDEX;
	}

	LogInventory(
		"[{}] stored [{}] ([{}]) in equipment key ring slot [{}]",
		GetCleanName(),
		stored->GetItem()->Name,
		stored->GetID(),
		slot_id
	);

	return slot_id;
}

// the client dropped the cursor item on the tab
bool Client::AddToEquipmentKeyRing()
{
	// put the item back on the cursor
	auto refuse = [this]() {
		if (const auto cursor = m_inv.GetCursorItem()) {
			SendItemPacket(EQ::invslot::slotCursor, cursor, ItemPacketLimbo);
		}

		return false;
	};

	const auto inst = m_inv.GetCursorItem();
	if (!inst || !inst->GetItem()) {
		return refuse();
	}

	if (!CanAddToEquipmentKeyRing(inst)) {
		MessageString(Chat::Red, KEY_RING_WRONG_TYPE, EQUIPMENT_KEY_RING_NAME);
		return refuse();
	}

	if (FindFreeEquipmentKeyRingSlot() == INVALID_INDEX) {
		MessageString(Chat::Red, KEY_RING_FULL, inst->GetItem()->Name, EQUIPMENT_KEY_RING_NAME);
		return refuse();
	}

	const int16 slot_id = StoreInEquipmentKeyRing(*inst);
	if (slot_id == INVALID_INDEX) {
		return refuse();
	}

	// the client already took the item off its cursor, so there is no delete echo
	DeleteItemInInventory(EQ::invslot::slotCursor, 0, false);
	SendCursorBuffer();

	SendItemPacket(slot_id, m_inv[slot_id], ItemPacketCharInventory);
	SendEquipmentKeyRingState();

	return true;
}

// the state also labels each stored item with the class whose equipment set holds it
void Client::SendEquipmentKeyRingState()
{
	EQApplicationPacket outapp(OP_EquipmentKeyRingOption, sizeof(EquipmentKeyRingState_Struct));
	auto state                  = reinterpret_cast<EquipmentKeyRingState_Struct *>(outapp.pBuffer);
	state->capacity             = m_has_equipment_key_ring ? GetEquipmentKeyRingCapacity() : 0;
	state->store_equipment_sets = m_store_equipment_sets ? 1 : 0;

	if (m_has_equipment_key_ring && !m_inv.GetEquipmentKeyRingItems().empty()) {
		const auto owners = Persona::LoadEquipmentSetOwners(database, CharacterID());
		for (const auto &[slot_id, inst] : m_inv.GetEquipmentKeyRingItems()) {
			if (!inst || state->owner_count >= std::size(state->owners)) {
				continue;
			}

			const auto owner = owners.find(static_cast<uint32>(inst->GetSerialNumber()));
			if (owner == owners.end()) {
				continue;
			}

			auto &entry      = state->owners[state->owner_count++];
			entry.index      = static_cast<uint16>(slot_id - EQ::invslot::EQUIPMENT_KEYRING_BEGIN);
			entry.class_mask = owner->second;
		}
	}

	QueuePacket(&outapp);
}

void Client::SendEquipmentKeyRingItems()
{
	if (!m_has_equipment_key_ring) {
		return;
	}

	for (const auto &[slot_id, inst] : m_inv.GetEquipmentKeyRingItems()) {
		SendItemPacket(slot_id, inst, ItemPacketCharInventory);
	}
}

// the Equipment tab's hello, and again each time its box changes
void Client::Handle_OP_EquipmentKeyRingOption(const EQApplicationPacket *app)
{
	if (app->size != sizeof(EquipmentKeyRingOption_Struct)) {
		LogNetcode("Received invalid sized OP_EquipmentKeyRingOption: got [{}], expected [{}]", app->size, sizeof(EquipmentKeyRingOption_Struct));
		return;
	}

	const auto option = reinterpret_cast<const EquipmentKeyRingOption_Struct *>(app->pBuffer);

	// a client with the ring off hears capacity 0 and hides the tab
	const bool enabled = GetEquipmentKeyRingCapacity() > 0;
	const bool first   = enabled && !m_has_equipment_key_ring;

	m_has_equipment_key_ring = enabled;
	m_store_equipment_sets   = enabled && option->store_equipment_sets != 0;

	if (enabled) {
		SetBucket(EQUIPMENT_KEY_RING_STORE_BUCKET, m_store_equipment_sets ? "1" : "0");
	}

	LogInventory(
		"[{}] equipment key ring option [{}], tab [{}]",
		GetCleanName(),
		m_store_equipment_sets,
		m_has_equipment_key_ring
	);

	SendEquipmentKeyRingState();

	// the capacity has to arrive before the items, the labels after them
	if (first) {
		SendEquipmentKeyRingItems();
		SendEquipmentKeyRingState();
	}
}

bool Client::IsKeyRingUnlocked(int ring)
{
	if (ring < 0 || ring >= EQ::invslot::KEYRING_COUNT) {
		return false;
	}

	const std::string value = GetBucket(fmt::format("keyring_unlocked_{}", ring));
	if (value.empty()) {
		return RuleB(Character, KeyRingsUnlockedByDefault);
	}

	return value == "1";
}

void Client::SetKeyRingUnlocked(int ring, bool unlocked)
{
	if (ring < 0 || ring >= EQ::invslot::KEYRING_COUNT) {
		return;
	}

	// TOB clients read the unlocks from the player profile, so a change shows after the next zone
	SetBucket(fmt::format("keyring_unlocked_{}", ring), unlocked ? "1" : "0");
}

uint8 Client::GetKeyRingUnlockMask()
{
	uint8 mask = 0;
	for (int ring = 0; ring < EQ::invslot::KEYRING_COUNT; ++ring) {
		if (IsKeyRingUnlocked(ring)) {
			mask |= static_cast<uint8>(1 << ring);
		}
	}

	return mask;
}

bool Client::AddItemToKeyRing(uint32 item_id)
{
	if (!RuleB(Character, EnableKeyRings)) {
		return false;
	}

	std::unique_ptr<EQ::ItemInstance> inst(database.CreateItem(item_id));
	if (!inst || !inst->GetItem()) {
		return false;
	}

	// the specific rings come before the activated-items ring, which takes any clicky
	for (int ring = 0; ring < EQ::invslot::KEYRING_COUNT; ++ring) {
		if (!CanAddToKeyRing(ring, inst.get()) || !IsKeyRingUnlocked(ring)) {
			continue;
		}

		std::set<int> used;
		for (const auto &[slot_id, ring_inst] : m_inv.GetKeyRingItems()) {
			if (!ring_inst || EQ::invslot::KeyRingFromSlot(slot_id) != ring) {
				continue;
			}

			// one of each item per ring
			if (ring_inst->GetID() == item_id) {
				return false;
			}

			used.insert(EQ::invslot::KeyRingIndexFromSlot(slot_id));
		}

		int index = -1;
		for (int i = 0; i < GetKeyRingCapacity(ring); i++) {
			if (!used.contains(i)) {
				index = i;
				break;
			}
		}

		if (index < 0) {
			continue;
		}

		const int16 slot_id = EQ::invslot::KeyRingSlot(ring, index);
		if (m_inv.PutItem(slot_id, *inst) == INVALID_INDEX) {
			return false;
		}

		const auto stored = m_inv[slot_id];
		if (!stored || !database.SaveInventory(CharacterID(), stored, slot_id)) {
			m_inv.DeleteItem(slot_id);
			return false;
		}

		LogInventory("[{}] quest added [{}] to key ring [{}] index [{}]", GetCleanName(), item_id, ring, index);

		if (IsKeyRingEnabled()) {
			SendItemPacket(slot_id, stored, ItemPacketCharInventory);
		}

		return true;
	}

	return false;
}

bool Client::RemoveItemFromKeyRing(uint32 item_id)
{
	int16 slot_id = INVALID_INDEX;
	for (const auto &[ring_slot_id, ring_inst] : m_inv.GetKeyRingItems()) {
		if (ring_inst && ring_inst->GetID() == item_id) {
			slot_id = ring_slot_id;
			break;
		}
	}

	if (slot_id == INVALID_INDEX) {
		return false;
	}

	LogInventory("[{}] quest removed [{}] from key ring slot [{}]", GetCleanName(), item_id, slot_id);

	DeleteItemInInventory(slot_id, 0, IsKeyRingEnabled());

	return true;
}

std::vector<uint32> Client::GetKeyRingItemIDs(int ring)
{
	std::vector<uint32> item_ids;
	for (const auto &[slot_id, ring_inst] : m_inv.GetKeyRingItems()) {
		if (ring_inst && EQ::invslot::KeyRingFromSlot(slot_id) == ring) {
			item_ids.push_back(ring_inst->GetID());
		}
	}

	return item_ids;
}

bool Client::AddItemToOverflow(uint32 item_id, int16 charges)
{
	std::unique_ptr<EQ::ItemInstance> inst(database.CreateItem(item_id, charges));
	if (!inst || !inst->GetItem()) {
		return false;
	}

	if (CheckLoreConflict(inst->GetItem())) {
		MessageString(Chat::Red, DUP_LORE);
		return false;
	}

	// a cursor that already holds an item queues this one, which TOB shows in the Item Overflow window
	return PushItemOnCursor(*inst, true);
}

int Client::GetItemOverflowCount()
{
	return std::max(0, m_inv.CursorSize() - 1);
}
