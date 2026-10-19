#include "common/content/world_content_service.h"
#include "common/races.h"
#include "common/repositories/achievement_components_repository.h"
#include "common/repositories/achievements_repository.h"
#include "common/repositories/character_achievement_components_repository.h"
#include "common/repositories/character_achievements_repository.h"
#include "common/rulesys.h"
#include "common/compression.h"
#include "common/serialize_buffer.h"
#include "common/strings.h"

#include "zone/achievements.h"
#include "zone/client.h"
#include "zone/entity.h"
#include "zone/quest_parser_collection.h"
#include "zone/string_ids.h"
#include "zone/zone.h"
#include "zone/zonedb.h"

#include <algorithm>
#include <ctime>

// TOB+ achievements and collections. Definitions come from the content tables; a character's completions and
// counted progress are stored, while progress that follows from the profile (level, skills, spent AA points) is
// worked out again at each zone-in.

namespace {
	constexpr uint8 RECORDS_WITH_INDEX = 0;
	constexpr uint8 RECORDS_IN_ORDER   = 1;
	constexpr uint32 BITS_PER_WORD     = 16;

	// Requirement component ids in the client's AchievementComponentsClient.txt. Class, level and expansion
	// requirements encode their value as an offset from a base id.
	constexpr uint32 CLASS_REQUIREMENT_BASE      = 1302; // 1303 Warrior .. 1318 Berserker
	constexpr uint32 LEVEL_CAP_REQUIREMENT_BASE  = 1600; // 1605 level cap 5 .. 1800 level cap 200
	constexpr uint32 LEVEL_CAP_REQUIREMENT_LAST  = 1800;
	constexpr uint32 LEVEL_REQUIREMENT_BASE      = 2008; // 2013 level 5 .. 2093 level 85
	constexpr uint32 LEVEL_REQUIREMENT_LAST      = 2093;
	constexpr uint32 EXPANSION_REQUIREMENT_BASE  = 6999; // 7005 Lost Dungeons of Norrath .. 7014 Seeds of Destruction
	constexpr uint32 EXPANSION_REQUIREMENT_LAST  = 7014;
	constexpr uint32 GNOME_REQUIREMENT           = 9008;
	constexpr uint32 LEVEL_LOCKED_REQUIREMENT    = 10014;
	constexpr uint32 EVENT_ACTIVE_REQUIREMENT    = 27122;
	constexpr uint32 EPIC_DISPLAY_FIRST          = 260001;
	constexpr uint32 EPIC_DISPLAY_LAST           = 260004;
	constexpr uint32 VISIBILITY_FIRST            = 590101;
	constexpr uint32 VISIBILITY_LAST             = 590121;
	constexpr uint32 EVENT_DISPLAY_FIRST         = 600000;
	constexpr uint32 EVENT_DISPLAY_LAST          = 600999;
	constexpr uint32 HEROS_FORGE_DISPLAY         = 999999;

	uint64 GetEventKey(Achievement::EventType type, uint32 event_id)
	{
		return (static_cast<uint64>(type) << 32) | event_id;
	}

	// Required components and the requirement rows that carry a sequence share the first bit group, optional
	// components the second, special ones the third. Requirement rows at sequence 0 have no bit at all. Checked
	// against all 6594 records of a Live state dump.
	uint8 GetComponentGroup(Achievement::ComponentType type, uint32 sequence)
	{
		switch (type) {
			case Achievement::ComponentType::Required:
				return 0;
			case Achievement::ComponentType::Requirement:
				return sequence ? 0 : AchievementDefinitions::NO_GROUP;
			case Achievement::ComponentType::Optional:
				return 1;
			default:
				return 2;
		}
	}
}

