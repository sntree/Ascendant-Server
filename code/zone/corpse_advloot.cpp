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
#include "zone/corpse.h"

#include "common/data_verification.h"
#include "common/events/player_event_logs.h"
#include "common/rulesys.h"
#include "common/say_link.h"
#include "common/serialize_buffer.h"
#include "common/strings.h"
#include "zone/entity.h"
#include "zone/groups.h"
#include "zone/queryserv.h"
#include "zone/raids.h"
#include "zone/string_ids.h"
#include "zone/worldserver.h"
#include "zone/zone.h"

#include <algorithm>
#include <memory>
#include <vector>

extern QueryServ   *QServ;
extern WorldServer worldserver;

// Advanced Loot (TOB+ clients). An NPC killed by a client with "Use Advanced Looting" on is looted from the
// Advanced Loot window instead of the classic loot window:
// - solo: every item goes to the killer's Personal list;
// - group / raid: when every member in the zone is a TOB client with Advanced Looting on, the items go to everyone's
//   Shared list. The group (raid) leader is the master looter: they hand items to players (grab/give), or put an
//   item up for need/greed ("ask"); the server runs the ask timer and the roll and reserves the item for the winner.
//   Like Live, nobody is told who chose what: the roll result is the only thing the group sees.
// The items stay on the corpse until someone loots them; the window is a view. A reserved item can only be looted by
// the player it is reserved for, a shared item only through the master looter. When the corpse unlocks for everyone
// (or everything is looted or left) it goes back to classic looting.
//
// Mixed groups/raids and other clients keep classic looting. A recipient keeps their reservations if they untick
// the box later; the box only decides whether a new kill uses Advanced Loot.
// Packet layouts and Live's behaviour: analysis/systems/advloot/live-findings.md in the ToB project notes.

namespace {

enum AdvLootSubtype : uint16 {
	AdvLootSync         = 3,
	AdvLootCoin         = 6,
	AdvLootLeave        = 7,
	AdvLootLoot         = 8,
	AdvLootLootResult   = 9,
	AdvLootAsk          = 0xa,
	AdvLootGive         = 0xe,
	AdvLootClearPending = 0x10,
	AdvLootState        = 0x11,
	AdvLootRollResult   = 0x13,
	AdvLootCapabilities = 0x20, // not a client message: tells the client-side buttons what the server supports
};

// the loot request's trailing byte; the stock client only sends Loot and Loot All
enum AdvLootAction : uint8 {
	AdvLootActionLoot            = 0,
	AdvLootActionLootAll         = 1,
	AdvLootActionSell            = 3,
	AdvLootActionStore           = 5, // Tradeskill Depot for components, else Dragon's Hoard
	AdvLootActionStoreDepot      = 6,
	AdvLootActionStoreDepotStack = 7, // Tradeskill Depot, only onto a stack that is already there
};

enum AdvLootCapability : uint32 {
	AdvLootCanSell       = 1 << 0,
	AdvLootCanStoreDepot = 1 << 1,
	AdvLootCanStoreHoard = 1 << 2,
};

enum AdvLootResult : uint16 {
	AdvLootResultLooted = 1,
	AdvLootResultLore   = 3, // client prints 371
	AdvLootResultNoRoom = 5, // client prints 372
	AdvLootResultRefused = 6, // no client handling: frees the request and the row, prints nothing
	AdvLootResultGone   = 7, // client prints 11318
};

// transaction states (client: Waiting, Ask, Ask auto Roll, Stop, Ask Completed, Fixed Ask auto Roll,
// Fixed Ask Completed, Transaction Removed)
enum AdvLootState : uint32 {
	AdvLootStateWaiting     = 0,
	AdvLootStateAsk         = 1,
	AdvLootStateAskAutoRoll = 2,
	AdvLootStateStop        = 3,
	AdvLootStateCompleted   = 4,
	AdvLootStateFreeGrab    = 5, // client: "Fixed Ask auto Roll"; the hand icon, anyone may take it
	AdvLootStateRemoved     = 7,
};

enum AdvLootChoice : uint32 {
	AdvLootChoiceNone  = 0,
	AdvLootChoiceNo    = 1,
	AdvLootChoiceGreed = 2,
	AdvLootChoiceNeed  = 3,
};

uint32 AdvancedLootAskMilliseconds()
{
	return static_cast<uint32>(std::max(1, RuleI(Character, AdvancedLootAskSeconds))) * 1000;
}

constexpr uint32 ADVLOOT_ROLL_MAX = 1000;  // Live roll seen: 530

// the same cap as a merchant sale, and the client's own id for Krono
constexpr double ADVLOOT_SELL_PRICE_CAP = 4000000000.0;
constexpr uint32 ADVLOOT_KRONO_ITEM_ID  = 88888;

void SendAdvLootResult(Client *c, uint16 result, uint32 sequence, uint64 copper = 0)
{
	// u16 subtype, u16 result, u32 (Live: always 1), u32 the client's request sequence, u8; a sale adds the
	// proceeds as a u64 that only the client-side buttons read
	SerializeBuffer buf;
	buf.WriteUInt16(AdvLootLootResult);
	buf.WriteUInt16(result);
	buf.WriteUInt32(1);
	buf.WriteUInt32(sequence);
	buf.WriteUInt8(0);
	if (copper) {
		buf.WriteUInt64(copper);
	}

	auto outapp = new EQApplicationPacket(OP_AdvLoot, std::move(buf));
	c->QueuePacket(outapp);
	safe_delete(outapp);
}

void SendAdvLootClearPending(Client *c, uint32 corpse_id, uint32 item_id)
{
	// clears the row's "busy" state when a request is refused without a result code
	SerializeBuffer buf;
	buf.WriteUInt16(AdvLootClearPending);
	buf.WriteUInt32(corpse_id);
	buf.WriteUInt64(item_id);
	buf.WriteUInt32(0);

	auto outapp = new EQApplicationPacket(OP_AdvLoot, std::move(buf));
	c->QueuePacket(outapp);
	safe_delete(outapp);
}

// a refused loot request: sub 0x10 alone leaves the client's request pending (it is keyed by sequence and only a
// result frees it), so answer with a result the client takes as "nothing happened" as well
void RefuseAdvLootRequest(Client *c, uint32 corpse_id, uint32 item_id, uint32 sequence)
{
	SendAdvLootResult(c, AdvLootResultRefused, sequence);
	SendAdvLootClearPending(c, corpse_id, item_id);
}

void SendAdvLootCapabilities(Client *c)
{
	if (!RuleB(Character, AdvancedLootActions)) {
		return;
	}

	uint32 flags = AdvLootCanSell;
	if (c->IsTradeskillDepotEnabled()) {
		flags |= AdvLootCanStoreDepot;
	}
	if (c->IsDragonHoardEnabled()) {
		flags |= AdvLootCanStoreHoard;
	}

	LogLoot("[{}] Advanced Loot actions announced, flags [{}]", c->GetCleanName(), flags);

	SerializeBuffer buf;
	buf.WriteUInt16(AdvLootCapabilities);
	buf.WriteUInt32(flags);

	auto outapp = new EQApplicationPacket(OP_AdvLoot, std::move(buf));
	c->QueuePacket(outapp);
	safe_delete(outapp);
}

void SendAdvLootMasterLooter(Client *c, const std::string &name)
{
	SerializeBuffer buf;
	buf.WriteUInt32(0);
	buf.WriteString(name);

	auto outapp = new EQApplicationPacket(OP_AdvLootMasterLooter, std::move(buf));
	c->QueuePacket(outapp);
	safe_delete(outapp);
}

// u16 subtype, u8 split, u64 copper, u8 received, u32 bonus. The client plays the coin sound, prints its own "You
// receive ..." line and adds the coin to what it shows when the coin is a split or it is the master looter.
void SendAdvLootCoin(Client *c, uint64 copper, bool split)
{
	SerializeBuffer buf;
	buf.WriteUInt16(AdvLootCoin);
	buf.WriteUInt8(split ? 1 : 0);
	buf.WriteUInt64(copper);
	buf.WriteUInt8(1);
	buf.WriteUInt32(0); // no bonus suffix

	auto outapp = new EQApplicationPacket(OP_AdvLoot, std::move(buf));
	c->QueuePacket(outapp);
	safe_delete(outapp);
}

// the money update has to come after the coin message: it sets the real total over the client's own addition
void GiveAdvLootCoin(Client *c, uint64 copper, bool split)
{
	SendAdvLootCoin(c, copper, split);
	c->AddMoneyToPP(copper, true);
}

bool HasBagContents(LootItem *const *bag_item_data)
{
	return std::any_of(bag_item_data, bag_item_data + EQ::invbag::SLOT_COUNT, [](const LootItem *content) {
		return content != nullptr;
	});
}

bool IsBagContent(const LootItem *item)
{
	return EQ::ValueWithin(item->equip_slot, EQ::invbag::GENERAL_BAGS_BEGIN, EQ::invbag::CURSOR_BAG_END);
}

bool UsesAdvancedLoot(Client *c)
{
	return c && c->ClientVersion() >= EQ::versions::ClientVersion::TOB && c->IsAdvancedLooting();
}

} // namespace

