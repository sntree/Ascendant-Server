#include "common/classes.h"
#include "common/deity.h"
#include "common/eq_packet_structs.h"
#include "common/memory_buffer.h"
#include "common/persona.h"
#include "common/rulesys.h"
#include "common/strings.h"
#include "common/tasks.h"

#include "zone/client.h"
#include "zone/client_version.h"
#include "zone/event_codes.h"
#include "zone/groups.h"
#include "zone/guild_mgr.h"
#include "zone/merc.h"
#include "zone/npc.h"
#include "zone/quest_parser_collection.h"
#include "zone/raids.h"
#include "zone/string_ids.h"
#include "zone/task_client_state.h"
#include "zone/zone.h"
#include "zone/zonedb.h"

#include <limits>
#include <string>

// TOB+ personas: a character owns several classes and swaps between them in place (rule Character:PersonaSlots)

namespace {
	// the client's experience bar is this many ticks wide
	constexpr float EXP_BAR_SIZE = 330.0f;

	// a refusal without a chat line
	constexpr uint32 NO_STRING = 0;
}

bool Client::IsPersonasEnabled()
{
	return RuleI(Character, PersonaSlots) > 0 && ClientVersion() >= EQ::versions::ClientVersion::TOB;
}

const Persona::ClassLevels &Client::GetPersonaClasses() const
{
	return m_persona_classes;
}

bool Client::HasPersona(uint8 class_id) const
{
	if (class_id == Class::None) {
		return false;
	}

	return class_id == GetClass() || m_persona_classes.contains(class_id);
}

uint8 Client::GetPersonaLevel(uint8 class_id) const
{
	if (class_id == Class::None) {
		return 0;
	}

	// the cache only learns about a level-up on the next list refresh, the live value is always current
	if (class_id == GetClass()) {
		return GetLevel();
	}

	const auto persona = m_persona_classes.find(class_id);
	return persona == m_persona_classes.end() ? 0 : persona->second;
}

void Client::LoadPersonaClasses()
{
	if (!IsPersonasEnabled()) {
		return;
	}

	m_persona_classes = Persona::LoadClassLevels(database, CharacterID(), GetClass(), GetLevel());
}

void Client::SendPersonaClassList()
{
	if (!IsPersonasEnabled()) {
		return;
	}

	auto outapp = new EQApplicationPacket(OP_PersonaClassList, sizeof(PersonaClassList_Struct));
	auto list   = reinterpret_cast<PersonaClassList_Struct *>(outapp->pBuffer);
	memset(list, 0, sizeof(PersonaClassList_Struct));

	auto add_entry = [&](uint8 class_id, uint8 level) {
		if (class_id == Class::None || level == 0 || list->count >= Class::PLAYER_CLASS_COUNT) {
			return;
		}

		list->entries[list->count].class_id = class_id;
		list->entries[list->count].level    = level;
		list->count++;
	};

	add_entry(GetClass(), GetLevel());
	for (const auto &[class_id, level] : m_persona_classes) {
		if (class_id != GetClass()) {
			add_entry(class_id, level);
		}
	}

	QueuePacket(outapp);
	safe_delete(outapp);
}

void Client::UpdatePersonaLevel()
{
	if (RuleI(Character, PersonaSlots) <= 0) {
		return;
	}

	Persona::UpdateLevel(database, CharacterID(), GetClass(), GetLevel());

	// an empty cache means zone entry has not loaded it yet, which will pick up this level itself
	if (!IsPersonasEnabled() || m_persona_classes.empty()) {
		return;
	}

	m_persona_classes[GetClass()] = GetLevel();
	SendPersonaClassList();
}

bool Client::DeletePersona(uint8 class_id)
{
	if (class_id == GetClass() || class_id == Class::None) {
		return false;
	}

	if (!Persona::Delete(database, CharacterID(), class_id)) {
		return false;
	}

	LogPersonas("Deleted class [{}] of [{}]", class_id, GetCleanName());

	if (IsPersonasEnabled()) {
		LoadPersonaClasses();
		SendPersonaClassList();
	}

	return true;
}

void Client::RefusePersonaRequest(uint32 string_id)
{
	if (string_id != NO_STRING) {
		MessageString(Chat::Red, string_id);
	}

	// pops the request the persona window is waiting on
	EQApplicationPacket ack(OP_RequestAck, 0);
	QueuePacket(&ack);
}