void AchievementDefinitions::Load()
{
	m_loaded = true;
	m_definitions.clear();
	m_index_by_id.clear();
	m_components_by_event.clear();
	m_owned_item_ids.clear();
	m_components_by_npc_name.clear();

	const auto achievements = AchievementsRepository::GetWhere(content_db, "TRUE ORDER BY `id`");
	const auto components   = AchievementComponentsRepository::GetWhere(
		content_db,
		"TRUE ORDER BY `achievement_id`, `sequence`, `component_id`"
	);

	m_definitions.reserve(achievements.size());
	for (const auto &e : achievements) {
		m_index_by_id[e.id] = static_cast<uint32>(m_definitions.size());
		m_definitions.push_back(AchievementDefinition{ .id = e.id, .name = e.name, .group_bits = {0, 0, 0} });
	}

	uint32 component_count = 0;
	for (const auto &e : components) {
		const auto it = m_index_by_id.find(e.achievement_id);
		if (it == m_index_by_id.end()) {
			continue;
		}

		auto &definition = m_definitions[it->second];

		AchievementComponent c{};
		c.type         = static_cast<Achievement::ComponentType>(e.component_type);
		c.component_id = e.component_id;
		c.event_type   = static_cast<Achievement::EventType>(e.event_type);
		c.event_id     = e.event_id;
		c.event_count  = e.event_count;
		c.group        = GetComponentGroup(c.type, e.sequence);
		if (c.group != NO_GROUP) {
			c.bit = definition.group_bits[c.group]++;
		}

		if (c.event_type == Achievement::EventType::OwnItem) {
			m_owned_item_ids.push_back(c.event_id);
		}

		if (c.event_type == Achievement::EventType::KillNamedNPC) {
			m_components_by_npc_name[Strings::ToLower(e.description)].push_back(
				AchievementComponentRef{
					.definition_index = it->second,
					.component_index = static_cast<uint32>(definition.components.size())
				}
			);
		}

		const bool by_event_id = c.event_type != Achievement::EventType::None &&
			c.event_type != Achievement::EventType::KillNamedNPC;
		if (by_event_id) {
			m_components_by_event[GetEventKey(c.event_type, c.event_id)].push_back(
				AchievementComponentRef{
					.definition_index = it->second,
					.component_index = static_cast<uint32>(definition.components.size())
				}
			);
		}

		definition.components.push_back(c);
		++component_count;
	}

	std::sort(m_owned_item_ids.begin(), m_owned_item_ids.end());
	m_owned_item_ids.erase(std::unique(m_owned_item_ids.begin(), m_owned_item_ids.end()), m_owned_item_ids.end());

	LogAchievements("Loaded [{}] achievements with [{}] components", m_definitions.size(), component_count);
}

int AchievementDefinitions::GetIndex(uint32 achievement_id) const
{
	const auto it = m_index_by_id.find(achievement_id);
	return it == m_index_by_id.end() ? -1 : static_cast<int>(it->second);
}

int AchievementDefinitions::GetComponentIndex(uint32 definition_index, uint32 component_id) const
{
	if (definition_index >= m_definitions.size()) {
		return -1;
	}

	const auto &components = m_definitions[definition_index].components;
	for (size_t i = 0; i < components.size(); ++i) {
		if (components[i].component_id == component_id && components[i].type != Achievement::ComponentType::Requirement) {
			return static_cast<int>(i);
		}
	}

	return -1;
}

const std::vector<AchievementComponentRef> *AchievementDefinitions::GetComponentsForEvent(
	Achievement::EventType type,
	uint32 event_id
) const
{
	const auto it = m_components_by_event.find(GetEventKey(type, event_id));
	return it == m_components_by_event.end() ? nullptr : &it->second;
}

const std::vector<AchievementComponentRef> *AchievementDefinitions::GetComponentsForNPCName(const std::string &npc_name) const
{
	const auto it = m_components_by_npc_name.find(Strings::ToLower(npc_name));
	return it == m_components_by_npc_name.end() ? nullptr : &it->second;
}

uint32 AchievementDefinitions::GetRequiredProgress(const AchievementComponent &component)
{
	switch (component.event_type) {
		case Achievement::EventType::ReachLevel:
		case Achievement::EventType::SkillValue:
		case Achievement::EventType::SpendAAPoints:
		case Achievement::EventType::KillNPC:
		case Achievement::EventType::KillNamedNPC:
		case Achievement::EventType::LootItem:
			return std::max<uint32>(component.event_count, 1);
		default:
			return 1;
	}
}