void Corpse::StartAdvancedLoot(Client *killer)
{
	// a corpse with only coin still qualifies: the coin goes out automatically, as on Live
	if (!RuleB(Character, EnableAdvancedLoot) || !killer || IsPlayerCorpse() || IsEmpty()) {
		return;
	}

	Raid  *raid  = killer->IsRaidGrouped() ? killer->GetRaid() : nullptr;
	Group *group = !raid && killer->IsGrouped() ? killer->GetGroup() : nullptr;

	// A player grouped only with their merc loots alone: the ToB client knows the merc is not a looter and
	// shows only Personal Loot for that group, so shared loot would never appear in its window
	if (group) {
		int players = 0;
		for (int i = 0; i < MAX_GROUP_MEMBERS; ++i) {
			const Mob *m = group->members[i];
			if (group->membername[i][0] && !(m && (m->IsMerc() || m->IsBot()))) {
				++players;
			}
		}

		if (players <= 1) {
			group = nullptr;
		}
	}
	if (raid) {
		// shared loot only when every raid member in the zone can use it and the leader (master looter) is here
		Client *master_looter = raid->GetAdvancedLootMasterLooter();
		if (!master_looter || !UsesAdvancedLoot(master_looter)) {
			LogLoot("[{}] raid corpse uses classic looting: master looter [{}] is not using Advanced Looting",
				GetCleanName(), master_looter ? master_looter->GetCleanName() : "none in zone");
			return;
		}

		std::vector<uint32> recipients;
		for (const auto &m : raid->members) {
			if (!m.member || m.is_bot) {
				continue;
			}

			if (!UsesAdvancedLoot(m.member)) {
				LogLoot("[{}] raid corpse uses classic looting: [{}] is not using Advanced Looting",
					GetCleanName(), m.member->GetCleanName());
				return;
			}

			recipients.push_back(m.member->CharacterID());
		}

		m_advloot_recipients = recipients;
		m_advloot_group_id   = 0;
		m_advloot_raid_id    = raid->GetID();
	}
	else if (group) {
		// shared loot only when every group member in the zone can use it and the leader (master looter) is here
		Client *master_looter = group->GetAdvancedLootMasterLooter();
		if (!master_looter || !UsesAdvancedLoot(master_looter)) {
			LogLoot("[{}] group corpse uses classic looting: master looter [{}] is not using Advanced Looting",
				GetCleanName(), master_looter ? master_looter->GetCleanName() : "none in zone");
			return;
		}

		std::vector<uint32> recipients;
		for (Mob *m : group->members) {
			if (!m || !m->IsClient()) {
				continue;
			}

			if (!UsesAdvancedLoot(m->CastToClient())) {
				LogLoot("[{}] group corpse uses classic looting: [{}] is not using Advanced Looting",
					GetCleanName(), m->GetCleanName());
				return;
			}

			recipients.push_back(m->CastToClient()->CharacterID());
		}

		m_advloot_recipients = recipients;
		m_advloot_group_id   = group->GetID();
		m_advloot_raid_id    = 0;
	}
	else {
		if (!UsesAdvancedLoot(killer)) {
			return;
		}

		m_advloot_recipients = {killer->CharacterID()};
		m_advloot_group_id   = 0;
		m_advloot_raid_id    = 0;
	}

	m_advloot_owner.clear();
	m_advloot_left.clear();
	m_advloot_choices.clear();
	m_advloot_transactions.clear();
	m_advloot_names.clear();
	m_advloot_present.clear();

	// Live: a member who was out of range when the loot was collected sees it locked until they go to the corpse
	m_advloot_present.insert(killer->CharacterID());
	for (uint32 character_id : m_advloot_recipients) {
		Client *r = entity_list.GetClientByCharID(character_id);
		if (r) {
			m_advloot_names[character_id] = r->GetCleanName();
			const int range = RuleI(Character, AdvancedLootPresenceRange);
			if (range <= 0 || DistanceNoZ(r->GetPosition(), GetPosition()) <= range) {
				m_advloot_present.insert(character_id);
			}
		}
	}

	// solo: everything is the killer's
	if (!IsAdvancedLootShared()) {
		for (const auto *item : m_item_list) {
			m_advloot_owner[item] = killer->CharacterID();
		}
	}

	// coin goes straight to the killer, or is split over the group, as on Live
	if (GetCopper() || GetSilver() || GetGold() || GetPlatinum()) {
		const uint64 total = static_cast<uint64>(GetCopper()) + GetSilver() * 10ULL + GetGold() * 100ULL + GetPlatinum() * 1000ULL;

		Client *master_looter = GetAdvancedLootMasterLooter();
		if (master_looter && !master_looter->AutoSplitEnabled()) {
			GiveAdvLootCoin(master_looter, total, false);
		}
		else if (raid || group) {
			// even split of the total value (Group::SplitMoney splits each coin type and gives the
			// remainders to the killer, which reads as uneven)
			std::vector<Client *> sharers;
			if (raid) {
				for (const auto &m : raid->members) {
					if (m.member && !m.is_bot) {
						sharers.push_back(m.member);
					}
				}
			}
			else {
				for (Mob *m : group->members) {
					if (m && m->IsClient()) {
						sharers.push_back(m->CastToClient());
					}
				}
			}

			if (sharers.empty()) {
				sharers.push_back(killer);
			}

			const uint64 share         = total / sharers.size();
			const uint64 extra         = total - share * sharers.size();
			const bool   killer_shares = std::find(sharers.begin(), sharers.end(), killer) != sharers.end();
			Client      *remainder_to  = killer_shares ? killer : sharers.front(); // the remainder goes to the killer
			for (Client *s : sharers) {
				const uint64 amount = share + (s == remainder_to ? extra : 0);
				if (amount) {
					GiveAdvLootCoin(s, amount, true);
				}
			}
		}
		else {
			// the client only takes unsplit coin as its own when it sees itself as master looter, and nothing tells
			// it so after it leaves a group, so say it again for a solo kill
			SendAdvLootMasterLooter(killer, killer->GetCleanName());
			GiveAdvLootCoin(killer, total, false);
		}

		RemoveCash();
	}

	if (!HasAdvancedLootItems()) {
		m_advloot_recipients.clear();
		m_advloot_group_id = 0;
		m_advloot_raid_id  = 0;
		return;
	}

	// give the group time to decide (Live keeps Advanced Loot corpses locked much longer than classic corpses),
	// and keep the corpse around past the unlock so nothing decays while it is still being looted
	const uint32 unlock_ms = static_cast<uint32>(std::max(0, RuleI(Character, AdvancedLootUnlockSeconds))) * 1000;
	if (unlock_ms && !m_private_corpse) {
		m_corpse_delay_timer.Start(unlock_ms);
		const uint32 keep_ms = unlock_ms + 120000;
		if (m_corpse_decay_timer.Enabled() && m_corpse_decay_timer.GetRemainingTime() < keep_ms) {
			m_corpse_decay_timer.Start(keep_ms);
		}
	}

	Client *master_looter = GetAdvancedLootMasterLooter();
	for (Client *c : GetAdvancedLootRecipients()) {
		if (master_looter) {
			SendAdvLootMasterLooter(c, master_looter->GetCleanName());
		}

		SendAdvancedLootItems(c);
	}
}