void Client::Handle_OP_PersonaCreate(const EQApplicationPacket *app)
{
	if (!IsPersonasEnabled()) {
		return;
	}

	if (app->size != sizeof(PersonaRequest_Struct)) {
		LogNetcode("Received invalid sized OP_PersonaCreate: got [{}], expected [{}]", app->size, sizeof(PersonaRequest_Struct));
		return;
	}

	const auto request = reinterpret_cast<const PersonaRequest_Struct *>(app->pBuffer);

	uint32 refusal = NO_STRING;
	if (request->target_class < Class::Warrior || request->target_class > Class::Berserker) {
		refusal = PERSONA_CREATE_INVALID_CLASS;
	}
	else if (HasPersona(static_cast<uint8>(request->target_class))) {
		refusal = PERSONA_CREATE_CLASS_OWNED;
	}
	else if (Persona::GetAlternateCount(m_persona_classes) >= static_cast<uint32>(RuleI(Character, PersonaSlots))) {
		refusal = PERSONA_NO_SLOTS;
	}
	else if (dead) {
		refusal = PERSONA_SWITCH_DEAD;
	}
	else if (IsPersonaChangeBlockedByCombat()) {
		refusal = PERSONA_SWITCH_IN_COMBAT;
	}
	else if (!m_inv.CursorEmpty()) {
		refusal = PERSONA_SWITCH_HOLDING_ITEM;
	}

	if (refusal != NO_STRING) {
		LogPersonas("Create of class [{}] refused for [{}] with string [{}]", request->target_class, GetCleanName(), refusal);
		RefusePersonaRequest(refusal);
		return;
	}

	if (request->current_class != GetClass()) {
		LogPersonasDetail(
			"[{}] reports class [{}] but is class [{}]",
			GetCleanName(),
			request->current_class,
			GetClass()
		);
	}

	// there is no refusal form of this packet; any answer starts the client's create sequence
	auto outapp   = new EQApplicationPacket(OP_PersonaCreate, sizeof(PersonaCreateApproval_Struct));
	auto approval = reinterpret_cast<PersonaCreateApproval_Struct *>(outapp->pBuffer);
	memset(approval, 0, sizeof(PersonaCreateApproval_Struct));
	approval->class_id     = request->target_class;
	approval->character_id = CharacterID();
	approval->offer_now    = 1;
	SavePersonaEquipmentSet();
	QueuePacket(outapp);
	safe_delete(outapp);

	LogPersonas("Create of class [{}] approved for [{}]", request->target_class, GetCleanName());

	if (parse->PlayerHasQuestSub(EVENT_PERSONA_CREATE)) {
		parse->EventPlayer(EVENT_PERSONA_CREATE, this, std::to_string(request->target_class), 0);
	}
}

void Client::Handle_OP_PersonaSwitch(const EQApplicationPacket *app)
{
	if (!IsPersonasEnabled()) {
		return;
	}

	if (app->size != sizeof(PersonaRequest_Struct)) {
		LogNetcode("Received invalid sized OP_PersonaSwitch: got [{}], expected [{}]", app->size, sizeof(PersonaRequest_Struct));
		return;
	}

	const auto request = reinterpret_cast<const PersonaRequest_Struct *>(app->pBuffer);

	if (request->target_class < Class::Warrior || request->target_class > Class::Berserker) {
		LogPersonas("Switch to class [{}] refused for [{}]: not a class", request->target_class, GetCleanName());
		RefusePersonaRequest(PERSONA_CREATE_INVALID_CLASS);
		return;
	}

	if (request->current_class != GetClass()) {
		LogPersonasDetail(
			"[{}] reports class [{}] but is class [{}]",
			GetCleanName(),
			request->current_class,
			GetClass()
		);
	}

	SwitchPersona(static_cast<uint8>(request->target_class), false, request->aa_purchasable != 0);
}

void Client::Handle_OP_EquipmentSetAction(const EQApplicationPacket *app)
{
	if (!IsPersonasEnabled()) {
		return;
	}

	if (app->size != sizeof(EquipmentSetAction_Struct)) {
		LogNetcode("Received invalid sized OP_EquipmentSetAction: got [{}], expected [{}]", app->size, sizeof(EquipmentSetAction_Struct));
		return;
	}

	const auto action = reinterpret_cast<const EquipmentSetAction_Struct *>(app->pBuffer);
	LogPersonasDetail(
		"Equipment set action [{}] for class [{}] from [{}]",
		action->action,
		action->class_id,
		GetCleanName()
	);

	// a set belongs to one class and only that class can save or load it
	if (action->class_id != GetBaseClass()) {
		MessageString(
			Chat::Red,
			EQUIPMENT_SET_WRONG_CLASS,
			GetClassIDName(static_cast<uint8>(action->class_id)),
			GetClassIDName(GetBaseClass())
		);
	}
	else if (action->action == Persona::EquipmentSetSave) {
		SavePersonaEquipmentSet();
	}
	else if (action->action == Persona::EquipmentSetLoad) {
		LoadPersonaEquipmentSet(true);
		CalcBonuses();
	}

	EQApplicationPacket finish(OP_FinishWindow, 0);
	QueuePacket(&finish);
}