// Progress that follows from the profile is not stored; everything else is.
bool AchievementDefinitions::IsStoredProgress(Achievement::EventType type)
{
	switch (type) {
		case Achievement::EventType::ReachLevel:
		case Achievement::EventType::SkillValue:
		case Achievement::EventType::SkillMaxForLevel:
		case Achievement::EventType::SpendAAPoints:
			return false;
		default:
			return true;
	}
}

const AchievementDefinitions &Zone::GetAchievementDefinitions()
{
	if (!m_achievement_definitions.IsLoaded()) {
		m_achievement_definitions.Load();
	}

	return m_achievement_definitions;
}

bool Client::AreAchievementsEnabled() const
{
	return ClientVersion() >= EQ::versions::ClientVersion::TOB && RuleB(Character, EnableAchievements);
}

void Client::LoadAchievements()
{
	const auto &definitions = zone->GetAchievementDefinitions();

	m_achievements.clear();
	m_achievement_updates.clear();

	const auto completed = CharacterAchievementsRepository::GetWhere(
		database,
		fmt::format("`character_id` = {}", CharacterID())
	);

	for (const auto &e : completed) {
		const int index = definitions.GetIndex(e.achievement_id);
		if (index >= 0) {
			m_achievements[index].completed_at = e.completed_at;
		}
	}

	const auto progress = CharacterAchievementComponentsRepository::GetWhere(
		database,
		fmt::format("`character_id` = {}", CharacterID())
	);

	for (const auto &e : progress) {
		const int index = definitions.GetIndex(e.achievement_id);
		if (index < 0) {
			continue;
		}

		const auto &components = definitions.GetDefinitions()[index].components;
		for (size_t i = 0; i < components.size(); ++i) {
			if (components[i].component_id != e.component_id ||
				static_cast<uint8>(components[i].type) != e.component_type) {
				continue;
			}

			auto &state = m_achievements[index];
			state.progress.resize(components.size());
			state.progress[i] = e.progress;
			break;
		}
	}

	m_achievements_loaded = true;

	// Completions found while catching up are saved in one go and not announced.
	m_achievements_loading = true;
	ReconcileAchievementFacts();
	AchievementEvent(Achievement::EventType::EnterZone, zone->GetZoneID(), 1);
	m_achievements_loading = false;

	if (!m_achievement_completions_to_save.empty()) {
		std::vector<CharacterAchievementsRepository::CharacterAchievements> rows;
		rows.reserve(m_achievement_completions_to_save.size());
		for (const uint32 index : m_achievement_completions_to_save) {
			rows.push_back(
				CharacterAchievementsRepository::CharacterAchievements{
					.character_id = CharacterID(),
					.achievement_id = definitions.GetDefinitions()[index].id,
					.completed_at = m_achievements[index].completed_at
				}
			);
		}

		CharacterAchievementsRepository::ReplaceMany(database, rows);
		m_achievement_completions_to_save.clear();
	}

	m_achievement_updates.clear();

	LogAchievements(
		"[{}] has [{}] completed achievements and [{}] stored progress rows",
		GetCleanName(),
		std::count_if(
			m_achievements.begin(),
			m_achievements.end(),
			[](const auto &e) { return e.second.completed_at > 0; }
		),
		progress.size()
	);
}

void Client::ReconcileAchievementFacts()
{
	AchievementEvent(Achievement::EventType::ReachLevel, 0, GetLevel());
	AchievementEvent(Achievement::EventType::SpendAAPoints, 0, GetSpentAA());

	for (int skill = 0; skill <= EQ::skills::HIGHEST_SKILL; ++skill) {
		const auto skill_id = static_cast<EQ::skills::SkillType>(skill);
		AchievementEvent(Achievement::EventType::SkillValue, skill, GetRawSkill(skill_id));
		AchievementEvent(Achievement::EventType::SkillMaxForLevel, skill, GetRawSkill(skill_id));
	}

	constexpr uint8 owned = invWhereWorn | invWherePersonal | invWhereBank | invWhereSharedBank | invWhereCursor;
	for (const uint32 item_id : zone->GetAchievementDefinitions().GetOwnedItemIds()) {
		if (KeyRingCheck(item_id) || m_inv.HasItem(item_id, 1, owned) != INVALID_INDEX) {
			AchievementEvent(Achievement::EventType::OwnItem, item_id, 1);
		}
	}

	// achievements completed before content that depends on them was added
	const auto &definitions = zone->GetAchievementDefinitions().GetDefinitions();
	std::vector<uint32> completed_ids;
	for (const auto &[index, state] : m_achievements) {
		if (state.completed_at > 0) {
			completed_ids.push_back(definitions[index].id);
		}
	}

	for (const uint32 achievement_id : completed_ids) {
		AchievementEvent(Achievement::EventType::CompleteAchievement, achievement_id, 1);
	}
}

