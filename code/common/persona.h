#pragma once

#include "common/ptimer.h"
#include "common/types.h"

#include <limits>
#include <map>
#include <utility>
#include <vector>

class Database;

// Personas (TOB+): one character owns several classes. The active class lives in the normal character tables;
// every other owned class is kept in the character_persona_* mirrors, keyed by (character_id, class_id).
namespace Persona {
	// owned class -> level, the active class included
	using ClassLevels = std::map<uint8, uint8>;

	// class-bound persistent timer types, moved with their persona; all others stay with the character
	constexpr std::pair<pTimerType, pTimerType> MOVED_TIMER_RANGES[] = {
		{pTimerDisciplineReuseStart, pTimerDisciplineReuseEnd},
		{pTimerCombatAbility, pTimerCombatAbility2},
		{pTimerLinkedSpellReuseStart, pTimerLinkedSpellReuseEnd},
		{pTimerLayHands, pTimerLayHands},
		{pTimerHarmTouch, pTimerHarmTouch},
		{pTimerAAStart, pTimerAAEnd},
		{pTimerAAEffectStart, pTimerAAEffectEnd},
		{pTimerSpellStart, std::numeric_limits<pTimerType>::max()},
	};

	// a new persona starts at level 1 with the practice points a new character gets
	constexpr uint8  NEW_PERSONA_LEVEL  = 1;
	constexpr uint32 NEW_PERSONA_POINTS = 5;

	struct Bind {
		uint32 slot;
		uint32 zone_id;
		uint32 instance_id;
		float  x;
		float  y;
		float  z;
		float  heading;
	};

	// what the character creator chose for a new persona
	struct NewPersona {
		uint8  class_id;
		uint16 race;
		uint32 deity;
		uint8  gender;
		uint32 face;
		uint8  hair_color;
		uint8  hair_style;
		uint8  beard;
		uint8  beard_color;
		uint8  eye_color_1;
		uint8  eye_color_2;
		uint32 drakkin_heritage;
		uint32 drakkin_tattoo;
		uint32 drakkin_details;
		uint32 str;
		uint32 sta;
		uint32 cha;
		uint32 dex;
		uint32 int_;
		uint32 agi;
		uint32 wis;

		std::vector<std::pair<uint16, uint16>> skills; // skill id, value; shared skills are skipped
		std::vector<Bind>                      binds;
	};

	// tradeskills stay with the character
	bool IsSharedSkill(uint16 skill_id);

	// checks once per process that every table the mover touches is transactional; logs any that is not
	void CheckStorageEngines(Database &db);
	bool IsStorageTransactional();

	// owned classes from character_personas plus the active class and level
	ClassLevels LoadClassLevels(Database &db, uint32 character_id, uint8 active_class, uint8 active_level);

	// alternate classes owned beyond the base class
	uint32 GetAlternateCount(const ClassLevels &classes);

	// stores the active class in the mirrors and makes new_class (an owned persona) active, in one
	// transaction; the caller saves the character first and reloads it afterwards
	bool Switch(Database &db, uint32 character_id, uint8 new_class);

	// stores the active class in the mirrors and writes a fresh persona as the active class, in one transaction
	bool Create(Database &db, uint32 character_id, const NewPersona &persona);

	// removes an inactive persona's mirror rows
	bool Delete(Database &db, uint32 character_id, uint8 class_id);

	// keeps the active persona's character_personas row current; does nothing for a character without personas
	void UpdateLevel(Database &db, uint32 character_id, uint8 class_id, uint8 level);

	// the persona window's equipment set buttons (TOB numbering)
	enum EquipmentSetAction : uint32 {
		EquipmentSetLoad = 2,
		EquipmentSetSave = 3,
	};

	// the gear a class last wore, by worn slot and item serial
	struct EquippedItem {
		int16  slot_id;
		uint32 item_serial;
	};
	void SaveEquipmentSet(Database &db, uint32 character_id, uint8 class_id, const std::vector<EquippedItem> &items);
	std::vector<EquippedItem> LoadEquipmentSet(Database &db, uint32 character_id, uint8 class_id);

	// item serial -> bit (class - 1) set for every class whose set holds it
	std::map<uint32, uint16> LoadEquipmentSetOwners(Database &db, uint32 character_id);
}