// the client's own test: no aggro and no detrimental buff that blocks resting; unlike rest regen it does
// not need the player to sit or the rest timer to run out
bool Client::IsPersonaChangeBlockedByCombat()
{
	if (IsEngaged() || GetAggroCount() > 0) {
		return true;
	}

	const uint32 buff_count = GetMaxTotalSlots();
	for (uint32 slot = 0; slot < buff_count; slot++) {
		const int32 spell_id = buffs[slot].spellid;
		if (
			IsValidSpell(spell_id) &&
			IsDetrimentalSpell(spell_id) &&
			buffs[slot].ticsremaining > 0 &&
			!IsRestAllowedSpell(spell_id)
		) {
			return true;
		}
	}

	return false;
}

uint32 Client::GetPersonaSwitchRefusal(uint8 class_id, bool aa_purchasable)
{
	if (dead) {
		return PERSONA_SWITCH_DEAD;
	}

	if (IsPersonaChangeBlockedByCombat()) {
		return PERSONA_SWITCH_IN_COMBAT;
	}

	if (!m_inv.CursorEmpty()) {
		return PERSONA_SWITCH_HOLDING_ITEM;
	}

	if (RuleB(Character, PersonaSwitchRequiresSpentAA) && aa_purchasable && m_pp.aapoints > 0) {
		return PERSONA_SWITCH_UNSPENT_AA;
	}

	// states that hang on this entity or on the rows the mover is about to swap
	const char *busy = nullptr;
	if (IsZoning()) {
		busy = "zoning";
	}
	else if (IsTrader() || IsBuyer()) {
		busy = "trading in the bazaar";
	}
	else if (IsCasting()) {
		busy = "casting";
	}
	else if (GetTradeskillObject() || IsLooting()) {
		busy = "using a container or looting";
	}
	else if (camp_timer.Enabled()) {
		busy = "camping";
	}

	if (busy) {
		LogPersonasDetail("Switch for [{}] refused: {}", GetCleanName(), busy);
		return PERSONA_SWITCH_ZONE;
	}

	if (RuleB(Character, PersonaSwitchOnlyInFastCampZones) && !IsFastCampZone()) {
		LogPersonasDetail("Switch for [{}] refused: [{}] is not a fast camp zone", GetCleanName(), zone->GetShortName());
		return PERSONA_SWITCH_ZONE;
	}

	const uint8 target_level = GetPersonaLevel(class_id);
	if (!CanEnterZone("", -1, target_level)) {
		LogPersonasDetail("Switch for [{}] refused: level [{}] may not enter this zone", GetCleanName(), target_level);
		return PERSONA_SWITCH_ZONE;
	}

	if (RuleB(TaskSystem, EnableTaskSystem) && GetTaskState() && GetTaskState()->HasActiveSharedTask()) {
		const auto task = TaskManager::Instance()->GetTaskData(GetTaskState()->GetActiveSharedTask().task_id);
		if (
			task &&
			((task->min_level > 0 && target_level < task->min_level) ||
			 (task->max_level > 0 && target_level > task->max_level))
		) {
			return PERSONA_SWITCH_SHARED_TASK_LEVEL;
		}
	}

	if (parse->PlayerHasQuestSub(EVENT_PERSONA_SWITCH)) {
		if (parse->EventPlayer(EVENT_PERSONA_SWITCH, this, fmt::format("{} {}", GetClass(), class_id), 0) != 0) {
			LogPersonasDetail("Switch for [{}] refused by script", GetCleanName());
			return PERSONA_SWITCH_ZONE;
		}
	}

	return NO_STRING;
}

bool Client::CancelPersonaTrade(uint32 &with_id)
{
	with_id = 0;
	if (trade->state == TradeNone) {
		return false;
	}

	Mob *with = trade->With();
	if (with) {
		with_id = with->GetID();

		if (with->IsClient()) {
			EQApplicationPacket cancel(OP_CancelTrade, sizeof(CancelTrade_Struct));
			auto message    = reinterpret_cast<CancelTrade_Struct *>(cancel.pBuffer);
			message->fromid = with->GetID();
			with->CastToClient()->QueuePacket(&cancel);
		}
	}

	// puts the offered items and coin back, as a cancel from the client does
	FinishTrade(this);
	trade->Reset();

	return true;
}