// The client reads its Resources/Achievements files when this arrives, but keeps them only when the version equals
// the newest modification time of those files. The compressed list would replace the files' achievements, so it is
// not a way around that. The list itself carries server-defined achievements (Live: the Hero's Forge sets); we
// have none. Live sends this once per login; a second one rereads the files.
// {u32 version, u32 inflated size, zlib {u32 achievement count, achievements, u32 category count, categories}}
void Client::SendAchievementFiles()
{
	const uint32 empty_lists[2] = {0, 0};

	std::vector<char> compressed(EQ::EstimateDeflateBuffer(sizeof(empty_lists)));
	const uint32 compressed_size = EQ::DeflateData(
		reinterpret_cast<const char *>(empty_lists),
		sizeof(empty_lists),
		compressed.data(),
		static_cast<uint32>(compressed.size())
	);
	if (compressed_size == 0) {
		LogAchievements("Failed to compress the achievement list for [{}]", GetCleanName());
		return;
	}

	SerializeBuffer buffer(8 + compressed_size);
	buffer.WriteUInt32(static_cast<uint32>(RuleI(Character, AchievementFileTime)));
	buffer.WriteUInt32(sizeof(empty_lists));
	for (uint32 i = 0; i < compressed_size; ++i) {
		buffer.WriteUInt8(static_cast<uint8>(compressed[i]));
	}

	auto outapp = new EQApplicationPacket(OP_AchievementLoadFiles, std::move(buffer));
	FastQueuePacket(&outapp);
}

void Client::SendAchievementState()
{
	if (!m_achievements_loaded) {
		return;
	}

	const auto &definitions = zone->GetAchievementDefinitions().GetDefinitions();

	SerializeBuffer buffer(9 + definitions.size() * 8);
	buffer.WriteUInt32(CharacterID());
	buffer.WriteUInt8(RECORDS_IN_ORDER);
	buffer.WriteUInt32(static_cast<uint32>(definitions.size()));
	for (uint32 i = 0; i < definitions.size(); ++i) {
		WriteAchievementRecord(buffer, i);
	}

	auto outapp = new EQApplicationPacket(OP_AchievementRequestTrigger, std::move(buffer));
	FastQueuePacket(&outapp);
}

void Client::SendAchievementUpdates()
{
	if (m_achievement_updates.empty() || m_achievements_loading) {
		return;
	}

	std::sort(m_achievement_updates.begin(), m_achievement_updates.end());
	m_achievement_updates.erase(
		std::unique(m_achievement_updates.begin(), m_achievement_updates.end()),
		m_achievement_updates.end()
	);

	SerializeBuffer buffer(9 + m_achievement_updates.size() * 12);
	buffer.WriteUInt32(CharacterID());
	buffer.WriteUInt8(RECORDS_WITH_INDEX);
	buffer.WriteUInt32(static_cast<uint32>(m_achievement_updates.size()));
	for (const uint32 index : m_achievement_updates) {
		buffer.WriteUInt32(index);
		WriteAchievementRecord(buffer, index);
	}

	m_achievement_updates.clear();

	auto outapp = new EQApplicationPacket(OP_AchievementRequestTrigger, std::move(buffer));
	FastQueuePacket(&outapp);
}