bool Corpse::IsAdvancedLootFor(Client *c)
{
	return (
		c &&
		c->ClientVersion() >= EQ::versions::ClientVersion::TOB &&
		std::find(m_advloot_recipients.begin(), m_advloot_recipients.end(), c->CharacterID()) != m_advloot_recipients.end()
	);
}

bool Corpse::IsAdvancedLootItemListed(const LootItem *item)
{
	return !IsBagContent(item) && !m_advloot_left.contains(item);
}

bool Corpse::HasAdvancedLootItems()
{
	if (m_advloot_recipients.empty()) {
		return false;
	}

	for (const auto *item : m_item_list) {
		if (IsAdvancedLootItemListed(item)) {
			return true;
		}
	}

	return false;
}

bool Corpse::HasAdvancedLootItemsFor(Client *c)
{
	if (!IsAdvancedLootFor(c)) {
		return false;
	}

	for (const auto *item : m_item_list) {
		if (!IsAdvancedLootItemListed(item)) {
			continue;
		}

		const uint32 owner = GetAdvancedLootOwner(item);
		if (!owner || owner == c->CharacterID()) {
			return true;
		}
	}

	return false;
}

bool Corpse::HasAdvancedLootTransaction(uint32 transaction_id)
{
	return m_advloot_transactions.contains(transaction_id);
}

bool Corpse::IsAdvancedLootItemInAsk(const LootItem *item)
{
	for (const auto &[id, t] : m_advloot_transactions) {
		if (t.item == item && t.state != AdvLootStateCompleted) {
			return true;
		}
	}

	return false;
}

// character an item is reserved for; 0 = Shared list. On a solo corpse everything belongs to the killer,
// including items a quest adds after the kill.
uint32 Corpse::GetAdvancedLootOwner(const LootItem *item)
{
	auto owner = m_advloot_owner.find(item);
	if (owner != m_advloot_owner.end()) {
		return owner->second;
	}

	return (!IsAdvancedLootShared() && !m_advloot_recipients.empty()) ? m_advloot_recipients.front() : 0;
}

// owner_character_id 0 = an unreserved (Shared list) item; skip_asked skips copies already up for need/greed
LootItem *Corpse::FindAdvancedLootItem(uint32 item_id, uint32 owner_character_id, bool skip_asked)
{
	for (auto *item : m_item_list) {
		if (item->item_id != item_id || !IsAdvancedLootItemListed(item)) {
			continue;
		}

		if (skip_asked && IsAdvancedLootItemInAsk(item)) {
			continue;
		}

		if (GetAdvancedLootOwner(item) == owner_character_id) {
			return item;
		}
	}

	return nullptr;
}

// the bag's contents, by pointer (Corpse::GetItem looks bags up by loot slot)
void Corpse::GetAdvancedLootBagContents(LootItem *item, LootItem **bag_item_data)
{
	if (!EQ::InventoryProfile::SupportsContainers(item->equip_slot)) {
		return;
	}

	const int16 bag_start = EQ::InventoryProfile::CalcSlotId(item->equip_slot, EQ::invbag::SLOT_BEGIN);
	for (auto *content : m_item_list) {
		if (content->equip_slot >= bag_start && content->equip_slot < bag_start + EQ::invbag::SLOT_COUNT) {
			bag_item_data[content->equip_slot - bag_start] = content;
		}
	}
}

Client *Corpse::GetAdvancedLootMasterLooter()
{
	if (m_advloot_raid_id) {
		Raid   *raid          = entity_list.GetRaidByID(m_advloot_raid_id);
		Client *master_looter = raid ? raid->GetAdvancedLootMasterLooter() : nullptr;
		return master_looter && IsAdvancedLootFor(master_looter) ? master_looter : nullptr;
	}

	if (!m_advloot_group_id) {
		return nullptr;
	}

	Group  *group         = entity_list.GetGroupByID(m_advloot_group_id);
	Client *master_looter = group ? group->GetAdvancedLootMasterLooter() : nullptr;
	return master_looter && IsAdvancedLootFor(master_looter) ? master_looter : nullptr;
}

std::vector<Client *> Corpse::GetAdvancedLootRecipients()
{
	std::vector<Client *> clients;
	for (uint32 character_id : m_advloot_recipients) {
		Client *c = entity_list.GetClientByCharID(character_id);
		if (c && c->ClientVersion() >= EQ::versions::ClientVersion::TOB) {
			clients.push_back(c);
		}
	}

	return clients;
}

void Corpse::QueueAdvancedLootRecipients(const EQApplicationPacket *app)
{
	for (Client *c : GetAdvancedLootRecipients()) {
		c->QueuePacket(app);
	}
}