void Client::UnequipPersonaGear(bool check_level)
{
	for (int16 slot_id = EQ::invslot::EQUIPMENT_BEGIN; slot_id <= EQ::invslot::EQUIPMENT_END; ++slot_id) {
		const EQ::ItemInstance *worn = m_inv[slot_id];
		if (!worn || !worn->GetItem()) {
			continue;
		}

		if (CanWearAsPersona(worn->GetItem(), check_level)) {
			continue;
		}

		auto inst = m_inv.PopItem(slot_id);
		if (!inst) {
			continue;
		}

		// the Equipment ring takes it when the player asked for that; the swap that follows sends the ring
		if (CanStoreInEquipmentKeyRing(inst)) {
			const int16 ring_slot_id = StoreInEquipmentKeyRing(*inst);
			if (ring_slot_id != INVALID_INDEX) {
				LogPersonasDetail("[{}] cannot wear [{}] as this persona, moving slot [{}] to the equipment key ring", GetCleanName(), inst->GetItem()->Name, slot_id);
				database.SaveInventory(character_id, nullptr, slot_id);
				safe_delete(inst);
				continue;
			}
		}

		const bool is_arrow = inst->GetItem()->ItemType == EQ::item::ItemTypeArrow;
		const int16 free_slot_id = m_inv.FindFreeSlot(inst->IsClassBag(), true, inst->GetItem()->Size, is_arrow);

		LogPersonasDetail("[{}] cannot wear [{}] as this persona, moving slot [{}] to [{}]", GetCleanName(), inst->GetItem()->Name, slot_id, free_slot_id);

		if (free_slot_id == INVALID_INDEX) {
			PushItemOnCursor(*inst, false);
		}
		else {
			PutItemInInventory(free_slot_id, *inst, false);
		}

		database.SaveInventory(character_id, nullptr, slot_id);
		safe_delete(inst);
	}
}

// the same three checks the inventory runs when the player equips something
bool Client::CanWearAsPersona(const EQ::ItemData *item, bool check_level)
{
	if (!item) {
		return false;
	}

	const bool class_ok = item->IsEquipable(GetBaseRace(), GetBaseClass());
	const bool deity_ok = !GetDeity() || !item->Deity || (Deity::GetBitmask(GetDeity()) & item->Deity);
	const bool level_ok = !check_level || !item->ReqLevel || GetLevel() >= item->ReqLevel;

	return class_ok && deity_ok && level_ok;
}

// worn, general, bag or Equipment ring slot holding the item with this serial
int16 Client::FindPersonaItemSlot(uint32 item_serial)
{
	for (int16 slot_id = EQ::invslot::EQUIPMENT_BEGIN; slot_id <= EQ::invslot::GENERAL_END; ++slot_id) {
		const EQ::ItemInstance *inst = m_inv[slot_id];
		if (!inst) {
			continue;
		}

		if (static_cast<uint32>(inst->GetSerialNumber()) == item_serial) {
			return slot_id;
		}

		if (slot_id < EQ::invslot::GENERAL_BEGIN || !inst->IsClassBag()) {
			continue;
		}

		for (uint8 bag_slot = EQ::invbag::SLOT_BEGIN; bag_slot < inst->GetItem()->BagSlots; ++bag_slot) {
			const EQ::ItemInstance *contents = inst->GetItem(bag_slot);
			if (contents && static_cast<uint32>(contents->GetSerialNumber()) == item_serial) {
				return EQ::InventoryProfile::CalcSlotId(slot_id, bag_slot);
			}
		}
	}

	for (const auto &[ring_slot_id, inst] : m_inv.GetEquipmentKeyRingItems()) {
		if (inst && static_cast<uint32>(inst->GetSerialNumber()) == item_serial) {
			return ring_slot_id;
		}
	}

	return INVALID_INDEX;
}

// Live keeps an equipment set per class; ours is saved whenever the class is left
void Client::SavePersonaEquipmentSet()
{
	std::vector<Persona::EquippedItem> items;
	for (int16 slot_id = EQ::invslot::EQUIPMENT_BEGIN; slot_id <= EQ::invslot::EQUIPMENT_END; ++slot_id) {
		const EQ::ItemInstance *worn = m_inv[slot_id];
		if (worn) {
			items.push_back({slot_id, static_cast<uint32>(worn->GetSerialNumber())});
		}
	}

	Persona::SaveEquipmentSet(database, CharacterID(), GetBaseClass(), items);
	MessageString(Chat::White, EQUIPMENT_SET_SAVED, GetClassIDName(GetBaseClass()));

	// stored items' labels follow the sets
	if (HasEquipmentKeyRing()) {
		SendEquipmentKeyRingState();
	}
}