// {u32 state, u16 bits per group for each of the three groups, u32 completed_at when completed}
void Client::WriteAchievementRecord(SerializeBuffer &buffer, uint32 definition_index) const
{
	const auto &definition = zone->GetAchievementDefinitions().GetDefinitions()[definition_index];
	const auto  it         = m_achievements.find(definition_index);
	const auto *state      = it == m_achievements.end() ? nullptr : &it->second;

	buffer.WriteUInt32(static_cast<uint32>(GetAchievementState(definition_index)));

	for (uint8 group = 0; group < definition.group_bits.size(); ++group) {
		std::vector<uint16> words((definition.group_bits[group] + BITS_PER_WORD - 1) / BITS_PER_WORD, 0);

		if (state) {
			for (size_t i = 0; i < definition.components.size() && i < state->progress.size(); ++i) {
				const auto &c = definition.components[i];
				const bool  done = state->progress[i] >= AchievementDefinitions::GetRequiredProgress(c);
				if (c.group == group && c.type != Achievement::ComponentType::Requirement && done) {
					words[c.bit / BITS_PER_WORD] |= static_cast<uint16>(1 << (c.bit % BITS_PER_WORD));
				}
			}
		}

		for (const uint16 word : words) {
			buffer.WriteUInt16(word);
		}
	}

	if (state && state->completed_at > 0) {
		buffer.WriteUInt32(state->completed_at);
	}
}

Achievement::State Client::GetAchievementState(uint32 definition_index) const
{
	const auto it = m_achievements.find(definition_index);
	if (it != m_achievements.end() && it->second.completed_at > 0) {
		return Achievement::State::Completed;
	}

	const auto &definition = zone->GetAchievementDefinitions().GetDefinitions()[definition_index];
	return MeetsAchievementRequirements(definition) ? Achievement::State::Open : Achievement::State::Hidden;
}

bool Client::MeetsAchievementRequirements(const AchievementDefinition &definition) const
{
	return std::all_of(
		definition.components.begin(),
		definition.components.end(),
		[this](const AchievementComponent &c) {
			return c.type != Achievement::ComponentType::Requirement || MeetsAchievementRequirement(c.component_id);
		}
	);
}

// Requirements the server can't fulfil (events, Live-only displays, level locked servers) keep the achievement
// hidden; caps the server does not enforce (AA counts, tradeskill caps) count as met.
bool Client::MeetsAchievementRequirement(uint32 component_id) const
{
	if (component_id > CLASS_REQUIREMENT_BASE && component_id <= CLASS_REQUIREMENT_BASE + Class::Berserker) {
		return GetClass() == component_id - CLASS_REQUIREMENT_BASE;
	}

	if (component_id > LEVEL_CAP_REQUIREMENT_BASE && component_id <= LEVEL_CAP_REQUIREMENT_LAST) {
		return static_cast<uint32>(RuleI(Character, MaxLevel)) >= component_id - LEVEL_CAP_REQUIREMENT_BASE;
	}

	if (component_id > LEVEL_REQUIREMENT_BASE && component_id <= LEVEL_REQUIREMENT_LAST) {
		return GetLevel() >= component_id - LEVEL_REQUIREMENT_BASE;
	}

	if (component_id > EXPANSION_REQUIREMENT_BASE && component_id <= EXPANSION_REQUIREMENT_LAST) {
		const int expansion = WorldContentService::Instance()->GetCurrentExpansion();
		const int required  = static_cast<int>(component_id - EXPANSION_REQUIREMENT_BASE);
		return expansion == Expansion::EXPANSION_ALL || expansion >= required;
	}

	if (component_id == GNOME_REQUIREMENT) {
		return GetBaseRace() == Race::Gnome;
	}

	const bool never_met =
		component_id == LEVEL_LOCKED_REQUIREMENT ||
		component_id == EVENT_ACTIVE_REQUIREMENT ||
		component_id == HEROS_FORGE_DISPLAY ||
		(component_id >= EPIC_DISPLAY_FIRST && component_id <= EPIC_DISPLAY_LAST) ||
		(component_id >= VISIBILITY_FIRST && component_id <= VISIBILITY_LAST) ||
		(component_id >= EVENT_DISPLAY_FIRST && component_id <= EVENT_DISPLAY_LAST);

	return !never_met;
}

void Client::AchievementEvent(Achievement::EventType type, uint32 event_id, uint32 value)
{
	if (!m_achievements_loaded) {
		return;
	}

	const auto refs = zone->GetAchievementDefinitions().GetComponentsForEvent(type, event_id);
	if (refs) {
		ApplyAchievementEvent(type, *refs, value);
		SendAchievementUpdates();
	}
}