void Corpse::SendAdvancedLootItems(Client *c)
{
	struct Entry {
		const LootItem     *loot;
		const EQ::ItemData *data;
		bool               mine;
	};

	// Personal list: items reserved for this player. Shared list: unreserved items (group loot only).
	// Items reserved for someone else are not shown. A corpse without Advanced Loot (#advloottest) shows as personal.
	const bool test_view = m_advloot_recipients.empty();

	std::vector<Entry> entries;
	for (const auto *loot : m_item_list) {
		if (!IsAdvancedLootItemListed(loot)) {
			continue;
		}

		const uint32 reserved = GetAdvancedLootOwner(loot);
		if (reserved && reserved != c->CharacterID()) {
			continue;
		}

		const auto *data = database.GetItem(loot->item_id);
		if (data && entries.size() < 200) { // the client holds at most 200 items per corpse
			entries.push_back({loot, data, test_view || reserved != 0});
		}
	}

	if (entries.empty()) {
		return;
	}

	const uint32 unlock_seconds = m_corpse_delay_timer.Enabled() ? m_corpse_delay_timer.GetRemainingTime() / 1000 : 0;

	// TOB wire order; item links stay in server format and are converted by the TOB encoder
	SerializeBuffer buf;
	buf.WriteUInt8(IsAdvancedLootPresent(c) || test_view ? 1 : 0); // Live: 1 when present (guess: 0 = locked overlay)
	buf.WriteUInt8(IsAdvancedLootShared() ? 1 : 0); // Live: 1 for group corpses, 0 solo
	buf.WriteUInt32(unlock_seconds);
	buf.WriteUInt32(GetID());
	buf.WriteUInt32(entries.size());
	buf.WriteString(GetCleanName());

	for (const auto &e : entries) {
		std::unique_ptr<EQ::ItemInstance> inst(
			database.CreateItem(
				e.data,
				e.loot->charges,
				e.loot->aug_1,
				e.loot->aug_2,
				e.loot->aug_3,
				e.loot->aug_4,
				e.loot->aug_5,
				e.loot->aug_6,
				e.loot->attuned,
				e.loot->custom_data,
				e.loot->ornamenticon,
				e.loot->ornamentidfile,
				e.loot->ornament_hero_model
			)
		);

		std::string link;
		if (inst) {
			EQ::SayLinkEngine linker;
			linker.SetLinkType(EQ::saylink::SayLinkItemInst);
			linker.SetItemInst(inst.get());
			link = linker.GenerateLink();
		}

		const bool   stackable = e.data->Stackable && e.data->StackSize > 1;
		const uint32 quantity  = stackable ? std::max<uint32>(1, e.loot->charges) : 1;

		buf.WriteUInt64(e.loot->item_id);
		buf.WriteUInt32(e.data->Icon);
		buf.WriteUInt32(e.mine ? 0 : quantity);             // Shared list quantity
		buf.WriteUInt32(stackable ? e.data->StackSize : 1); // max stack size; 1 = one row per item
		buf.WriteUInt8(0);
		buf.WriteUInt32(0);
		buf.WriteUInt32(e.mine ? quantity : 0);             // Personal list quantity
		buf.WriteString(e.data->Name);
		buf.WriteUInt32(0);                                 // the next five are Live's common values
		buf.WriteUInt32(0xFFFF);
		buf.WriteUInt32(0xFFFF);
		buf.WriteUInt32(0);
		buf.WriteUInt32(0);
		buf.WriteLengthString(link);
	}

	auto outapp = new EQApplicationPacket(OP_AdvLootCorpseItems, std::move(buf));
	c->QueuePacket(outapp);
	safe_delete(outapp);

	for (auto &[id, t] : m_advloot_transactions) {
		SendAdvancedLootTransaction(c, id, t);
		SendAdvancedLootStateTo(c, id, t);
	}
}

void Corpse::AdvancedLootItem(Client *c, uint32 item_id, uint32 sequence, uint8 action)
{
	if (!IsAdvancedLootFor(c) || m_player_corpse_depop) {
		SendAdvLootResult(c, AdvLootResultGone, sequence);
		return;
	}

	if (m_is_locked && c->Admin() < AccountStatus::GMAdmin) {
		c->Message(Chat::Red, "Error: Corpse locked by GM.");
		RefuseAdvLootRequest(c, GetID(), item_id, sequence);
		return;
	}

	if (!IsAdvancedLootPresent(c)) {
		c->Message(Chat::Red, "You were not near this corpse when it was looted. Go to the corpse and right-click it first.");
		RefuseAdvLootRequest(c, GetID(), item_id, sequence);
		return;
	}

	LootItem *item_data = FindAdvancedLootItem(item_id, c->CharacterID());
	const auto *item    = item_data ? database.GetItem(item_data->item_id) : nullptr;
	if (!item) {
		SendAdvLootResult(c, AdvLootResultGone, sequence);
		return;
	}

	LootItem *bag_item_data[EQ::invbag::SLOT_COUNT] = {};
	GetAdvancedLootBagContents(item_data, bag_item_data);

	std::unique_ptr<EQ::ItemInstance> inst(
		database.CreateItem(
			item,
			item_data->charges,
			item_data->aug_1,
			item_data->aug_2,
			item_data->aug_3,
			item_data->aug_4,
			item_data->aug_5,
			item_data->aug_6,
			item_data->attuned,
			item_data->custom_data,
			item_data->ornamenticon,
			item_data->ornamentidfile,
			item_data->ornament_hero_model
		)
	);
	if (!inst) {
		SendAdvLootResult(c, AdvLootResultGone, sequence);
		return;
	}

	if (!RuleB(Character, AdvancedLootActions)) {
		action = AdvLootActionLoot;
	}

	switch (action) {
		case AdvLootActionLoot:
		case AdvLootActionLootAll:
			break;
		case AdvLootActionSell:
		case AdvLootActionStore:
		case AdvLootActionStoreDepot:
		case AdvLootActionStoreDepotStack:
			// quest loot events run inside, once the item is known to be sellable or storable
			if (action == AdvLootActionSell) {
				AdvancedLootSell(c, inst.get(), item_data, bag_item_data, sequence);
			}
			else {
				AdvancedLootStore(c, inst.get(), item_data, bag_item_data, sequence, action);
			}
			return;
		default:
			LogLoot("[{}] unknown Advanced Loot action [{}] for item [{}]", c->GetCleanName(), action, item_id);
			RefuseAdvLootRequest(c, GetID(), item_id, sequence);
			return;
	}

	if (HasLootLoreConflict(c, inst.get())) {
		SendAdvLootResult(c, AdvLootResultLore, sequence);
		return;
	}

	// Live refuses when there is no room; stackables may still fit onto an existing stack
	const bool is_arrow = item->ItemType == EQ::item::ItemTypeArrow;
	if (
		!inst->IsStackable() &&
		c->GetInv().FindFreeSlot(inst->IsClassBag(), false, item->Size, is_arrow) == INVALID_INDEX
	) {
		SendAdvLootResult(c, AdvLootResultNoRoom, sequence);
		return;
	}

	if (LootPreventedByEvents(c, inst.get())) {
		RefuseAdvLootRequest(c, GetID(), item_id, sequence);
		return;
	}

	c->CommonBreakInvisible();

	GiveLootToClient(c, inst.get(), item, item_data, bag_item_data, true, true); // deletes item_data (and forgets it)
	c->SendItemLink(inst.get(), true);

	SendAdvLootResult(c, AdvLootResultLooted, sequence);
	CheckAdvancedLootDone();
}

// the item leaves the corpse without going to the player's inventory (sold or stored); frees item_data
void Corpse::TakeAdvancedLootItem(LootItem *item_data)
{
	database.DeleteItemOffCharacterCorpse(m_corpse_db_id, item_data->equip_slot, item_data->item_id);
	RemoveItem(item_data);
	CheckAdvancedLootDone();
}

