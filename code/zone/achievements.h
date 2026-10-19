#pragma once

#include "common/achievements.h"
#include "common/types.h"

#include <array>
#include <string>
#include <unordered_map>
#include <vector>

struct AchievementComponent {
	Achievement::ComponentType type;
	uint32                     component_id;
	Achievement::EventType     event_type;
	uint32                     event_id;
	uint32                     event_count;
	uint8                      group;
	uint16                     bit;
};

struct AchievementDefinition {
	uint32                            id;
	std::string                       name;
	std::array<uint16, 3>             group_bits;
	std::vector<AchievementComponent> components;
};

struct AchievementComponentRef {
	uint32 definition_index;
	uint32 component_index;
};

// Achievement content, imported from the client's Resources/Achievements files. The client reads state records in
// its own file order (ascending id) and sizes each record from its own component lists, so the content tables must
// match the files the players have.
class AchievementDefinitions {
public:
	static constexpr uint8 NO_GROUP = 0xFF;

	void Load();
	bool IsLoaded() const { return m_loaded; }
	const std::vector<AchievementDefinition> &GetDefinitions() const { return m_definitions; }
	int GetIndex(uint32 achievement_id) const;
	int GetComponentIndex(uint32 definition_index, uint32 component_id) const;
	const std::vector<AchievementComponentRef> *GetComponentsForEvent(Achievement::EventType type, uint32 event_id) const;
	const std::vector<uint32> &GetOwnedItemIds() const { return m_owned_item_ids; }
	const std::vector<AchievementComponentRef> *GetComponentsForNPCName(const std::string &npc_name) const;

	static uint32 GetRequiredProgress(const AchievementComponent &component);
	static bool IsStoredProgress(Achievement::EventType type);

private:
	bool                                                             m_loaded = false;
	std::vector<AchievementDefinition>                               m_definitions;
	std::unordered_map<uint32, uint32>                               m_index_by_id;
	std::unordered_map<uint64, std::vector<AchievementComponentRef>> m_components_by_event;
	std::vector<uint32>                                              m_owned_item_ids;
	std::unordered_map<std::string, std::vector<AchievementComponentRef>> m_components_by_npc_name;
};

struct CharacterAchievement {
	uint32              completed_at = 0;
	std::vector<uint32> progress;
};