// Raid and rare-monster achievements name the NPC rather than an NPC type, since one NPC often has several.
void Client::AchievementNPCKill(uint32 npc_type_id, const std::string &npc_name)
{
	if (!m_achievements_loaded) {
		return;
	}

	const auto &achievements = zone->GetAchievementDefinitions();
	if (const auto refs = achievements.GetComponentsForEvent(Achievement::EventType::KillNPC, npc_type_id)) {
		ApplyAchievementEvent(Achievement::EventType::KillNPC, *refs, 1);
	}

	if (const auto refs = achievements.GetComponentsForNPCName(npc_name)) {
		ApplyAchievementEvent(Achievement::EventType::KillNamedNPC, *refs, 1);
	}

	SendAchievementUpdates();
}

void Client::ApplyAchievementEvent(
	Achievement::EventType type,
	const std::vector<AchievementComponentRef> &refs,
	uint32 value
)
{
	const auto &achievements = zone->GetAchievementDefinitions();
	for (const auto &ref : refs) {
		const auto &c        = achievements.GetDefinitions()[ref.definition_index].components[ref.component_index];
		const auto  required = AchievementDefinitions::GetRequiredProgress(c);

		const auto  it      = m_achievements.find(ref.definition_index);
		const bool  has_row = it != m_achievements.end() && ref.component_index < it->second.progress.size();
		const auto  current = has_row ? it->second.progress[ref.component_index] : 0;

		uint32 progress = current;
		switch (type) {
			case Achievement::EventType::ReachLevel:
			case Achievement::EventType::SkillValue:
			case Achievement::EventType::SpendAAPoints:
				progress = std::min(value, required);
				break;
			case Achievement::EventType::SkillMaxForLevel: {
				const auto skill_id  = static_cast<EQ::skills::SkillType>(c.event_id);
				const auto level     = static_cast<uint8>(std::min<uint32>(c.event_count, UINT8_MAX));
				const auto cap       = MaxSkill(skill_id, GetClass(), level);
				const bool maxed_out = cap > 0 && GetLevel() >= level && value >= cap;
				progress = maxed_out ? 1 : 0;
				break;
			}
			case Achievement::EventType::KillNPC:
			case Achievement::EventType::KillNamedNPC:
			case Achievement::EventType::LootItem:
				progress = std::min<uint64>(static_cast<uint64>(current) + value, required);
				break;
			default:
				progress = 1;
				break;
		}

		if (progress != current) {
			SetAchievementProgress(ref.definition_index, ref.component_index, progress);
		}
	}
}

bool Client::SetAchievementProgress(uint32 definition_index, uint32 component_index, uint32 progress)
{
	const auto &definition = zone->GetAchievementDefinitions().GetDefinitions()[definition_index];
	const auto &c          = definition.components[component_index];

	auto &state = m_achievements[definition_index];
	state.progress.resize(definition.components.size());
	if (state.progress[component_index] == progress) {
		return false;
	}

	state.progress[component_index] = progress;
	m_achievement_updates.push_back(definition_index);

	if (AchievementDefinitions::IsStoredProgress(c.event_type)) {
		CharacterAchievementComponentsRepository::ReplaceOne(
			database,
			CharacterAchievementComponentsRepository::CharacterAchievementComponents{
				.character_id = CharacterID(),
				.achievement_id = definition.id,
				.component_type = static_cast<uint8>(c.type),
				.component_id = c.component_id,
				.progress = progress
			}
		);
	}

	TryCompleteAchievement(definition_index);
	return true;
}

void Client::TryCompleteAchievement(uint32 definition_index)
{
	const auto &definition = zone->GetAchievementDefinitions().GetDefinitions()[definition_index];
	const auto &state      = m_achievements[definition_index];
	if (state.completed_at > 0 || !MeetsAchievementRequirements(definition)) {
		return;
	}

	bool has_required = false;
	for (size_t i = 0; i < definition.components.size(); ++i) {
		const auto &c = definition.components[i];
		if (c.type != Achievement::ComponentType::Required) {
			continue;
		}

		has_required = true;
		const uint32 progress = i < state.progress.size() ? state.progress[i] : 0;
		if (progress < AchievementDefinitions::GetRequiredProgress(c)) {
			return;
		}
	}

	if (has_required) {
		MarkAchievementCompleted(definition_index);
	}
}