void Corpse::AdvancedLootSell(Client *c, EQ::ItemInstance *inst, LootItem *item_data, LootItem **bag_item_data, uint32 sequence)
{
	const auto *item = inst->GetItem();

	const uint32 quantity  = inst->IsStackable() ? std::max<int16>(inst->GetCharges(), 1) : 1;
	const double raw_price = static_cast<double>(item->Price) * quantity * RuleR(Merchant, BuyCostMod) + 0.5;
	const uint64 price     = static_cast<uint64>(std::clamp(raw_price, 0.0, ADVLOOT_SELL_PRICE_CAP));

	const bool unsellable =
		!item->NoDrop ||
		!item->Price ||
		!price ||
		item_data->attuned ||
		inst->IsAugmented() ||
		HasBagContents(bag_item_data) ||
		zone->GetCurrencyID(item->ID) != 0 ||
		item->ID == ADVLOOT_KRONO_ITEM_ID;
	if (unsellable) {
		c->Message(Chat::Red, fmt::format("{} cannot be sold from the loot window.", item->Name).c_str()); // no client string for this
		LogLoot("[{}] sell of [{}] from [{}] refused", c->GetCleanName(), item->Name, GetCleanName());
		RefuseAdvLootRequest(c, GetID(), item->ID, sequence);
		return;
	}

	// the item still leaves the corpse, so it counts as a loot for quests (and they may veto it)
	if (LootPreventedByEvents(c, inst)) {
		RefuseAdvLootRequest(c, GetID(), item->ID, sequence);
		return;
	}

	c->AddMoneyToPP(price, true);

	// name the item: auto-sell and Sell All give the player no other way to tell what went
	const std::string money = Strings::Money(price / 1000, (price / 100) % 10, (price / 10) % 10, price % 10);
	const std::string sold  = quantity > 1 ? fmt::format("{} {}", quantity, item->Name) : std::string(item->Name);
	c->Message(Chat::MoneySplit, fmt::format("You sold {} for {}.", sold, money).c_str());

	if (PlayerEventLogs::Instance()->IsEventEnabled(PlayerEvent::MERCHANT_SELL)) {
		auto e = PlayerEvent::MerchantSellEvent{
			.npc_id                  = 0,
			.merchant_name           = "Advanced Loot",
			.merchant_type           = 0,
			.item_id                 = item->ID,
			.item_name               = item->Name,
			.charges                 = static_cast<int16>(quantity),
			.cost                    = static_cast<uint32>(price),
			.alternate_currency_id   = 0,
			.player_money_balance    = c->GetCarriedMoney(),
			.player_currency_balance = 0,
		};

		RecordPlayerEventLogWithClient(c, PlayerEvent::MERCHANT_SELL, e);
	}

	LogLoot("[{}] sold [{}] x [{}] from [{}] for [{}] copper", c->GetCleanName(), item->Name, quantity, GetCleanName(), price);

	TakeAdvancedLootItem(item_data);
	SendAdvLootResult(c, AdvLootResultLooted, sequence, price);
}

void Corpse::AdvancedLootStore(Client *c, EQ::ItemInstance *inst, LootItem *item_data, LootItem **bag_item_data, uint32 sequence, uint8 action)
{
	const auto *item = inst->GetItem();

	// room is checked up front: once the quest loot events below have run, the store must not fail for a reason
	// we could have seen (a full hoard or depot, a lore copy already in the hoard)
	const bool depot_ok = c->TradeskillDepotHasRoomFor(inst) &&
		(action != AdvLootActionStoreDepotStack || c->GetTradeskillDepotQuantity(item->ID) > 0);
	const bool hoard_ok = action == AdvLootActionStore && c->DragonHoardHasRoomFor(inst);

	if (HasBagContents(bag_item_data) || (!depot_ok && !hoard_ok)) {
		c->Message(Chat::Red, fmt::format("{} cannot be stored from the loot window, or there is no room for it.", item->Name).c_str()); // no client string for this
		LogLoot("[{}] store of [{}] from [{}] refused", c->GetCleanName(), item->Name, GetCleanName());
		RefuseAdvLootRequest(c, GetID(), item->ID, sequence);
		return;
	}

	// the item still leaves the corpse, so it counts as a loot for quests (and they may veto it)
	if (LootPreventedByEvents(c, inst)) {
		RefuseAdvLootRequest(c, GetID(), item->ID, sequence);
		return;
	}

	// a full depot falls back to the hoard; the helpers tell the player why a store failed
	const bool in_depot = depot_ok && c->StoreInTradeskillDepot(inst, !hoard_ok);
	const bool in_hoard = !in_depot && hoard_ok && c->StoreInDragonHoard(inst, false);
	if (!in_depot && !in_hoard) {
		RefuseAdvLootRequest(c, GetID(), item->ID, sequence);
		return;
	}

	// the depot prints its own deposit line; the hoard prints nothing when nothing is on the cursor
	if (in_hoard) {
		const int16 count = inst->IsStackable() ? std::max<int16>(inst->GetCharges(), 1) : 1;
		const std::string added = count > 1 ? fmt::format("{} {}", count, item->Name) : std::string(item->Name);
		c->Message(Chat::White, fmt::format("You have added {} to your Dragon's Hoard.", added).c_str());
	}

	LogLoot("[{}] stored [{}] from [{}] (action [{}])", c->GetCleanName(), item->Name, GetCleanName(), action);

	TakeAdvancedLootItem(item_data);
	SendAdvLootResult(c, AdvLootResultLooted, sequence);
}

namespace {
	constexpr uint32 ADVLOOT_YOU_LEFT_ON_CORPSE    = 11309; // --You left %1 %2 on %3.--
	constexpr uint32 ADVLOOT_PLAYER_LEFT_ON_CORPSE = 11313; // --%1 left %2 %3 on %4.--

	std::string AdvLootItemLink(const LootItem *item_data)
	{
		const EQ::ItemData *item = database.GetItem(item_data->item_id);
		if (!item) {
			return "an item";
		}

		std::unique_ptr<EQ::ItemInstance> inst(
			database.CreateItem(
				item,
				item_data->charges,
				item_data->aug_1, item_data->aug_2, item_data->aug_3,
				item_data->aug_4, item_data->aug_5, item_data->aug_6,
				item_data->attuned,
				item_data->custom_data,
				item_data->ornamenticon,
				item_data->ornamentidfile,
				item_data->ornament_hero_model
			)
		);
		if (!inst) {
			return item->Name;
		}

		EQ::SayLinkEngine linker;
		linker.SetLinkType(EQ::saylink::SayLinkItemInst);
		linker.SetItemInst(inst.get());
		linker.GenerateLink();
		return linker.Link();
	}

	std::string AdvLootQuantityWord(uint16 quantity)
	{
		return quantity <= 1 ? "a" : std::to_string(quantity);
	}
}

void Corpse::AdvancedLeaveItem(Client *c, uint32 item_id)
{
	if (!IsAdvancedLootFor(c)) {
		return;
	}

	// the packet does not say which list the row was in: the player's own (Personal) copy first, then, for the
	// master looter, a Shared one (handled like Leave On Corpse so every Shared list drops it)
	LootItem *item_data = FindAdvancedLootItem(item_id, c->CharacterID());
	if (!item_data) {
		if (GetAdvancedLootMasterLooter() == c && FindAdvancedLootItem(item_id, 0)) {
			AdvancedGiveItem(c, 0, item_id, 1, true, c->GetCleanName());
		}
		return;
	}

	// a left item stays on the corpse; classic looting opens once nothing is left in the window or the corpse unlocks
	const std::string link = AdvLootItemLink(item_data);
	m_advloot_left.insert(item_data);
	m_advloot_owner.erase(item_data);
	c->MessageString(Chat::Loot, ADVLOOT_YOU_LEFT_ON_CORPSE, AdvLootQuantityWord(1).c_str(), link.c_str(), GetCleanName());
	CheckAdvancedLootDone();
}