// puts the gear this class last wore back on; during a switch the swap packet carries the whole inventory,
// otherwise each move is sent the way the client sends its own
void Client::LoadPersonaEquipmentSet(bool client_update)
{
	auto send_move = [&](int16 from_slot_id, int16 to_slot_id) {
		if (!client_update) {
			return;
		}

		EQApplicationPacket outapp(OP_MoveItem, sizeof(MoveItem_Struct));
		auto move             = reinterpret_cast<MoveItem_Struct *>(outapp.pBuffer);
		move->from_slot       = from_slot_id;
		move->to_slot         = to_slot_id;
		move->number_in_stack = 0;
		QueuePacket(&outapp);
	};

	const auto set = Persona::LoadEquipmentSet(database, CharacterID(), GetBaseClass());
	if (set.empty()) {
		return;
	}

	for (const auto &entry : set) {
		if (entry.slot_id < EQ::invslot::EQUIPMENT_BEGIN || entry.slot_id > EQ::invslot::EQUIPMENT_END) {
			continue;
		}

		const EQ::ItemInstance *worn = m_inv[entry.slot_id];
		if (worn && static_cast<uint32>(worn->GetSerialNumber()) == entry.item_serial) {
			continue;
		}

		const int16 from_slot_id = FindPersonaItemSlot(entry.item_serial);
		const EQ::ItemInstance *wanted = from_slot_id == INVALID_INDEX ? nullptr : m_inv.GetItem(from_slot_id);
		if (!wanted || !CanWearAsPersona(wanted->GetItem()) || !(wanted->GetItem()->Slots & (1 << entry.slot_id))) {
			continue;
		}

		const bool from_ring = EQ::invslot::IsEquipmentKeyRingSlot(from_slot_id);

		auto moving    = m_inv.PopItem(from_slot_id);
		auto displaced = m_inv.PopItem(entry.slot_id);

		// the client has no move between a ring and a slot, so a ring involved means it gets the end state of each slot
		const bool store_displaced = displaced && CanStoreInEquipmentKeyRing(displaced);
		const bool ring_update     = from_ring || store_displaced;
		if (ring_update && client_update) {
			DeleteItemInInventory(from_slot_id, 0, true, false);
		}

		PutItemInInventory(entry.slot_id, *moving, false);
		if (ring_update && client_update) {
			SendItemPacket(entry.slot_id, m_inv[entry.slot_id], ItemPacketTrade);
		}
		else {
			send_move(from_slot_id, entry.slot_id);
		}

		int16 ring_slot_id = INVALID_INDEX;
		if (store_displaced) {
			ring_slot_id = StoreInEquipmentKeyRing(*displaced);
			if (ring_slot_id != INVALID_INDEX && client_update && HasEquipmentKeyRing()) {
				SendItemPacket(ring_slot_id, m_inv[ring_slot_id], ItemPacketCharInventory);
			}
		}

		if (displaced && ring_slot_id == INVALID_INDEX) {
			// a bag or a big item cannot always go where the equipped one came from
			int16 back_slot_id = from_slot_id;
			if (from_slot_id > EQ::invslot::GENERAL_END) {
				const bool is_arrow = displaced->GetItem()->ItemType == EQ::item::ItemTypeArrow;
				back_slot_id = m_inv.FindFreeSlot(displaced->IsClassBag(), true, displaced->GetItem()->Size, is_arrow);
			}

			if (back_slot_id == INVALID_INDEX) {
				PushItemOnCursor(*displaced, ring_update && client_update);
				database.SaveInventory(CharacterID(), nullptr, from_slot_id);
				if (!ring_update) {
					send_move(from_slot_id, EQ::invslot::slotCursor);
				}
			}
			else {
				PutItemInInventory(back_slot_id, *displaced, ring_update && client_update);
				if (back_slot_id != from_slot_id) {
					database.SaveInventory(CharacterID(), nullptr, from_slot_id);
					if (!ring_update) {
						send_move(from_slot_id, back_slot_id);
					}
				}
			}
		}
		else if (ring_slot_id != from_slot_id) {
			database.SaveInventory(CharacterID(), nullptr, from_slot_id);
		}

		LogPersonasDetail("[{}] equipment set: slot [{}] from [{}], displaced to ring slot [{}]", GetCleanName(), entry.slot_id, from_slot_id, ring_slot_id);
		safe_delete(moving);
		safe_delete(displaced);
	}

	// the client asks for its set right after a switch, so the switch itself stays quiet
	if (client_update) {
		MessageString(Chat::White, EQUIPMENT_SET_LOADED, GetClassIDName(GetBaseClass()));
		if (HasEquipmentKeyRing()) {
			SendEquipmentKeyRingState();
		}
	}
}