void Client::MarkAchievementCompleted(uint32 definition_index)
{
	const auto &definition = zone->GetAchievementDefinitions().GetDefinitions()[definition_index];

	auto &state = m_achievements[definition_index];
	state.completed_at = static_cast<uint32>(std::time(nullptr));
	m_achievement_updates.push_back(definition_index);

	LogAchievements("[{}] completed [{}] ({})", GetCleanName(), definition.name, definition.id);

	if (m_achievements_loading) {
		m_achievement_completions_to_save.push_back(definition_index);
	} else {
		CharacterAchievementsRepository::ReplaceOne(
			database,
			CharacterAchievementsRepository::CharacterAchievements{
				.character_id = CharacterID(),
				.achievement_id = definition.id,
				.completed_at = state.completed_at
			}
		);

		MessageString(Chat::Achievement, ACHIEVEMENT_COMPLETED, definition.name.c_str());

		const float range_squared = RuleI(Range, Emote) * RuleI(Range, Emote);
		for (const auto &[id, c] : entity_list.GetClientList()) {
			const bool can_see = c && c != this && c->ClientVersion() >= EQ::versions::ClientVersion::TOB;
			if (can_see && DistanceSquared(GetPosition(), c->GetPosition()) <= range_squared) {
				c->MessageString(Chat::AchievementOther, ACHIEVEMENT_COMPLETED_OTHER, GetCleanName(), definition.name.c_str());
			}
		}
	}

	if (parse->PlayerHasQuestSub(EVENT_ACHIEVEMENT_COMPLETE)) {
		parse->EventPlayer(EVENT_ACHIEVEMENT_COMPLETE, this, std::to_string(definition.id), 0);
	}

	AchievementEvent(Achievement::EventType::CompleteAchievement, definition.id, 1);
}

bool Client::TryCollectItem(int16 slot_id, const EQ::ItemInstance *inst)
{
	if (!m_achievements_loaded || !inst || !inst->GetItem()) {
		return false;
	}

	const auto &achievements = zone->GetAchievementDefinitions();
	const auto  item         = inst->GetItem();
	const auto  refs         = achievements.GetComponentsForEvent(Achievement::EventType::CollectItem, item->ID);
	if (!refs) {
		return false;
	}

	for (const auto &ref : *refs) {
		const auto it = m_achievements.find(ref.definition_index);
		const bool collected =
			it != m_achievements.end() &&
			ref.component_index < it->second.progress.size() &&
			it->second.progress[ref.component_index] > 0;

		if (collected) {
			continue;
		}

		DeleteItemInInventory(slot_id, 1, true);
		MessageString(Chat::Achievement, COLLECTION_ITEM_ADDED, item->Name);
		SetAchievementProgress(ref.definition_index, ref.component_index, 1);
		SendAchievementUpdates();
		return true;
	}

	MessageString(Chat::White, COLLECTION_ALREADY_DONE, item->Name);
	return true;
}

bool Client::IsAchievementCompleted(uint32 achievement_id) const
{
	if (!m_achievements_loaded) {
		return false;
	}

	const int index = zone->GetAchievementDefinitions().GetIndex(achievement_id);
	if (index < 0) {
		return false;
	}

	const auto it = m_achievements.find(index);
	return it != m_achievements.end() && it->second.completed_at > 0;
}

bool Client::CompleteAchievement(uint32 achievement_id)
{
	if (!m_achievements_loaded) {
		return false;
	}

	const auto &achievements = zone->GetAchievementDefinitions();
	const int   index        = achievements.GetIndex(achievement_id);
	if (index < 0 || IsAchievementCompleted(achievement_id)) {
		return false;
	}

	const auto &definition = achievements.GetDefinitions()[index];
	auto       &state      = m_achievements[index];
	state.progress.resize(definition.components.size());
	for (size_t i = 0; i < definition.components.size(); ++i) {
		const auto &c = definition.components[i];
		if (c.type == Achievement::ComponentType::Required) {
			state.progress[i] = AchievementDefinitions::GetRequiredProgress(c);
		}
	}

	MarkAchievementCompleted(index);
	SendAchievementUpdates();
	return true;
}