void Corpse::AdvancedGiveItem(Client *c, uint32 transaction_id, uint32 item_id, uint16 quantity, bool leave, const std::string &to_name)
{
	Client *master_looter = GetAdvancedLootMasterLooter();
	if (!master_looter || master_looter != c) {
		return;
	}

	// an asked row names its ask: with duplicates of one item, that says which copy (the item id alone would pick the first)
	LootItem *item_data = nullptr;
	auto      ask       = m_advloot_transactions.find(transaction_id);
	if (transaction_id && ask != m_advloot_transactions.end() && ask->second.item_id == item_id) {
		item_data = const_cast<LootItem *>(ask->second.item);
	}
	else {
		item_data = FindAdvancedLootItem(item_id, 0, true);
	}

	if (!item_data || (m_is_locked && c->Admin() < AccountStatus::GMAdmin)) {
		SendAdvLootClearPending(c, GetID(), item_id);
		return;
	}

	// flag 1 = Leave On Corpse (Leave All sends it with the master looter's own name for every Shared row;
	// Live answers with eqstr 11313 "--%1 left %2 %3 on %4.--"); flag 0 = give, to anyone including yourself
	if (leave) {
		const std::string link = AdvLootItemLink(item_data);
		const std::string qty  = AdvLootQuantityWord(std::max<uint16>(1, quantity));

		uint32 ask_id = DropAdvancedLootAsks(item_data);
		if (!ask_id) {
			ask_id = transaction_id; // the client's row still carries it; the echo must match it to clear the row
		}
		m_advloot_left.insert(item_data);
		m_advloot_owner.erase(item_data);
		m_advloot_choices.erase(item_data);

		// Live echoes the request (flag 1) to everyone; the client takes the row out of the Shared list
		SerializeBuffer echo;
		echo.WriteUInt16(AdvLootGive);
		echo.WriteUInt32(ask_id);
		echo.WriteUInt32(GetID());
		echo.WriteUInt64(item_id);
		echo.WriteUInt16(std::max<uint16>(1, quantity));
		echo.WriteUInt8(1);
		echo.WriteString(c->GetCleanName());
		auto outapp = new EQApplicationPacket(OP_AdvLoot, std::move(echo));

		for (Client *r : GetAdvancedLootRecipients()) {
			r->MessageString(Chat::Loot, ADVLOOT_PLAYER_LEFT_ON_CORPSE, c->GetCleanName(), qty.c_str(), link.c_str(), GetCleanName());
			r->QueuePacket(outapp);
		}
		safe_delete(outapp);

		CheckAdvancedLootDone(); // removes the corpse from the windows once nothing is listed
		return;
	}

	Client *target = entity_list.GetClientByName(to_name.c_str());
	if (!target || !IsAdvancedLootFor(target)) {
		c->Message(Chat::Loot, fmt::format("{} cannot receive loot from this corpse.", to_name).c_str());
		SendAdvLootClearPending(c, GetID(), item_id);

		// the clear alone leaves the Give row busy (out-of-zone target), so send the corpse's rows again
		SerializeBuffer buf;
		buf.WriteUInt32(GetID());
		auto outapp = new EQApplicationPacket(OP_AdvLootRemoveCorpse, std::move(buf));
		c->QueuePacket(outapp);
		safe_delete(outapp);

		SendAdvancedLootItems(c);
		return;
	}

	uint32 ask_id = DropAdvancedLootAsks(item_data);
	if (!ask_id) {
		ask_id = transaction_id; // the client's row still carries it; the echo must match it to clear the row
	}
	m_advloot_owner[item_data] = target->CharacterID();
	m_advloot_choices.erase(item_data);

	// everyone's Shared list drops the item; the target's Personal list gains it (Live echoes the request to all)
	SerializeBuffer buf;
	buf.WriteUInt16(AdvLootGive);
	buf.WriteUInt32(ask_id);
	buf.WriteUInt32(GetID());
	buf.WriteUInt64(item_id);
	buf.WriteUInt16(std::max<uint16>(1, quantity));
	buf.WriteUInt8(0);
	buf.WriteString(target->GetCleanName());

	auto outapp = new EQApplicationPacket(OP_AdvLoot, std::move(buf));
	QueueAdvancedLootRecipients(outapp);
	safe_delete(outapp);

	const auto *data = database.GetItem(item_id);
	const std::string message = fmt::format(
		"{} has given {} to {}.",
		c->GetCleanName(),
		data ? data->Name : "an item",
		target->GetCleanName()
	);
	for (Client *r : GetAdvancedLootRecipients()) {
		r->Message(Chat::Loot, message.c_str());
	}
}

void Corpse::AdvancedChoice(Client *c, uint32 transaction_id, uint32 item_id, uint32 choice)
{
	if (!IsAdvancedLootFor(c) || choice > AdvLootChoiceNeed) {
		return;
	}

	const LootItem *key = nullptr;
	auto            t   = m_advloot_transactions.find(transaction_id);
	if (transaction_id && t != m_advloot_transactions.end()) {
		key = t->second.item;
	}
	else {
		// a choice made before the master looter asks (Live allows this)
		LootItem *item_data = FindAdvancedLootItem(item_id, 0);
		if (!item_data) {
			return;
		}

		key = item_data;
	}

	if (choice == AdvLootChoiceNone) {
		m_advloot_choices[key].erase(c->CharacterID());
	}
	else {
		m_advloot_choices[key][c->CharacterID()] = choice;
		m_advloot_names[c->CharacterID()] = c->GetCleanName();
	}
}

void Corpse::AdvancedAsk(Client *c, uint32 transaction_id, uint32 item_id, uint32 state)
{
	Client *master_looter = GetAdvancedLootMasterLooter();
	if (!master_looter || master_looter != c || !transaction_id) {
		return;
	}

	// the client picks the id (highest it has seen + 1); refuse one that is already in use
	LootItem *item_data = FindAdvancedLootItem(item_id, 0, true);
	if (!item_data || FindAdvancedLootTransaction(transaction_id, c)) {
		SendAdvLootClearPending(c, GetID(), item_id);
		return;
	}

	DropAdvancedLootAsks(item_data); // asking again replaces a completed ask

	AdvLootTransaction t;
	t.item     = item_data;
	t.item_id  = item_id;
	if (state == AdvLootStateFreeGrab) {
		t.state = AdvLootStateFreeGrab; // no timer: it stays up until someone grabs it or the corpse unlocks
	}
	else {
		t.state = state == AdvLootStateAskAutoRoll ? AdvLootStateAskAutoRoll : AdvLootStateAsk;
		t.timer.Start(AdvancedLootAskMilliseconds()); // sent to the client in ms
	}

	auto &stored = m_advloot_transactions[transaction_id] = t;
	for (Client *r : GetAdvancedLootRecipients()) {
		SendAdvancedLootTransaction(r, transaction_id, stored);
	}

	// the frozen TOB client starts a row's countdown (CLL_ controls, FUN_1400a44b0) only from a state update
	// (AdvLoot 0x11), not from the ask itself, so follow the ask with its state
	SendAdvancedLootState(transaction_id, stored);
}

void Corpse::AdvancedSetState(Client *c, uint32 transaction_id, uint32 state)
{
	Client *master_looter = GetAdvancedLootMasterLooter();
	auto    t             = m_advloot_transactions.find(transaction_id);
	if (!master_looter || master_looter != c || t == m_advloot_transactions.end()) {
		return;
	}

	switch (state) {
		case AdvLootStateAsk:
			t->second.timer.Start(AdvancedLootAskMilliseconds());
			break;
		case AdvLootStateAskAutoRoll:
			if (!t->second.timer.Enabled()) {
				t->second.timer.Start(AdvancedLootAskMilliseconds());
			}
			break;
		case AdvLootStateStop:
			t->second.timer.Disable();
			break;
		case AdvLootStateRemoved:
			t->second.state = AdvLootStateRemoved;
			SendAdvancedLootState(transaction_id, t->second);
			m_advloot_transactions.erase(t);
			return;
		default:
			return; // the client only asks for 1, 2, 3 (and 7 when it drops an ask)
	}

	t->second.state = state;
	SendAdvancedLootState(transaction_id, t->second);
}

