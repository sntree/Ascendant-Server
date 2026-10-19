#pragma once

#include "common/types.h"

namespace Achievement {
	// per-achievement state in OP_AchievementRequestTrigger and OP_AchievementCompare
	enum class State : uint32 {
		Completed = 0,
		Open      = 1,
		Locked    = 2,
		Hidden    = 3,
	};

	// component type column of the client's AchievementComponentsClient.txt
	enum class ComponentType : uint8 {
		Special     = 0,
		Required    = 1,
		Optional    = 2,
		Requirement = 3,
	};

	// what advances a component; stored in achievement_components.event_type, so append only
	enum class EventType : uint8 {
		None                = 0,  // quests and GM commands only
		ReachLevel          = 1,  // event_count = level
		SkillValue          = 2,  // event_id = skill, event_count = value
		SkillMaxForLevel    = 3,  // event_id = skill, event_count = level whose class cap must be reached
		SpendAAPoints       = 4,  // event_count = points
		CompleteAchievement = 5,  // event_id = achievement
		KillNPC             = 6,  // event_id = npc type, event_count = kills
		LootItem            = 7,  // event_id = item, event_count = items
		CollectItem         = 8,  // event_id = item, consumed by a right click
		CompleteTask        = 9,  // event_id = task
		EnterZone           = 10, // event_id = zone
		OwnItem             = 11, // event_id = item, in the inventory, bank or key ring
		KillNamedNPC        = 12, // the description names the NPC, event_count = kills
	};
}
