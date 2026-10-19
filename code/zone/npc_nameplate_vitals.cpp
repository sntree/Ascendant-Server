#include "zone/npc.h"

#include "common/rulesys.h"
#include "common/timer.h"
#include "zone/client.h"
#include "zone/entity.h"
#include "zone/groups.h"
#include "zone/position.h"
#include "zone/raids.h"

#include <algorithm>

// ToB clients draw HP and mana bars over every NPC near them, not only the one they target. The target, x-target and
// group paths in Mob::SendHPUpdate only reach clients that already follow the NPC, so the rest get their numbers here.

namespace {
	// about two updates a second per NPC; the latest change goes out from NPC::Process once the interval is over
	constexpr uint32 NAMEPLATE_MIN_INTERVAL_MS = 500;

	uint8 ToPercent(float ratio)
	{
		return static_cast<uint8>(std::clamp(static_cast<int>(ratio), 0, 100));
	}

	// most zones have no ToB player in them; look a few times a minute instead of walking every NPC's close list
	constexpr uint32 NAMEPLATE_CLIENT_RECHECK_MS = 5000;

	bool ZoneHasToBClients()
	{
		static Timer recheck(NAMEPLATE_CLIENT_RECHECK_MS);
		static bool  checked = false;
		static bool  has_tob = false;

		if (!checked || recheck.Check()) {
			checked = true;
			has_tob = std::any_of(
				entity_list.GetClientList().begin(),
				entity_list.GetClientList().end(),
				[](const auto &e) {
					return e.second && e.second->ClientVersion() >= EQ::versions::ClientVersion::TOB;
				}
			);
		}

		return has_tob;
	}
}

void NPC::SendNameplateVitals()
{
	m_nameplate_vitals_pending = false;

	const float range = static_cast<float>(RuleI(Character, ToBNameplateVitalsRange));
	if (range <= 0.0f || !IsTargetable() || IsZoneController() || !ZoneHasToBClients()) {
		return;
	}

	const bool  has_mana     = GetMaxMana() > 0;
	const uint8 hp_percent   = ToPercent(GetHPRatio());
	const uint8 mana_percent = has_mana ? ToPercent(GetManaRatio()) : m_nameplate_mana_percent;
	const bool  hp_changed   = hp_percent != m_nameplate_hp_percent;
	const bool  mana_changed = mana_percent != m_nameplate_mana_percent;
	if (!hp_changed && !mana_changed) {
		return;
	}

	const uint32 now = Timer::GetCurrentTime();
	if (now - m_nameplate_sent_at < NAMEPLATE_MIN_INTERVAL_MS) {
		m_nameplate_vitals_pending = true;
		return;
	}

	m_nameplate_sent_at      = now;
	m_nameplate_hp_percent   = hp_percent;
	m_nameplate_mana_percent = mana_percent;

	EQApplicationPacket hp_packet(OP_MobHealth, sizeof(SpawnHPUpdate_Struct2));
	auto hp_update      = reinterpret_cast<SpawnHPUpdate_Struct2 *>(hp_packet.pBuffer);
	hp_update->spawn_id = GetID();
	hp_update->hp       = hp_percent;

	EQApplicationPacket mana_packet(OP_MobManaUpdate, sizeof(MobManaUpdate_Struct));
	auto mana_update      = reinterpret_cast<MobManaUpdate_Struct *>(mana_packet.pBuffer);
	mana_update->spawn_id = GetID();
	mana_update->mana     = mana_percent;

	// a pet's owner and the owner's group already get its HP from Mob::SendHPUpdate
	Mob    *owner        = GetOwner();
	Client *owner_client = owner && owner->IsClient() ? owner->CastToClient() : nullptr;
	Group  *owner_group  = owner_client ? owner_client->GetGroup() : nullptr;
	Raid   *owner_raid   = owner_client ? owner_client->GetRaid() : nullptr;

	const float range_squared = range * range;
	for (auto &e : GetCloseMobList(range)) {
		Mob *mob = e.second;
		if (!mob || !mob->IsClient()) {
			continue;
		}

		Client *client = mob->CastToClient();
		if (client->ClientVersion() < EQ::versions::ClientVersion::TOB || !client->Connected()) {
			continue;
		}

		if (DistanceSquared(client->GetPosition(), GetPosition()) >= range_squared) {
			continue;
		}

		// those paths send no mana for an NPC, so only the HP is skipped
		const bool gets_hp_already =
			client->GetTarget() == this ||
			client->IsXTarget(this) ||
			client == owner_client ||
			(owner_group && owner_group->IsGroupMember(client)) ||
			(owner_raid && owner_raid->IsRaidMember(client));

		if (hp_changed && !gets_hp_already) {
			client->QueuePacket(&hp_packet, false, Client::CLIENT_CONNECTED);
		}

		if (mana_changed && has_mana) {
			client->QueuePacket(&mana_packet, false, Client::CLIENT_CONNECTED);
		}
	}
}