void Corpse::AdvancedRoll(Client *c, uint32 transaction_id)
{
	Client *master_looter = GetAdvancedLootMasterLooter();
	auto    t             = m_advloot_transactions.find(transaction_id);
	if (
		!master_looter ||
		master_looter != c ||
		t == m_advloot_transactions.end() ||
		t->second.state != AdvLootStateCompleted
	) {
		return;
	}

	ResolveAdvancedLootRoll(transaction_id);
}

void Corpse::AdvancedFreeGrab(Client *c, uint32 transaction_id)
{
	auto t = m_advloot_transactions.find(transaction_id);
	if (!IsAdvancedLootFor(c) || t == m_advloot_transactions.end() || t->second.state != AdvLootStateFreeGrab) {
		return;
	}

	if (!IsAdvancedLootPresent(c) || (m_is_locked && c->Admin() < AccountStatus::GMAdmin)) {
		c->Message(Chat::Red, "You cannot grab items from this corpse right now.");
		return;
	}

	auto *item_data = const_cast<LootItem *>(t->second.item);
	const uint32 item_id = t->second.item_id;

	m_advloot_owner[item_data] = c->CharacterID();
	m_advloot_choices.erase(item_data);

	// same echo as a give: the grabber's Personal list gains the item, everyone's Shared list drops it.
	// The client matches the Shared row on item and ask id (CAdvancedLootWnd FUN_1400a6fe0), so an asked row
	// needs its ask id here; with 0 the row stayed up as Transaction Removed (drawn as a roll).
	SerializeBuffer buf;
	buf.WriteUInt16(AdvLootGive);
	buf.WriteUInt32(transaction_id);
	buf.WriteUInt32(GetID());
	buf.WriteUInt64(item_id);
	buf.WriteUInt16(std::max<uint16>(1, item_data->charges));
	buf.WriteUInt8(0);
	buf.WriteString(c->GetCleanName());

	auto outapp = new EQApplicationPacket(OP_AdvLoot, std::move(buf));
	QueueAdvancedLootRecipients(outapp);
	safe_delete(outapp);

	t->second.state = AdvLootStateRemoved;
	SendAdvancedLootState(transaction_id, t->second);
	m_advloot_transactions.erase(t);

	const auto *data = database.GetItem(item_id);
	const std::string message = fmt::format(
		"{} grabbed {} {} from {}.",
		c->GetCleanName(),
		std::max<uint16>(1, item_data->charges),
		data ? data->Name : "an item",
		GetCleanName()
	);
	for (Client *r : GetAdvancedLootRecipients()) {
		r->Message(Chat::Loot, message.c_str());
	}
}

void Corpse::SendAdvancedLootTransaction(Client *c, uint32 transaction_id, AdvLootTransaction &t)
{
	// u32 id, u64 item, u32 state, u8, u32 time left (ms), u32 corpse count, {u32 corpse, u32 quantity}
	const uint32 time_left = t.timer.Enabled() ? t.timer.GetRemainingTime() : 0;

	SerializeBuffer buf;
	buf.WriteUInt16(AdvLootAsk);
	buf.WriteUInt32(transaction_id);
	buf.WriteUInt64(t.item_id);
	buf.WriteUInt32(t.state);
	buf.WriteUInt8(0);
	buf.WriteUInt32(time_left);
	buf.WriteUInt32(1);
	buf.WriteUInt32(GetID());
	buf.WriteUInt32(1);

	auto outapp = new EQApplicationPacket(OP_AdvLoot, std::move(buf));
	c->QueuePacket(outapp);
	safe_delete(outapp);
}

void Corpse::SendAdvancedLootStateTo(Client *c, uint32 transaction_id, AdvLootTransaction &t)
{
	SerializeBuffer buf;
	buf.WriteUInt16(AdvLootState);
	buf.WriteUInt32(transaction_id);
	buf.WriteUInt32(t.timer.Enabled() ? t.timer.GetRemainingTime() : 0);
	buf.WriteUInt32(t.state);
	buf.WriteUInt8(0);

	auto outapp = new EQApplicationPacket(OP_AdvLoot, std::move(buf));
	c->QueuePacket(outapp);
	safe_delete(outapp);
}

void Corpse::SendAdvancedLootState(uint32 transaction_id, AdvLootTransaction &t)
{
	// u32 id, u32 time left (ms), u32 state, u8
	SerializeBuffer buf;
	buf.WriteUInt16(AdvLootState);
	buf.WriteUInt32(transaction_id);
	buf.WriteUInt32(t.timer.Enabled() ? t.timer.GetRemainingTime() : 0);
	buf.WriteUInt32(t.state);
	buf.WriteUInt8(0);

	auto outapp = new EQApplicationPacket(OP_AdvLoot, std::move(buf));
	QueueAdvancedLootRecipients(outapp);
	safe_delete(outapp);
}

void Corpse::ResolveAdvancedLootRoll(uint32 transaction_id)
{
	auto t = m_advloot_transactions.find(transaction_id);
	if (t == m_advloot_transactions.end()) {
		return;
	}

	const LootItem *key = t->second.item;
	const uint32 item_id  = t->second.item_id;

	// need beats greed: only the best choice anyone made rolls
	struct Roller {
		uint32      character_id;
		std::string name;
		uint16      roll;
	};

	uint32              best_choice = AdvLootChoiceNo;
	std::vector<Roller> rollers;
	for (const auto &[character_id, choice] : m_advloot_choices[key]) {
		if (choice < AdvLootChoiceGreed || !m_advloot_names.contains(character_id)) {
			continue;
		}

		if (choice > best_choice) {
			best_choice = choice;
			rollers.clear();
		}

		if (choice == best_choice) {
			rollers.push_back({
				character_id,
				m_advloot_names[character_id],
				static_cast<uint16>(zone->random.Int(1, ADVLOOT_ROLL_MAX))
			});
		}
	}

	if (rollers.empty()) {
		// nobody wanted it: the item stays in the Shared list for the master looter
		t->second.state = AdvLootStateCompleted;
		t->second.timer.Disable();
		SendAdvancedLootState(transaction_id, t->second);

		const auto *data = database.GetItem(item_id);
		const std::string message = fmt::format("Nobody chose to roll on {}.", data ? data->Name : "the item");
		for (Client *r : GetAdvancedLootRecipients()) {
			r->Message(Chat::Loot, message.c_str());
		}

		// keep the completed ask: the master looter can still Give, Roll or Leave it from the same row
		return;
	}

	auto winner = std::max_element(
		rollers.begin(),
		rollers.end(),
		[](const Roller &a, const Roller &b) { return a.roll < b.roll; }
	);

	// u32 id, u32 (Live: 3), u8, u16 count, {u16 roll, cstr name}; the client prints each roll and marks the winner
	SerializeBuffer buf;
	buf.WriteUInt16(AdvLootRollResult);
	buf.WriteUInt32(transaction_id);
	buf.WriteUInt32(best_choice);
	buf.WriteUInt8(0);
	buf.WriteUInt16(rollers.size());
	for (const auto &r : rollers) {
		buf.WriteUInt16(r.roll);
		buf.WriteString(r.name);
	}

	auto outapp = new EQApplicationPacket(OP_AdvLoot, std::move(buf));
	QueueAdvancedLootRecipients(outapp);
	safe_delete(outapp);

	m_advloot_owner[key] = winner->character_id;
	m_advloot_choices.erase(key);
	m_advloot_transactions.erase(t);

	const auto *data = database.GetItem(item_id);
	const std::string message = fmt::format(
		"{} won {} with a roll of {} ({}).",
		winner->name,
		data ? data->Name : "the item",
		winner->roll,
		best_choice == AdvLootChoiceNeed ? "need" : "greed"
	);
	for (Client *r : GetAdvancedLootRecipients()) {
		r->Message(Chat::Loot, message.c_str());
	}
}