void Client::ReloadPersonaState()
{
	const uint32 cid        = CharacterID();
	const uint32 last_login = m_pp.lastlogin;

	// the old persona's buffs were saved with the character; auras and songs end here
	BuffFadeAll();

	// the old persona's pet was saved with it, the new one is restored below
	Mob *pet = GetPet();
	if (pet) {
		pet->Depop(false);
		SetPetID(0);
	}
	memset(&m_petinfo, 0, sizeof(PetInfo));
	memset(&m_suspendedminion, 0, sizeof(PetInfo));

	// the loaders below only write where the new class has rows, so nothing of the old class may be left behind
	memset(&m_pp.disciplines, 0, sizeof(m_pp.disciplines));
	memset(m_pp.binds, 0, sizeof(m_pp.binds));
	memset(m_pp.spellSlotRefresh, 0, sizeof(m_pp.spellSlotRefresh));
	m_pp.abilitySlotRefresh = 0;
	memset(m_pp.aa_array, 0, sizeof(m_pp.aa_array));
	memset(m_pp.buffs, 0, sizeof(m_pp.buffs));
	memset(m_pp.mem_spells, 0, sizeof(m_pp.mem_spells));
	memset(m_pp.spell_book, 0, sizeof(m_pp.spell_book));
	ClearAAs();

	database.LoadCharacterBandolier(cid, &m_pp);
	database.LoadCharacterBindPoint(cid, &m_pp);
	database.LoadCharacterData(cid, &m_pp, &m_epp);
	database.LoadCharacterSkills(cid, &m_pp);
	database.LoadCharacterSpellBook(cid, &m_pp);
	database.LoadCharacterMemmedSpells(cid, &m_pp);

	// the stored login time is older than the one Save() just advanced; keep the played-time counter intact
	m_pp.lastlogin    = last_login;
	m_pp.zone_id      = zone->GetZoneID();
	m_pp.zoneInstance = zone->GetInstanceID();

	class_           = m_pp.class_;
	level            = m_pp.level;
	race             = m_pp.race;
	base_race        = m_pp.race;
	deity            = m_pp.deity;
	gender           = m_pp.gender;
	base_gender      = m_pp.gender;
	haircolor        = m_pp.haircolor;
	beardcolor       = m_pp.beardcolor;
	eyecolor1        = m_pp.eyecolor1;
	eyecolor2        = m_pp.eyecolor2;
	hairstyle        = m_pp.hairstyle;
	luclinface       = m_pp.face;
	beard            = m_pp.beard;
	drakkin_heritage = m_pp.drakkin_heritage;
	drakkin_tattoo   = m_pp.drakkin_tattoo;
	drakkin_details  = m_pp.drakkin_details;

	consume_food_timer.SetTimer(class_ == Class::Monk ? CONSUMPTION_MNK_TIMER : CONSUMPTION_TIMER);
	InitInnates();

	// bucket rows listed in Character:PersonaDataBuckets moved with the persona
	ClearDataBucketCache();
	LoadDataBucketsCache();

	if (!database.LoadAlternateAdvancement(this)) {
		LogError("Error loading AA points for [{}]", GetName());
	}

	LoadClassState();
	database.LoadCharacterDisciplines(this);

	UnequipPersonaGear();
	LoadPersonaEquipmentSet(false);
	CalcBonuses();

	if (m_pp.cur_hp == 0) {
		m_pp.cur_hp = GetMaxHP();
	}

	SetHP(m_pp.cur_hp);
	Mob::SetMana(m_pp.mana);
	SetEndurance(m_pp.endurance);
	m_pp.cur_hp    = static_cast<uint32>(GetHP());
	m_pp.mana      = static_cast<uint32>(GetMana());
	m_pp.endurance = static_cast<uint32>(GetEndurance());

	LoadClassTimers();

	if (RuleB(NPC, PetZoneWithOwner)) {
		database.LoadPetInfo(this);
		if (m_petinfo.SpellID > 1 && !GetPet() && m_petinfo.SpellID <= SPDAT_RECORDS) {
			MakePoweredPet(
				m_petinfo.SpellID,
				spells[m_petinfo.SpellID].teleport_zone,
				m_petinfo.petpower,
				m_petinfo.Name,
				m_petinfo.size
			);

			if (GetPet() && GetPet()->IsNPC()) {
				NPC *restored = GetPet()->CastToNPC();
				restored->RestorePetState(m_petinfo.Buffs, m_petinfo.Items);
				restored->CalcBonuses();
				restored->SetHP(m_petinfo.HP);
				restored->SetMana(m_petinfo.Mana);

				if (m_ClientVersionBit & EQ::versions::maskUFAndLater) {
					if (!ingame) {
						restored->SetTaunting(m_petinfo.taunting);
					}
				}
			}

			m_petinfo.SpellID = 0;
		}
	}

	if (!aabonuses.ZoneSuspendMinion && !spellbonuses.ZoneSuspendMinion && !itembonuses.ZoneSuspendMinion) {
		memset(&m_suspendedminion, 0, sizeof(PetInfo));
	}

	ReapplyLoadedBuffs();

	// every persona gets the points and rewards a character earns by level or account age
	AutoGrantAAPoints();
	GrantVeteranAAs();
	SaveAA();

	SetAttackTimer();
}