bool Client::ResetAchievement(uint32 achievement_id)
{
	if (!m_achievements_loaded) {
		return false;
	}

	const int index = zone->GetAchievementDefinitions().GetIndex(achievement_id);
	if (index < 0) {
		return false;
	}

	CharacterAchievementsRepository::DeleteWhere(
		database,
		fmt::format("`character_id` = {} AND `achievement_id` = {}", CharacterID(), achievement_id)
	);
	CharacterAchievementComponentsRepository::DeleteWhere(
		database,
		fmt::format("`character_id` = {} AND `achievement_id` = {}", CharacterID(), achievement_id)
	);

	m_achievements.erase(index);
	m_achievement_updates.push_back(index);
	SendAchievementUpdates();
	return true;
}

uint32 Client::GetAchievementComponentProgress(uint32 achievement_id, uint32 component_id) const
{
	if (!m_achievements_loaded) {
		return 0;
	}

	const auto &achievements = zone->GetAchievementDefinitions();
	const int   index        = achievements.GetIndex(achievement_id);
	const int   component    = index < 0 ? -1 : achievements.GetComponentIndex(index, component_id);
	if (component < 0) {
		return 0;
	}

	const auto it = m_achievements.find(index);
	if (it == m_achievements.end() || static_cast<size_t>(component) >= it->second.progress.size()) {
		return 0;
	}

	return it->second.progress[component];
}

bool Client::UpdateAchievementComponent(uint32 achievement_id, uint32 component_id, uint32 amount)
{
	if (!m_achievements_loaded || !amount) {
		return false;
	}

	const auto &achievements = zone->GetAchievementDefinitions();
	const int   index        = achievements.GetIndex(achievement_id);
	const int   component    = index < 0 ? -1 : achievements.GetComponentIndex(index, component_id);
	if (component < 0) {
		return false;
	}

	const auto  &c        = achievements.GetDefinitions()[index].components[component];
	const uint32 required = AchievementDefinitions::GetRequiredProgress(c);
	const uint32 current  = GetAchievementComponentProgress(achievement_id, component_id);
	const uint32 progress = static_cast<uint32>(std::min<uint64>(static_cast<uint64>(current) + amount, required));

	const bool changed = SetAchievementProgress(index, component, progress);
	SendAchievementUpdates();
	return changed;
}

void Client::Handle_OP_AchievementCompareRequest(const EQApplicationPacket *app)
{
	if (!AreAchievementsEnabled() || app->size != sizeof(AchievementCompareRequest_Struct)) {
		return;
	}

	const auto in     = reinterpret_cast<const AchievementCompareRequest_Struct *>(app->pBuffer);
	const auto target = entity_list.GetClientByID(static_cast<uint16>(in->target_id));
	if (!target || !target->AreAchievementsEnabled() || !target->m_achievements_loaded) {
		return;
	}

	const auto &definitions = zone->GetAchievementDefinitions().GetDefinitions();

	SerializeBuffer buffer(definitions.size() * 8);
	for (uint32 i = 0; i < definitions.size(); ++i) {
		target->WriteAchievementRecord(buffer, i);
	}

	auto outapp = new EQApplicationPacket(OP_AchievementCompare, std::move(buffer));
	FastQueuePacket(&outapp);
}

// The client asks for everything again when a record names an achievement it doesn't have. Answer once per zone
// session, so a server list that doesn't match the client's files can't start a loop.
void Client::Handle_OP_AchievementsCompressed(const EQApplicationPacket *app)
{
	if (!AreAchievementsEnabled() || !m_achievements_loaded) {
		return;
	}

	if (m_achievement_resend_sent) {
		LogAchievements(
			"[{}] asked for the achievement list again: either Character:AchievementFileTime [{}] is not the newest modification time of the client's Resources/Achievements files, or the achievement tables don't match those files",
			GetCleanName(),
			RuleI(Character, AchievementFileTime)
		);
		return;
	}

	m_achievement_resend_sent = true;
	SendAchievementState();
}