void Corpse::ProcessAdvancedLoot()
{
	if (m_advloot_transactions.empty()) {
		return;
	}

	std::vector<uint32> expired;
	for (auto &[id, t] : m_advloot_transactions) {
		if (t.timer.Enabled() && t.timer.Check(false)) {
			t.timer.Disable();
			expired.push_back(id);
		}
	}

	for (uint32 id : expired) {
		auto t = m_advloot_transactions.find(id);
		if (t == m_advloot_transactions.end()) {
			continue;
		}

		if (t->second.state == AdvLootStateAskAutoRoll) {
			ResolveAdvancedLootRoll(id);
		}
		else if (t->second.state == AdvLootStateAsk) {
			// Live: the ask completes and the master looter decides (Roll, Give, or ask again)
			t->second.state = AdvLootStateCompleted;
			SendAdvancedLootState(id, t->second);
		}
	}
}

// the master looter gave or left an item whose ask had completed: drop the ask for everyone (state 7).
// Returns the dropped ask id (0 = none): the give/leave echo must carry it to clear the asked Shared row.
uint32 Corpse::DropAdvancedLootAsks(const LootItem *item)
{
	uint32 ask_id = 0;
	for (auto t = m_advloot_transactions.begin(); t != m_advloot_transactions.end();) {
		if (t->second.item == item) {
			ask_id = t->first;
			t->second.state = AdvLootStateRemoved;
			t->second.timer.Disable();
			SendAdvancedLootState(t->first, t->second);
			t = m_advloot_transactions.erase(t);
		}
		else {
			++t;
		}
	}

	return ask_id;
}

// an item left the corpse (looted, or removed by a GM or a quest): drop its reservation, choices and ask
void Corpse::ForgetAdvancedLootItem(const LootItem *item)
{
	if (m_advloot_recipients.empty()) {
		return;
	}

	m_advloot_owner.erase(item);
	m_advloot_left.erase(item);
	m_advloot_choices.erase(item);

	for (auto t = m_advloot_transactions.begin(); t != m_advloot_transactions.end();) {
		if (t->second.item == item) {
			t->second.state = AdvLootStateRemoved;
			t->second.timer.Disable();
			SendAdvancedLootState(t->first, t->second);
			t = m_advloot_transactions.erase(t);
		}
		else {
			++t;
		}
	}
}

void Corpse::CheckAdvancedLootDone()
{
	if (HasAdvancedLootItems()) {
		return;
	}

	EndAdvancedLoot();

	// an emptied corpse goes away, as after classic looting
	if (IsEmpty()) {
		Delete();
		return;
	}

	// Live: once every item is looted or left, the corpse unlocks and anyone may loot what was left
	if (m_private_corpse) {
		return;
	}

	for (int &allowed_looter : m_allowed_looters) {
		allowed_looter = 0;
	}
	m_corpse_delay_timer.Disable();
}

bool Corpse::IsAdvancedLootPresent(Client *c)
{
	return c && m_advloot_present.contains(c->CharacterID());
}

void Corpse::MarkAdvancedLootPresent(Client *c)
{
	if (!IsAdvancedLootFor(c) || IsAdvancedLootPresent(c)) {
		return;
	}

	m_advloot_present.insert(c->CharacterID());

	// replace the locked rows: remove the corpse from this player's window, then send it again unlocked
	SerializeBuffer buf;
	buf.WriteUInt32(GetID());
	auto outapp = new EQApplicationPacket(OP_AdvLootRemoveCorpse, std::move(buf));
	c->QueuePacket(outapp);
	safe_delete(outapp);

	SendAdvancedLootItems(c);
}

void Corpse::EndAdvancedLoot()
{
	if (m_advloot_recipients.empty()) {
		return;
	}

	auto recipients = GetAdvancedLootRecipients();

	m_advloot_recipients.clear();
	m_advloot_group_id = 0;
	m_advloot_raid_id  = 0;
	m_advloot_owner.clear();
	m_advloot_left.clear();
	m_advloot_choices.clear();
	m_advloot_transactions.clear();
	m_advloot_names.clear();
	m_advloot_present.clear();

	SerializeBuffer buf;
	buf.WriteUInt32(GetID());

	auto outapp = new EQApplicationPacket(OP_AdvLootRemoveCorpse, std::move(buf));
	for (Client *c : recipients) {
		c->QueuePacket(outapp);
	}
	safe_delete(outapp);
}

void Corpse::SendAdvancedLootSync(Client *c)
{
	if (!c || c->ClientVersion() < EQ::versions::ClientVersion::TOB) {
		return;
	}

	// Live's reply: u32 0, master looter, no corpses, no transactions, no choices. The corpses (and their asks)
	// follow one by one so their item links go through the TOB encoder.
	std::string master_looter_name = c->GetCleanName();
	Raid  *raid  = c->GetRaid();
	Group *group = c->GetGroup();
	if (raid && raid->GetAdvancedLootMasterLooter()) {
		master_looter_name = raid->GetAdvancedLootMasterLooter()->GetCleanName();
	}
	else if (group && group->GetAdvancedLootMasterLooter()) {
		master_looter_name = group->GetAdvancedLootMasterLooter()->GetCleanName();
	}

	SerializeBuffer buf;
	buf.WriteUInt16(AdvLootSync);
	buf.WriteUInt32(0);
	buf.WriteString(master_looter_name);
	buf.WriteUInt32(0);
	buf.WriteUInt8(0);
	buf.WriteUInt8(0);

	auto outapp = new EQApplicationPacket(OP_AdvLoot, std::move(buf));
	c->QueuePacket(outapp);
	safe_delete(outapp);

	SendAdvLootCapabilities(c);

	if (raid || group) {
		AnnounceAdvancedLootMasterLooter(c, master_looter_name);
	}

	for (const auto &[id, corpse] : entity_list.GetCorpseList()) {
		if (corpse && corpse->HasAdvancedLootItemsFor(c)) {
			corpse->SendAdvancedLootItems(c);
		}
	}
}

// transaction ids are chosen by the master looter's client, so they are only unique within that master looter's asks
// The client builds the master looter's Give-to list from its group/raid roster only when it becomes master
// looter, so a roster change needs the master looter to be announced again: an empty name first (the client
// drops the role), then the leader (it takes it back and rebuilds the list).
void Corpse::AnnounceAdvancedLootMasterLooter(Client *to, const std::string &name)
{
	if (!to || to->ClientVersion() < EQ::versions::ClientVersion::TOB) {
		return;
	}

	SendAdvLootMasterLooter(to, "");
	SendAdvLootMasterLooter(to, name);
}

Corpse *Corpse::FindAdvancedLootTransaction(uint32 transaction_id, Client *master_looter, Client *recipient)
{
	for (const auto &[id, corpse] : entity_list.GetCorpseList()) {
		if (
			corpse &&
			corpse->HasAdvancedLootTransaction(transaction_id) &&
			(!master_looter || corpse->GetAdvancedLootMasterLooter() == master_looter) &&
			(!recipient || corpse->IsAdvancedLootFor(recipient))
		) {
			return corpse;
		}
	}

	return nullptr;
}