void Client::SendPersonaExpUpdate()
{
	const uint32 next_exp = GetEXPForLevel(GetLevel() + 1);
	const uint32 this_exp = GetEXPForLevel(GetLevel());

	// an invalid class or level yields equal or maximal values, which would divide by zero
	if (
		next_exp == this_exp ||
		next_exp == std::numeric_limits<uint32>::max() ||
		this_exp == std::numeric_limits<uint32>::max()
	) {
		return;
	}

	auto outapp = new EQApplicationPacket(OP_ExpUpdate, sizeof(ExpUpdate_Struct));
	auto update = reinterpret_cast<ExpUpdate_Struct *>(outapp->pBuffer);

	const float progress = static_cast<float>(m_pp.exp - this_exp) / static_cast<float>(next_exp - this_exp);
	update->exp          = static_cast<uint32>(EXP_BAR_SIZE * progress);

	QueuePacket(outapp);
	safe_delete(outapp);
}

void Client::SendPersonaSwitchStart(bool trade_cancelled, uint32 trade_with_id)
{
	// the client shows its wait window until the finish packet
	EQApplicationPacket wait(OP_ShroudTransformWait, 0);
	QueuePacket(&wait);

	EQApplicationPacket finish(OP_FinishWindow, 0);
	QueuePacket(&finish);

	if (trade_cancelled) {
		EQApplicationPacket cancel(OP_CancelTrade, sizeof(CancelTrade_Struct));
		auto message    = reinterpret_cast<CancelTrade_Struct *>(cancel.pBuffer);
		message->fromid = trade_with_id;
		QueuePacket(&cancel);
	}

	DoTributeUpdate();
	SendClearPlayerAA();
}

void Client::SendPersonaSwapPackets()
{
	// the inventory goes as one list, collected as the bulk inventory send collects it
	EQ::OutBuffer items;
	auto add_item = [&](int16 slot_id, const EQ::ItemInstance *inst) {
		if (inst) {
			inst->Serialize(items, slot_id);
		}
	};

	for (int16 slot_id = EQ::invslot::POSSESSIONS_BEGIN; slot_id <= EQ::invslot::POSSESSIONS_END; slot_id++) {
		add_item(slot_id, m_inv[slot_id]);
	}

	if (!RuleB(Inventory, LazyLoadBank)) {
		for (int16 slot_id = EQ::invslot::BANK_BEGIN; slot_id <= EQ::invslot::BANK_END; slot_id++) {
			add_item(slot_id, m_inv[slot_id]);
		}

		for (int16 slot_id = EQ::invslot::SHARED_BANK_BEGIN; slot_id <= EQ::invslot::SHARED_BANK_END; slot_id++) {
			add_item(slot_id, m_inv[slot_id]);
		}
	}

	if (IsKeyRingEnabled()) {
		for (const auto &[slot_id, inst] : m_inv.GetKeyRingItems()) {
			add_item(slot_id, inst);
		}
	}

	if (IsMercEquipmentEnabled()) {
		for (const auto &[slot_id, inst] : m_inv.GetMercEquipmentItems()) {
			add_item(slot_id, inst);
		}
	}

	const std::string item_list = items.str();

	// the self spawn is built as zone entry builds it, without the lift
	NewSpawn_Struct self_spawn;
	memset(&self_spawn, 0, sizeof(self_spawn));
	FillSpawnStruct(&self_spawn, CastToMob());
	self_spawn.spawn.curHp = 1;
	self_spawn.spawn.NPC   = 0;

	m_pp.entityid            = GetID();
	m_pp.tob_keyring_unlocks = GetKeyRingUnlockMask();

	auto outapp = new EQApplicationPacket(OP_PersonaSwap, sizeof(PersonaSwap_Struct) + item_list.size());
	memset(outapp->pBuffer, 0, outapp->size);

	auto swap        = reinterpret_cast<PersonaSwap_Struct *>(outapp->pBuffer);
	swap->spawn_id   = GetID();
	swap->item_count = static_cast<uint32>(item_list.size() / sizeof(EQ::InternalSerializedItem_Struct));
	swap->spawn      = self_spawn.spawn;
	memcpy(&swap->profile, &m_pp, sizeof(PlayerProfile_Struct));
	memcpy(outapp->pBuffer + sizeof(PersonaSwap_Struct), item_list.data(), item_list.size());

	outapp->priority = 6;
	QueuePacket(outapp);
	safe_delete(outapp);

	// the swap list leaves out the Equipment ring, and the client's inventory reset clears its capacity
	if (HasEquipmentKeyRing()) {
		SendEquipmentKeyRingState();
		SendEquipmentKeyRingItems();
		SendEquipmentKeyRingState();
	}

	// the swap carries only the first cursor item
	for (auto iter = m_inv.cursor_cbegin(); iter != m_inv.cursor_cend(); ++iter) {
		if (iter == m_inv.cursor_cbegin()) {
			continue;
		}

		SendItemPacket(EQ::invslot::slotCursor, *iter, ItemPacketLimbo);
	}

	SendPersonaExpUpdate();

	Mob *pet = GetPet();
	if (pet) {
		pet->SendWearChangeAndLighting(EQ::textures::LastTexture);
		ClientPatch::SendFullBuffRefresh(pet);
	}

	ClientPatch::SendFullBuffRefresh(this, false, true, true);

	SendAlternateAdvancementTable();
	SendAlternateAdvancementPoints();
	SendAlternateAdvancementStats();
	SendAlternateAdvancementTimers();

	SendDisciplineTimers();
	SendDisciplineUpdate();

	SendPersonaClassList();
	SendHPUpdate();
}

void Client::SendPersonaObserverUpdate()
{
	if (GetHideMe()) {
		return;
	}

	// everyone else sees the persona leave and a new one appear
	EQApplicationPacket despawn;
	CreateDespawnPacket(&despawn, false);
	entity_list.QueueClients(this, &despawn, true);

	EQApplicationPacket spawn;
	CreateSpawnPacket(&spawn);
	entity_list.QueueClients(this, &spawn, true);
}

void Client::UpdatePersonaSocialState()
{
	UpdateWho();

	Group *group = GetGroup();
	if (group) {
		group->SendUpdate(groupActUpdate, this);
	}

	if (IsRaidGrouped()) {
		Raid *raid = GetRaid();
		if (raid) {
			raid->UpdateLevel(GetName(), GetLevel());
			raid->UpdateClass(GetName(), GetClass());
			raid->SendRaidRemoveAll(GetName());
			raid->SendRaidAddAll(GetName());
		}
	}

	if (IsInAGuild()) {
		guild_mgr.SendToWorldMemberLevelUpdate(GuildID(), GetLevel(), std::string(GetCleanName()));
	}
}

bool Client::SwitchPersona(uint8 class_id, bool bypass_checks, bool aa_purchasable)
{
	if (!IsPersonasEnabled()) {
		return false;
	}

	if (class_id < Class::Warrior || class_id > Class::Berserker) {
		RefusePersonaRequest(PERSONA_CREATE_INVALID_CLASS);
		return false;
	}

	// the client only offers owned classes; anything else should have been a create request
	if (!HasPersona(class_id)) {
		LogPersonas("Switch to class [{}] refused for [{}]: not owned", class_id, GetCleanName());
		RefusePersonaRequest(PERSONA_CREATE_INVALID_CLASS);
		return false;
	}

	if (class_id == GetClass()) {
		RefusePersonaRequest(NO_STRING);
		return false;
	}

	if (bypass_checks) {
		LogPersonas("Switch to class [{}] for [{}] with the refusal checks bypassed", class_id, GetCleanName());
	}
	else {
		const uint32 refusal = GetPersonaSwitchRefusal(class_id, aa_purchasable);
		if (refusal != NO_STRING) {
			LogPersonas("Switch to class [{}] refused for [{}] with string [{}]", class_id, GetCleanName(), refusal);
			RefusePersonaRequest(refusal);
			return false;
		}
	}

	const uint8 old_class = GetClass();
	const uint8 old_level = GetLevel();

	LogPersonas(
		"Switching [{}] from class [{}] level [{}] to class [{}] level [{}]",
		GetCleanName(),
		old_class,
		old_level,
		class_id,
		GetPersonaLevel(class_id)
	);

	// state that cannot follow the character to another class
	uint32 trade_with_id = 0;
	const bool trade_cancelled = CancelPersonaTrade(trade_with_id);
	SetHorseId(0);

	if (GetMerc() && !GetMerc()->IsSuspended()) {
		GetMerc()->Suspend();
	}

	if (RuleB(Bots, Enabled)) {
		CampAllBots();
	}

	if (!Save(1)) {
		LogPersonas("Switch for [{}] stopped: the character could not be saved", GetCleanName());
		RefusePersonaRequest(PERSONA_SWITCH_ZONE);
		return false;
	}

	if (!Persona::Switch(database, CharacterID(), class_id)) {
		LogPersonas("Switch for [{}] to class [{}] failed in the database, character unchanged", GetCleanName(), class_id);
		RefusePersonaRequest(PERSONA_SWITCH_ZONE);
		return false;
	}

	SavePersonaEquipmentSet();

	SendPersonaSwitchStart(trade_cancelled, trade_with_id);
	ReloadPersonaState();
	Save(1);

	LoadPersonaClasses();
	SendPersonaSwapPackets();
	SendPersonaObserverUpdate();
	UpdatePersonaSocialState();

	if (parse->PlayerHasQuestSub(EVENT_PERSONA_SWITCHED)) {
		parse->EventPlayer(EVENT_PERSONA_SWITCHED, this, fmt::format("{} {}", old_class, GetClass()), 0);
	}

	return true;
}
