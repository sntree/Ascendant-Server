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
#include "merc.h"

#include "common/skill_caps.h"
#include "zone/client.h"
#include "zone/corpse.h"
#include "zone/entity.h"
#include "zone/groups.h"
#include "zone/mob.h"
#include "zone/quest_parser_collection.h"
#include "zone/string_ids.h"
#include "zone/zone.h"

extern volatile bool is_zone_loaded;

namespace {
	// HP percent at or below which a healer merc casts a heal, chosen by stance.
	struct MercHealProfile {
		int8 engaged_hp_percent;
		int8 idle_hp_percent;
		int8 group_heal_hp_percent;
		bool allow_heal_over_time;
		int8 mana_reserve_percent;
		bool allow_delayed_heals;
	};

	constexpr MercHealProfile LegacyHealProfile    = { 95, 99, 75, true, 0, true };
	constexpr MercHealProfile BalancedHealProfile  = { 80, 99, 70, true, 20, true };
	constexpr MercHealProfile EfficientHealProfile = { 70, 90, 60, false, 35, true };
	constexpr MercHealProfile ReactiveHealProfile  = { 90, 95, 70, false, 15, false };

	constexpr int8   IdleGroupHealHPPercent           = 95;
	constexpr int8   DelayedHealMinManaPercent        = 40;
	constexpr int8   EmergencyHealHPPercent           = 35;
	constexpr int8   VeryFastHealHPPercent            = 15;
	constexpr int8   RegularHealHPPercent             = 80;
	constexpr int8   HealOverTimeMaxHPPercent         = 95;
	constexpr int8   GroupHealWorstHPPercent          = 60;
	constexpr int8   PetHealHPOffset                  = 25;
	constexpr int8   RapidHPLossPercent               = 15;
	constexpr int8   BalancedCasterManaReservePercent = 15;
	constexpr int8   CasterFinishingHPPercent         = 20;
	constexpr int    MercRestRegenDelaySeconds        = 6;
	constexpr int    MercTicSeconds                   = 6;
	constexpr int8   RestBelowPercent                 = 90;
	constexpr uint8  NukesPerCheaperNuke              = 3;
	constexpr uint32 MinRetargetIntervalMs            = 4000;
	constexpr uint32 IdleSitDelayMs                   = 3000;
	constexpr uint32 ChatterRepeatMs                  = 15000;
	constexpr uint32 HealTargetBusyPaddingMs          = 500;
	constexpr uint32 StepBackRetryMs                  = 3000;
	constexpr uint32 StepBackMoveMs                   = 1500;
	constexpr float  AggressiveRangeMultiplier        = 1.5f;
	constexpr float  TargetLeashRadiusMultiplier      = 2.0f;
	constexpr float  CasterPositionRangeFraction      = 0.7f;
	constexpr float  CasterMinTargetDistance          = 30.0f;
	constexpr uint8  CasterPositioningMinLevel        = 12;
	constexpr float  FollowDistance                   = 15.0f;
	constexpr float  FollowRunDistance                = 40.0f;
	constexpr float  FollowTeleportDistance           = 450.0f;
	constexpr float  FollowTeleportHeight             = 100.0f;
	constexpr int8   EngagedBuffExtraManaPercent      = 20;
	constexpr int32  RebuffTicsRemaining              = 20;
	constexpr int32  RebuffMinDurationTics            = 40;

	// Lower number is healed first
	constexpr int HealPriorityEmergency = 0;
	constexpr int HealPriorityTank      = 1;
	constexpr int HealPriorityOwner     = 2;
	constexpr int HealPriorityOther     = 3;

	const MercHealProfile& GetMercHealProfile(uint8 stance)
	{
		if (!RuleB(Mercs, StanceBehavior)) {
			return LegacyHealProfile;
		}

		switch (stance) {
			case Stance::Efficient:
			case Stance::Efficient2:
				return EfficientHealProfile;
			case Stance::Reactive:
				return ReactiveHealProfile;
			default:
				return BalancedHealProfile;
		}
	}

	// One heal band closer to the fast heals, for a target that is losing health quickly
	float StepHealBandTowardFast(float hp_percent)
	{
		if (hp_percent >= RegularHealHPPercent) {
			return RegularHealHPPercent - 1;
		}

		if (hp_percent >= EmergencyHealHPPercent) {
			return EmergencyHealHPPercent - 1;
		}

		return VeryFastHealHPPercent - 1;
	}

	// Total counters on the buffs that need a cure, to cure the worst afflicted member first
	uint32 GetCureCounterTotal(Mob* tar)
	{
		uint32 total = 0;

		for (uint32 i = 0; i < tar->GetMaxTotalSlots(); i++) {
			const Buffs_Struct& buff = tar->GetBuffs()[i];

			if (IsValidSpell(buff.spellid) && CalculateCounters(buff.spellid) > 0) {
				total += buff.counters;
			}
		}

		return total;
	}

	// Mana percent a caster keeps for the end of a fight; Burn stances spend it all
	int8 GetCasterManaReservePercent(uint8 stance)
	{
		if (!RuleB(Mercs, StanceBehavior)) {
			return 0;
		}

		switch (stance) {
			case Stance::Burn:
			case Stance::AEBurn:
				return 0;
			default:
				return BalancedCasterManaReservePercent;
		}
	}

	// Efficient healers save their mana for healing
	bool IsEngagedBuffingAllowed(uint8 stance)
	{
		return RuleB(Mercs, StanceBehavior) && stance != Stance::Efficient && stance != Stance::Efficient2;
	}

	// A buff the target already has is worth casting again once it is close to fading; short buffs
	// are left alone or they would be recast all the time
	bool CanCastBuff(Mob* tar, int32 spell_id, uint8 caster_level, bool allow_refresh)
	{
		if (tar->CanBuffStack(spell_id, caster_level, true) >= 0) {
			return true;
		}

		if (!allow_refresh) {
			return false;
		}

		for (uint32 i = 0; i < tar->GetMaxTotalSlots(); i++) {
			const Buffs_Struct& buff = tar->GetBuffs()[i];

			if (buff.spellid == spell_id && buff.ticsremaining > 0 && buff.ticsremaining < RebuffTicsRemaining && buff.initialduration >= RebuffMinDurationTics) {
				return tar->CanBuffStack(spell_id, caster_level, false) >= 0;
			}
		}

		return false;
	}

	// Promised line: a buff that casts a heal on the target when it fades.
	bool IsDelayedHealSpell(int32 spell_id)
	{
		if (!IsValidSpell(spell_id)) {
			return false;
		}

		for (int i = 0; i < EFFECT_COUNT; i++) {
			if (spells[spell_id].effect_id[i] != SpellEffect::CastOnFadeEffect) {
				continue;
			}

			const int32 fade_spell_id = spells[spell_id].base_value[i];

			if (IsValidSpell(fade_spell_id) && IsBeneficialSpell(fade_spell_id) && IsEffectInSpell(fade_spell_id, SpellEffect::CurrentHP)) {
				return true;
			}
		}

		return false;
	}

	// Both instant hate and hate over time lines, but never hate reducers.
	bool IsHateGeneratingSpell(int32 spell_id)
	{
		if (!IsValidSpell(spell_id)) {
			return false;
		}

		for (int i = 0; i < EFFECT_COUNT; i++) {
			const int effect_id = spells[spell_id].effect_id[i];

			if ((effect_id == SpellEffect::InstantHate || effect_id == SpellEffect::Hate) && spells[spell_id].base_value[i] > 0) {
				return true;
			}
		}

		return false;
	}
}

#if EQDEBUG >= 12
	#define MercAI_DEBUG_Spells	25
#elif EQDEBUG >= 9
	#define MercAI_DEBUG_Spells	10
#else
	#define MercAI_DEBUG_Spells	-1
#endif

Merc::Merc(const NPCType* d, float x, float y, float z, float heading)
: NPC(d, nullptr, glm::vec4(x, y, z, heading), GravityBehavior::Water, false), endupkeep_timer(1000), rest_timer(1), confidence_timer(6000), check_target_timer(2000), pick_target_timer(2000), engaged_buff_timer(6000)
{
	base_hp = d->max_hp;
	base_mana = d->Mana;
	_baseAC = d->AC;
	_baseSTR = d->STR;
	_baseSTA = d->STA;
	_baseDEX = d->DEX;
	_baseAGI = d->AGI;
	_baseINT = d->INT;
	_baseWIS = d->WIS;
	_baseCHA = d->CHA;
	_baseATK = d->ATK;
	_baseRace = d->race;
	_baseGender = d->gender;
	_baseMR = d->MR;
	_baseCR = d->CR;
	_baseDR = d->DR;
	_baseFR = d->FR;
	_basePR = d->PR;
	_baseCorrup = d->Corrup;
	_OwnerClientVersion = static_cast<unsigned int>(EQ::versions::ClientVersion::Titanium);
	RestRegenHP = 0;
	RestRegenMana = 0;
	RestRegenEndurance = 0;
	cur_end = 0;

	_medding = false;
	_suspended = false;
	p_depop = false;
	_check_confidence = false;
	_lost_confidence = false;
	_hatedCount = 0;
	nuke_cast_count = 0;
	retarget_allowed_time = 0;
	follow_range_since = 0;
	melee_range_factor = 1.0f;
	melee_range_target_id = 0;

	memset(equipment, 0, sizeof(equipment));

	SetMercID(0);
	SetStance(Stance::Balanced);
	rest_timer.Disable();

	if (GetClass() == Class::Rogue)
		evade_timer.Start();

	int r;
	for (r = 0; r <= EQ::skills::HIGHEST_SKILL; r++) {
		skills[r] = SkillCaps::Instance()->GetSkillCap(GetClass(), (EQ::skills::SkillType)r, GetLevel()).cap;
	}

	size = d->size;
	CalcBonuses();

	// Class should use npc constructor to set light properties

	RestoreHealth();
	RestoreMana();
	RestoreEndurance();

	AI_Start();
}

Merc::~Merc() {
	AI_Stop();
	//entity_list.RemoveMerc(GetID());
	UninitializeBuffSlots();
}

void Merc::CalcBonuses()
{
	memset(&itembonuses, 0, sizeof(StatBonuses));
	memset(&aabonuses, 0, sizeof(StatBonuses));
	CalcItemBonuses(&itembonuses);
	AddMercEquipmentBonuses(&itembonuses);

	CalcSpellBonuses(&spellbonuses);

	CalcAC();
	CalcATK();

	CalcSTR();
	CalcSTA();
	CalcDEX();
	CalcAGI();
	CalcINT();
	CalcWIS();
	CalcCHA();

	CalcMR();
	CalcFR();
	CalcDR();
	CalcPR();
	CalcCR();
	CalcCorrup();

	CalcMaxHP();
	CalcMaxMana();
	CalcMaxEndurance();

	rooted = FindType(SpellEffect::Root);
}

float Merc::GetDefaultSize() {

	float MercSize = GetSize();

	switch(GetRace())
	{
		case 1: // Humans
			MercSize = 6.0;
			break;
		case 2: // Barbarian
			MercSize = 7.0;
			break;
		case 3: // Erudite
			MercSize = 6.0;
			break;
		case 4: // Wood Elf
			MercSize = 5.0;
			break;
		case 5: // High Elf
			MercSize = 6.0;
			break;
		case 6: // Dark Elf
			MercSize = 5.0;
			break;
		case 7: // Half Elf
			MercSize = 5.5;
			break;
		case 8: // Dwarf
			MercSize = 4.0;
			break;
		case 9: // Troll
			MercSize = 8.0;
			break;
		case 10: // Ogre
			MercSize = 9.0;
			break;
		case 11: // Halfling
			MercSize = 3.5;
			break;
		case 12: // Gnome
			MercSize = 3.0;
			break;
		case 128: // Iksar
			MercSize = 6.0;
			break;
		case 130: // Vah Shir
			MercSize = 7.0;
			break;
		case 330: // Froglok
			MercSize = 5.0;
			break;
		case 522: // Drakkin
			MercSize = 5.0;
			break;
		default:
			MercSize = 6.0;
			break;
	}

	return MercSize;
}

int Merc::GroupLeadershipAAHealthEnhancement()
{
	Group *g = GetGroup();

	if(!g || (g->GroupCount() < RuleI(Character, LeadershipMinGroupSize)))
		return 0;

	switch(g->GetLeadershipAA(groupAAHealthEnhancement))
	{
	case 0:
		return 0;
	case 1:
		return 30;
	case 2:
		return 60;
	case 3:
		return 100;
	}

	return 0;
}

int Merc::GroupLeadershipAAManaEnhancement()
{
	Group *g = GetGroup();

	if(!g || (g->GroupCount() < RuleI(Character, LeadershipMinGroupSize)))
		return 0;

	switch(g->GetLeadershipAA(groupAAManaEnhancement))
	{
	case 0:
		return 0;
	case 1:
		return 30;
	case 2:
		return 60;
	case 3:
		return 100;
	}

	return 0;
}

int Merc::GroupLeadershipAAHealthRegeneration()
{
	Group *g = GetGroup();

	if(!g || (g->GroupCount() < RuleI(Character, LeadershipMinGroupSize)))
		return 0;

	switch(g->GetLeadershipAA(groupAAHealthRegeneration))
	{
	case 0:
		return 0;
	case 1:
		return 4;
	case 2:
		return 6;
	case 3:
		return 8;
	}

	return 0;
}

int Merc::GroupLeadershipAAOffenseEnhancement()
{
	Group *g = GetGroup();

	if(!g || (g->GroupCount() < RuleI(Character, LeadershipMinGroupSize)))
		return 0;

	switch(g->GetLeadershipAA(groupAAOffenseEnhancement))
	{
	case 0:
		return 0;
	case 1:
		return 10;
	case 2:
		return 19;
	case 3:
		return 28;
	case 4:
		return 34;
	case 5:
		return 40;
	}
	return 0;
}

int32 Merc::CalcSTR() {
	int32 val = _baseSTR + itembonuses.STR + spellbonuses.STR;

	int32 mod = aabonuses.STR;

	STR = val + mod;

	if(STR < 1)
		STR = 1;

	return(STR);
}

int32 Merc::CalcSTA() {
	int32 val = _baseSTA + itembonuses.STA + spellbonuses.STA;

	int32 mod = aabonuses.STA;

	STA = val + mod;

	if(STA < 1)
		STA = 1;

	return(STA);
}

int32 Merc::CalcAGI() {
	int32 val = _baseAGI + itembonuses.AGI + spellbonuses.AGI;
	int32 mod = aabonuses.AGI;

	int32 str = GetSTR();

	AGI = val + mod;

	if(AGI < 1)
		AGI = 1;

	return(AGI);
}

int32 Merc::CalcDEX() {
	int32 val = _baseDEX + itembonuses.DEX + spellbonuses.DEX;

	int32 mod = aabonuses.DEX;

	DEX = val + mod;

	if(DEX < 1)
		DEX = 1;

	return(DEX);
}

int32 Merc::CalcINT() {
	int32 val = _baseINT + itembonuses.INT + spellbonuses.INT;

	int32 mod = aabonuses.INT;

	INT = val + mod;

	if(INT < 1)
		INT = 1;

	return(INT);
}

int32 Merc::CalcWIS() {
	int32 val = _baseWIS + itembonuses.WIS + spellbonuses.WIS;

	int32 mod = aabonuses.WIS;

	WIS = val + mod;

	if(WIS < 1)
		WIS = 1;

	return(WIS);
}

int32 Merc::CalcCHA() {
	int32 val = _baseCHA + itembonuses.CHA + spellbonuses.CHA;

	int32 mod = aabonuses.CHA;

	CHA = val + mod;

	if(CHA < 1)
		CHA = 1;

	return(CHA);
}

//The AA multipliers are set to be 5, but were 2 on WR
//The resistant discipline which I think should be here is implemented
//in Mob::ResistSpell
int32 Merc::CalcMR()
{
	MR = _baseMR + itembonuses.MR + spellbonuses.MR + aabonuses.MR;

	if(MR < 1)
		MR = 1;

	return(MR);
}

int32 Merc::CalcFR()
{
	FR = _baseFR + itembonuses.FR + spellbonuses.FR + aabonuses.FR;

	if(FR < 1)
		FR = 1;

	return(FR);
}

int32 Merc::CalcDR()
{
	DR = _baseDR + itembonuses.DR + spellbonuses.DR + aabonuses.DR;

	if(DR < 1)
		DR = 1;

	return(DR);
}

int32 Merc::CalcPR()
{
	PR = _basePR + itembonuses.PR + spellbonuses.PR + aabonuses.PR;

	if(PR < 1)
		PR = 1;

	return(PR);
}

int32 Merc::CalcCR()
{
	CR = _baseCR + itembonuses.CR + spellbonuses.CR + aabonuses.CR;

	if(CR < 1)
		CR = 1;

	return(CR);
}

int32 Merc::CalcCorrup()
{
	Corrup = _baseCorrup + itembonuses.Corrup + spellbonuses.Corrup + aabonuses.Corrup;

	return(Corrup);
}

int32 Merc::CalcATK() {
	ATK = _baseATK + itembonuses.ATK + spellbonuses.ATK + aabonuses.ATK + GroupLeadershipAAOffenseEnhancement();
	return(ATK);
}

int32 Merc::CalcAC() {
	//spell AC bonuses are added directly to natural total
	AC = _baseAC + itembonuses.AC + spellbonuses.AC;
	return(AC);
}

int64 Merc::CalcHPRegen() {
	int64 regen = hp_regen + itembonuses.HPRegen + spellbonuses.HPRegen;

	regen += aabonuses.HPRegen + GroupLeadershipAAHealthRegeneration();

	return (regen * RuleI(Character, HPRegenMultiplier) / 100);
}

int64 Merc::CalcHPRegenCap()
{
	int cap = RuleI(Character, ItemHealthRegenCap) + itembonuses.HeroicSTA/25;

	cap += aabonuses.ItemHPRegenCap + spellbonuses.ItemHPRegenCap + itembonuses.ItemHPRegenCap;

	return (cap * RuleI(Character, HPRegenMultiplier) / 100);
}

int64 Merc::CalcMaxHP() {
	float nd = 10000;
	max_hp = (CalcBaseHP() + itembonuses.HP);

	//The AA desc clearly says it only applies to base hp..
	//but the actual effect sent on live causes the client
	//to apply it to (basehp + itemhp).. I will oblige to the client's whims over
	//the aa description
	nd += aabonuses.PercentMaxHPChange + spellbonuses.PercentMaxHPChange + itembonuses.PercentMaxHPChange;  //Natural Durability, Physical Enhancement, Planar Durability

	max_hp = (float)max_hp * (float)nd / (float)10000; //this is to fix the HP-above-495k issue
	max_hp += spellbonuses.FlatMaxHPChange + aabonuses.FlatMaxHPChange + itembonuses.FlatMaxHPChange;

	max_hp += GroupLeadershipAAHealthEnhancement();

	if (current_hp > max_hp)
		current_hp = max_hp;

	int64 hp_perc_cap = spellbonuses.HPPercCap[SBIndex::RESOURCE_PERCENT_CAP];
	if(hp_perc_cap) {
		int64 curHP_cap = (max_hp * hp_perc_cap) / 100;
		if (current_hp > curHP_cap || (spellbonuses.HPPercCap[SBIndex::RESOURCE_AMOUNT_CAP] && current_hp > spellbonuses.HPPercCap[SBIndex::RESOURCE_AMOUNT_CAP]))
			current_hp = curHP_cap;
	}

	return max_hp;
}

int64 Merc::CalcBaseHP()
{
	return base_hp;
}

int64 Merc::CalcMaxMana()
{
	if (IsIntelligenceCasterClass() || IsWisdomCasterClass()) {
		max_mana = (CalcBaseMana() + itembonuses.Mana + spellbonuses.Mana + GroupLeadershipAAManaEnhancement());
	} else {
		max_mana = 0;
	}

	if (max_mana < 0) {
		max_mana = 0;
	}

	if (current_mana > max_mana) {
		current_mana = max_mana;
	}

	int mana_perc_cap = spellbonuses.ManaPercCap[SBIndex::RESOURCE_PERCENT_CAP];
	if(mana_perc_cap) {
		int curMana_cap = (max_mana * mana_perc_cap) / 100;
		if (current_mana > curMana_cap  || (spellbonuses.ManaPercCap[SBIndex::RESOURCE_AMOUNT_CAP] && current_mana > spellbonuses.ManaPercCap[SBIndex::RESOURCE_AMOUNT_CAP]))
			current_mana = curMana_cap;
	}

	return max_mana;
}

int64 Merc::CalcBaseMana()
{
	return base_mana;
}

int64 Merc::CalcBaseManaRegen()
{
	uint8 clevel = GetLevel();
	int32 regen = 0;
	if (IsSitting())
	{
		if (HasSkill(EQ::skills::SkillMeditate))
			regen = (((GetSkill(EQ::skills::SkillMeditate) / 10) + (clevel - (clevel / 4))) / 4) + 4;
		else
			regen = 2;
	}
	else {
		regen = 2;
	}
	return regen;
}

int64 Merc::CalcManaRegen()
{
	if (IsSitting())
	{
		BuffFadeBySitModifier();
		_medding = HasSkill(EQ::skills::SkillMeditate);
	}
	else {
		_medding = false;
	}

	return CalcManaRegenValue(IsSitting());
}

int64 Merc::CalcManaRegenValue(bool sitting) const
{
	int64 regen = mana_regen + spellbonuses.ManaRegen + itembonuses.ManaRegen;

	if (sitting && HasSkill(EQ::skills::SkillMeditate)) {
		regen += GetSkill(EQ::skills::SkillMeditate) / 10;
	}

	if (IsIntelligenceCasterClass()) {
		regen += (itembonuses.HeroicINT / 25);
	} else if (IsWisdomCasterClass()) {
		regen += (itembonuses.HeroicWIS / 25);
	} else {
		regen = 0;
	}

	//AAs
	regen += aabonuses.ManaRegen;

	return (regen * RuleI(Character, ManaRegenMultiplier) / 100 * RuleI(Mercs, ManaRegenPercent) / 100);
}

int64 Merc::CalcManaRegenCap()
{
	int64 cap = RuleI(Character, ItemManaRegenCap) + aabonuses.ItemManaRegenCap;

	if (IsIntelligenceCasterClass()) {
		cap += (itembonuses.HeroicINT / 25);
	} else if (IsWisdomCasterClass()) {
		cap += (itembonuses.HeroicWIS / 25);
	}

	return (cap * RuleI(Character, ManaRegenMultiplier) / 100);
}

void Merc::CalcMaxEndurance()
{
	max_end = CalcBaseEndurance() + spellbonuses.Endurance + itembonuses.Endurance + aabonuses.Endurance;

	if (max_end < 0) {
		max_end = 0;
	}

	if (cur_end > max_end) {
		cur_end = max_end;
	}

	int end_perc_cap = spellbonuses.EndPercCap[SBIndex::RESOURCE_PERCENT_CAP];
	if(end_perc_cap) {
		int curEnd_cap = (max_end * end_perc_cap) / 100;
		if (cur_end > curEnd_cap || (spellbonuses.EndPercCap[SBIndex::RESOURCE_AMOUNT_CAP] && cur_end > spellbonuses.EndPercCap[SBIndex::RESOURCE_AMOUNT_CAP]))
			cur_end = curEnd_cap;
	}
}

int64 Merc::CalcBaseEndurance()
{
	int64 base_end = 0;
	int64 base_endurance = 0;
	int32 ConvertedStats = 0;
	int32 sta_end = 0;
	int Stats = 0;

	if (GetClientVersion() >= static_cast<unsigned int>(EQ::versions::ClientVersion::SoD) && RuleB(Character, SoDClientUseSoDHPManaEnd)) {
		int HeroicStats = 0;

		Stats = ((GetSTR() + GetSTA() + GetDEX() + GetAGI()) / 4);
		HeroicStats = ((GetHeroicSTR() + GetHeroicSTA() + GetHeroicDEX() + GetHeroicAGI()) / 4);

		if (Stats > 100) {
			ConvertedStats = (((Stats - 100) * 5 / 2) + 100);
			if (Stats > 201) {
				ConvertedStats -= ((Stats - 201) * 5 / 4);
			}
		}
		else {
			ConvertedStats = Stats;
		}

		if (GetLevel() < 41) {
			sta_end = (GetLevel() * 75 * ConvertedStats / 1000);
			base_endurance = (GetLevel() * 15);
		}
		else if (GetLevel() < 81) {
			sta_end = ((3 * ConvertedStats) + ((GetLevel() - 40) * 15 * ConvertedStats / 100));
			base_endurance = (600 + ((GetLevel() - 40) * 30));
		}
		else {
			sta_end = (9 * ConvertedStats);
			base_endurance = (1800 + ((GetLevel() - 80) * 18));
		}
		base_end = (base_endurance + sta_end + (HeroicStats * 10));
	}
	else
	{
		Stats = GetSTR()+GetSTA()+GetDEX()+GetAGI();
		int LevelBase = GetLevel() * 15;

		int at_most_800 = Stats;
		if(at_most_800 > 800)
			at_most_800 = 800;

		int Bonus400to800 = 0;
		int HalfBonus400to800 = 0;
		int Bonus800plus = 0;
		int HalfBonus800plus = 0;

		int BonusUpto800 = int( at_most_800 / 4 ) ;
		if(Stats > 400) {
			Bonus400to800 = int( (at_most_800 - 400) / 4 );
			HalfBonus400to800 = int( std::max( ( at_most_800 - 400 ), 0 ) / 8 );

			if(Stats > 800) {
				Bonus800plus = int( (Stats - 800) / 8 ) * 2;
				HalfBonus800plus = int( (Stats - 800) / 16 );
			}
		}
		int bonus_sum = BonusUpto800 + Bonus400to800 + HalfBonus400to800 + Bonus800plus + HalfBonus800plus;

		base_end = LevelBase;

		//take all of the sums from above, then multiply by level*0.075
		base_end += ( bonus_sum * 3 * GetLevel() ) / 40;
	}
	return base_end;
}

int64 Merc::CalcEnduranceRegen() {
	int64 regen = int32(GetLevel() * 4 / 10) + 2;
	regen += aabonuses.EnduranceRegen + spellbonuses.EnduranceRegen + itembonuses.EnduranceRegen;

	return (regen * RuleI(Character, EnduranceRegenMultiplier) / 100 * RuleI(Mercs, EnduranceRegenPercent) / 100);
}

int64 Merc::CalcEnduranceRegenCap() {
	int64 cap = (RuleI(Character, ItemEnduranceRegenCap) + itembonuses.HeroicSTR/25 + itembonuses.HeroicDEX/25 + itembonuses.HeroicAGI/25 + itembonuses.HeroicSTA/25);

	return (cap * RuleI(Character, EnduranceRegenMultiplier) / 100);
}

void Merc::SetEndurance(int32 newEnd)
{
	/*Endurance can't be less than 0 or greater than max*/
	if(newEnd < 0)
		newEnd = 0;
	else if(newEnd > GetMaxEndurance()){
		newEnd = GetMaxEndurance();
	}

	cur_end = newEnd;
}

void Merc::DoEnduranceUpkeep() {

	if (!HasEndurUpkeep())
		return;

	int upkeep_sum = 0;
	int cost_redux = spellbonuses.EnduranceReduction + itembonuses.EnduranceReduction;

	bool has_effect = false;
	uint32 buffs_i;
	uint32 buff_count = GetMaxTotalSlots();
	for (buffs_i = 0; buffs_i < buff_count; buffs_i++) {
		if (IsValidSpell(buffs[buffs_i].spellid)) {
			int upkeep = spells[buffs[buffs_i].spellid].endurance_upkeep;
			if(upkeep > 0) {
				has_effect = true;
				if(cost_redux > 0) {
					if(upkeep <= cost_redux)
						continue;       //reduced to 0
					upkeep -= cost_redux;
				}
				if((upkeep+upkeep_sum) > GetEndurance()) {
					//they do not have enough to keep this one going.
					BuffFadeBySlot(buffs_i);
				} else {
					upkeep_sum += upkeep;
				}
			}
		}
	}

	if(upkeep_sum != 0)
		SetEndurance(GetEndurance() - upkeep_sum);

	if (!has_effect)
		SetEndurUpkeep(false);
}

void Merc::CalcRestState() {

	// This method calculates rest state HP and mana regeneration.
	// The bot must have been out of combat for RuleI(Character, RestRegenTimeToActivate) seconds,
	// must be sitting down, and must not have any detrimental spells affecting them.
	//
	if(!RuleB(Character, RestRegenEnabled))
		return;

	RestRegenHP = RestRegenMana = RestRegenEndurance = 0;

	// With a full-refill time set, a merc recovers standing too: it should be ready for the next pull, not need babysitting
	const int full_seconds = RuleI(Mercs, RestRegenFullSeconds);

	if(IsEngaged() || (full_seconds <= 0 && !IsSitting()))
		return;

	if(!rest_timer.Check(false))
		return;

	uint32 buff_count = GetMaxTotalSlots();
	for (unsigned int j = 0; j < buff_count; j++) {
		if(IsValidSpell(buffs[j].spellid)) {
			if(IsDetrimentalSpell(buffs[j].spellid) && (buffs[j].ticsremaining > 0))
				if(!IsRestAllowedSpell(buffs[j].spellid))
					return;
		}
	}

	if(full_seconds > 0) {
		const int64 ticks_to_full = std::max<int64>(1, full_seconds / MercTicSeconds);

		RestRegenHP        = GetMaxHP() / ticks_to_full;
		RestRegenMana      = GetMaxMana() / ticks_to_full;
		RestRegenEndurance = GetMaxEndurance() / ticks_to_full;
		return;
	}

	// mercs refill faster than players between pulls so the group isn't waiting on them
	const int rest_regen_percent = RuleI(Mercs, RestRegenPercent);

	RestRegenHP = 6 * (GetMaxHP() / zone->newzone_data.fast_regen_hp) * rest_regen_percent / 100;

	RestRegenMana = 6 * (GetMaxMana() / zone->newzone_data.fast_regen_mana) * rest_regen_percent / 100;

	RestRegenEndurance = 6 * (GetMaxEndurance() / zone->newzone_data.fast_regen_endurance) * rest_regen_percent / 100;
}

bool Merc::HasSkill(EQ::skills::SkillType skill_id) const {
	return ((GetSkill(skill_id) > 0) && CanHaveSkill(skill_id));
}

bool Merc::CanHaveSkill(EQ::skills::SkillType skill_id) const {
	return SkillCaps::Instance()->GetSkillCap(GetClass(), skill_id, RuleI(Character, MaxLevel)).cap > 0;
	//if you don't have it by max level, then odds are you never will?
}

uint16 Merc::MaxSkill(EQ::skills::SkillType skillid, uint16 class_, uint16 level) const {
	return SkillCaps::Instance()->GetSkillCap(class_, skillid, level).cap;
}

void Merc::FillSpawnStruct(NewSpawn_Struct* ns, Mob* ForWho) {
	if(ns) {
		Mob::FillSpawnStruct(ns, ForWho);

		ns->spawn.afk = 0;
		ns->spawn.lfg = 0;
		ns->spawn.anon = 0;
		ns->spawn.gm = 0;
		ns->spawn.guildID = 0xFFFFFFFF;         // 0xFFFFFFFF = NO GUILD, 0 = Unknown Guild
		ns->spawn.is_npc = 1;                           // 0=no, 1=yes
		ns->spawn.is_pet = 0;
		ns->spawn.guildrank = 0;
		ns->spawn.showhelm = 1;
		ns->spawn.flymode = 0;
		ns->spawn.NPC = 1;                                      // 0=player,1=npc,2=pc corpse,3=npc corpse
		ns->spawn.IsMercenary = 1;
		ns->spawn.show_name = true;

		UpdateActiveLight();
		ns->spawn.light = m_Light.Type[EQ::lightsource::LightActive];

		/*
		// Wear Slots are not setup for Mercs yet
		unsigned int i;
		for (i = 0; i < _MaterialCount; i++)
		{
			if (equipment[i] == 0)
			{
				continue;
			}
			const ItemData* item = database.GetItem(equipment[i]);
			if(item)
			{
				ns->spawn.equipment[i].material = item->Material;
				ns->spawn.equipment[i].elitematerial = item->EliteMaterial;
				ns->spawn.equipment[i].heroforgemodel = item->HerosForgeModel;
				if (armor_tint[i])
				{
					ns->spawn.colors[i].color = armor_tint[i];
				}
				else
				{
					ns->spawn.colors[i].color = item->Color;
				}
			}
		}
		*/
	}
}

bool Merc::Process()
{
	if(IsStunned() && stunned_timer.Check())
		Mob::UnStun();

	if (GetDepop())
	{
		SetMercCharacterID(0);
		SetOwnerID(0);
		return false;
	}

	if(!GetMercenaryOwner()) {
		//p_depop = true;               //this was causing a crash - removed merc from entity list, but not group
		//return false;                 //merc can live after client dies, not sure how long
	}

	if(IsSuspended())
	{
		return false;
	}

	if (HasGroup() && GetMercenaryOwner() && GetFollowID() == 0) {
		SetFollowID(GetMercenaryOwner()->GetID());
	}

	SpellProcess();

	if(tic_timer.Check())
	{
		//6 seconds, or whatever the rule is set to has passed, send this position to everyone to avoid ghosting
		if (!IsEngaged())
		{
			SentPositionPacket(0.0f, 0.0f, 0.0f, 0.0f, 0);
			if (!rest_timer.Enabled()) {
				const int rest_delay_seconds = RuleI(Mercs, RestRegenFullSeconds) > 0 ? MercRestRegenDelaySeconds : RuleI(Character, RestRegenTimeToActivate);
				rest_timer.Start(rest_delay_seconds * 1000);
			}
		}

		BuffProcess();

		CalcRestState();

		if(GetHP() < GetMaxHP())
			SetHP(GetHP() + CalcHPRegen() + RestRegenHP);

		if(GetMana() < GetMaxMana())
			SetMana(GetMana() + CalcManaRegen() + RestRegenMana);

		if(GetEndurance() < GetMaxEndurance())
			SetEndurance(GetEndurance() + CalcEnduranceRegen() + RestRegenEndurance);
	}

	if(confidence_timer.Check()) {
		_check_confidence = true;
	}

	if (send_hp_update_timer.Check()) {
		SendHPUpdate();
		SendStatTable(false);
	}

	if (endupkeep_timer.Check() && GetHP() > 0){
		DoEnduranceUpkeep();
	}

	if (IsStunned() || IsMezzed())
		return true;

	// Merc AI
	AI_Process();

	return true;
}

bool Merc::IsMercCasterCombatRange(Mob *target) {
	bool result = false;

	if(target) {
		float range = MercAISpellRange;

		range *= range;

		// half the max so the merc doesn't always stop at max range to allow combat movement
		range *= .5;

		float targetDistance = DistanceSquaredNoZ(m_Position, target->GetPosition());

		if(targetDistance > range)
			result = false;
		else
			result = true;
	}

	return result;
}

// The merc itself lost its owner's pace
bool Merc::IsMercBeyondLeash(Mob* owner) {
	const float leash_distance = RuleI(Mercs, LeashDistance);

	if(leash_distance <= 0) {
		return false;
	}

	return DistanceSquaredNoZ(m_Position, owner->GetPosition()) > leash_distance * leash_distance;
}

// A feared mob running off, or a long chase
bool Merc::IsTargetBeyondLeash(Mob* owner, Mob* target) {
	if(RuleI(Mercs, LeashDistance) <= 0) {
		return false;
	}

	// A healer stays with the puller's mob for a moment so it can heal whoever is bringing it in
	if(GetClass() == HEALER && target->GetID() == called_target_id && Timer::GetCurrentTime() < called_heal_until) {
		return false;
	}

	const float target_leash_distance = RuleI(Mercs, AggroRadius) * TargetLeashRadiusMultiplier;

	return DistanceSquaredNoZ(target->GetPosition(), owner->GetPosition()) > target_leash_distance * target_leash_distance;
}

// Removing a single hate entry bypasses Mob::RemoveFromHateList, so the end of combat has to be raised here
void Merc::EndCombatIfHateListEmpty() {
	if(hate_list.IsHateListEmpty()) {
		AI_Event_NoLongerEngaged();
	}
}

// The group member the mob is hitting, else the owner: where a caster stands relative to
Mob* Merc::GetCasterAnchor(Mob* target) {
	Mob* hit_member = target->GetHateTop();

	if(hit_member && hit_member != this && hit_member->GetGroup() == GetGroup()) {
		return hit_member;
	}

	Mob* owner = GetOwner();

	return owner ? owner : target;
}

bool Merc::IsInCasterPosition(Mob* target) {
	const float target_distance_squared = DistanceSquared(m_Position, target->GetPosition());

	// Never walk into the mob
	if(target_distance_squared <= CasterMinTargetDistance * CasterMinTargetDistance) {
		return true;
	}

	Mob* anchor = GetCasterAnchor(target);
	const float stay_distance = MercAISpellRange * CasterPositionRangeFraction;

	// A healer works from the group, wherever the mob is; waiting to get in range of the mob stopped it healing
	if(anchor != target && DistanceSquared(m_Position, anchor->GetPosition()) <= stay_distance * stay_distance
		&& (GetClass() == HEALER || target_distance_squared <= MercAISpellRange * MercAISpellRange)) {
		return true;
	}

	return anchor == target && target_distance_squared <= MercAISpellRange * MercAISpellRange;
}

void Merc::AI_Process() {
	if(!IsAIControlled())
		return;

	if(IsCasting())
		return;

	// A merc wont start its AI if not grouped
	if(!HasGroup()) {
		return;
	}

	Mob* MercOwner = GetOwner();

	if(GetAppearance() == eaDead)
	{
		if(!MercOwner)
		{
			Depop();
		}
		return;
	}

	// The merc needs an owner
	if(!MercOwner) {
		//SetTarget(0);
		//SetOwnerID(0);
		// TODO: Need to wait and try casting rez if merc is a healer with a dead owner
		return;
	}

	/*
	try {
		if(MercOwner->CastToClient()->IsDead()) {
			SetTarget(0);
			SetOwnerID(0);
			return;
		}
	}
	catch(...) {
		SetTarget(0);
		SetOwnerID(0);
		return;
	}
	*/

	if(check_target_timer.Check()) {
		CheckHateList();
	}

	if(IsEngaged())
	{
		follow_range_since = 0;

		if(IsHoldingBackOffense()) {
			// passive: drop the fight and go back to following the owner
			if(GetTarget()) {
				MercSay(MercChatter::Hold, "Holding.");
			}

			WipeHateList();
			SetTarget(nullptr);

			return;
		}

		if(rest_timer.Enabled())
			rest_timer.Disable();

		// A sitting merc takes automatic hits, and MercMeditate refuses to stand while engaged
		if(IsSitting()) {
			Stand();
		}

		if(IsRooted())
			SetTarget(hate_list.GetClosestEntOnHateList(this));
		else if(RuleB(Mercs, AssistOwnerTarget))
			PickTarget();
		else
			FindTarget();

		if(!GetTarget()) {
			// Mezzed or unattackable mobs are never picked, so they would keep the merc engaged forever
			std::vector<Mob*> unusable_mobs;

			for(const auto& entry : hate_list.GetHateList()) {
				Mob* hated_mob = entry->entity_on_hatelist;

				if(hated_mob && (hated_mob->IsMezzed() || !IsAttackAllowed(hated_mob))) {
					unusable_mobs.push_back(hated_mob);
				}
			}

			for(Mob* unusable_mob : unusable_mobs) {
				hate_list.RemoveEntFromHateList(unusable_mob);
			}

			if(hate_list.IsHateListEmpty()) {
				AI_Event_NoLongerEngaged();
			}

			return;
		}

		// Do not chase a feared or fleeing mob away from the owner
		if(IsMercBeyondLeash(MercOwner)) {
			MercSay(MercChatter::Leash, "Too far from you, coming back.");
			WipeHateList();
			SetTarget(nullptr);

			return;
		}

		if(IsTargetBeyondLeash(MercOwner, GetTarget())) {
			hate_list.RemoveEntFromHateList(GetTarget());
			EndCombatIfHateListEmpty();
			SetTarget(nullptr);

			return;
		}

		if(HasPet())
			GetPet()->SetTarget(GetTarget());

		if(!IsSitting())
			FaceTarget(GetTarget());

		if(DivineAura())
			return;

		// Counting hated mobs walks every NPC in the zone and only the confidence check uses it
		if(RuleB(Mercs, ConfidenceFlee)) {
			int hateCount = entity_list.GetHatedCount(this, nullptr, false);
			if(GetHatedCount() < hateCount) {
				SetHatedCount(hateCount);

				if(!CheckConfidence()) {
					if(!confidence_timer.Enabled()) {
						confidence_timer.Start(10000);
					}
				}
			}
		}

		//Check specific conditions for merc to lose confidence and flee (or regain confidence once fleeing)
		if(_check_confidence) {
			//not already running
			if(!_lost_confidence) {
				//and fail confidence check
				if(!CheckConfidence()) {
					_lost_confidence = true;

					//move to bottom of hate lists?
					//Iterate though hatelist
					// SetHate(other, hate, damage)

					if(RuleB(Combat, EnableFearPathing)) {
						CalculateNewFearpoint();
						if(currently_fleeing) {
							return;
						}
					}
					else {
						Stun(12000 - (6000 - tic_timer.GetRemainingTime()));
					}
				}
			}
			else { //are fleeing due to lost confidence
				if(CheckConfidence()) { //passed test - regain confidence
					_lost_confidence = false;
				}
			}

			//they are in flee mode
			if(_lost_confidence)
				return;
		}

		// A mezzed or charmed target is dropped on its own; the next pass picks another mob from the hate list
		if(GetTarget()->IsMezzed() || !IsAttackAllowed(GetTarget())) {
			if(GetTarget()->IsMezzed()) {
				MercSay(MercChatter::SkippedMez, fmt::format("{} is mesmerized.", GetTarget()->GetCleanName()));
			}

			hate_list.RemoveEntFromHateList(GetTarget());
			EndCombatIfHateListEmpty();
			SetTarget(nullptr);

			if(IsMoving()) {
				SetRunAnimSpeed(0);

				if(moved) {
					moved = false;
					StopNavigation();
				}
			}

			return;
		}
		else if (!CheckLosFN(GetTarget())) {
			auto Goal = GetTarget()->GetPosition();
			RunTo(Goal.x, Goal.y, Goal.z);

			return;
		}

		if (!(GetPlayerState() & static_cast<uint32>(PlayerState::Aggressive)))
			SendAddPlayerState(PlayerState::Aggressive);

		bool atCombatRange = false;

		float meleeDistance = GetMaxMeleeRangeToTarget(GetTarget());

		if(GetClass() == Class::ShadowKnight || GetClass() == Class::Paladin || GetClass() == Class::Warrior) {
			meleeDistance = meleeDistance * .30;
		}
		else {
			// Rolled once per target; a new roll every pass made the merc start and stop at the edge of range
			if(melee_range_target_id != GetTarget()->GetID()) {
				melee_range_target_id = GetTarget()->GetID();
				melee_range_factor    = static_cast<float>(zone->random.Real(.50, .85));
			}

			meleeDistance *= melee_range_factor;
		}
		const bool positions_as_caster = IsMercCaster() && GetLevel() > CasterPositioningMinLevel;

		if(positions_as_caster) {
			if(IsInCasterPosition(GetTarget()))
				atCombatRange = true;
		}
		else if(DistanceSquared(m_Position, GetTarget()->GetPosition()) <= meleeDistance) {
			atCombatRange = true;
		}

		if(atCombatRange)
		{
			if(IsMoving() && Timer::GetCurrentTime() >= step_back_until)
			{
				SetHeading(CalculateHeadingToTarget(GetTarget()->GetX(), GetTarget()->GetY()));
				SetRunAnimSpeed(0);

				if(moved) {
					StopNavigation();
				}
			}

			if(AI_movement_timer->Check()) {
				if (!IsMoving()) {
					if (positions_as_caster && StepBackFromTarget()) {
						return;
					}

					if (GetClass() == Class::Rogue) {
						if (HasTargetReflection() && !GetTarget()->IsFeared() && !GetTarget()->IsStunned()) {
							// Hate redux actions
							if (evade_timer.Check(false)) {
								// Attempt to evade
								int timer_duration = (HideReuseTime - GetSkillReuseTime(EQ::skills::SkillHide)) * 1000;
								if (timer_duration < 0)
									timer_duration = 0;
								evade_timer.Start(timer_duration);

								if (zone->random.Int(0, 260) < (int)GetSkill(EQ::skills::SkillHide))
									RogueEvade(GetTarget());

								return;
							}
							else if (GetTarget()->IsRooted()) {
								// Move rogue back from rooted mob - out of combat range, if necessary
								float melee_distance = GetMaxMeleeRangeToTarget(GetTarget());
								float current_distance = DistanceSquared(static_cast<glm::vec3>(m_Position), static_cast<glm::vec3>(GetTarget()->GetPosition()));

								if (current_distance <= melee_distance) {
									float newX = 0;
									float newY = 0;
									float newZ = 0;
									FaceTarget(GetTarget());
									if (PlotPositionAroundTarget(this, newX, newY, newZ)) {
										RunTo(newX, newY, newZ);
										return;
									}
								}
							}
						}
						else if (!BehindMob(GetTarget(), GetX(), GetY())) {
							// Move the rogue to behind the mob
							float newX = 0;
							float newY = 0;
							float newZ = 0;
							if (PlotPositionAroundTarget(GetTarget(), newX, newY, newZ)) {
								RunTo(newX, newY, newZ);
								return;
							}
						}
					}
					else if (GetClass() != Class::Rogue && (DistanceSquaredNoZ(m_Position, GetTarget()->GetPosition()) < GetTarget()->GetSize() * GetTarget()->GetSize())) {
						// If we are not a rogue trying to backstab, let's try to adjust our melee range so we don't appear to be bunched up
						float newX = 0;
						float newY = 0;
						float newZ = 0;
						if (PlotPositionAroundTarget(GetTarget(), newX, newY, newZ, false) && GetArchetype() != Archetype::Caster) {
							RunTo(newX, newY, newZ);
							return;
						}
					}
				}

				//if (IsMoving())
				//	SendPositionUpdate();
				//else
				//	SendPosition();
			}

			if(!IsMercCaster() && GetTarget() && !IsStunned() && !IsMezzed() && (GetAppearance() != eaDead))
			{
				// we can't fight if we don't have a target, are stun/mezzed or dead..
				// Stop attacking if the target is enraged
				if(IsEngaged() && !BehindMob(GetTarget(), GetX(), GetY()) && GetTarget()->IsEnraged())
					return;

				// First, special attack per class (kick, backstab etc..)
				DoClassAttacks(GetTarget());

				//try main hand first
				if(attack_timer.Check())
				{
					Attack(GetTarget(), EQ::invslot::slotPrimary);

					bool tripleSuccess = false;

					if(GetOwner() && GetTarget() && CanThisClassDoubleAttack())
					{
						if(GetOwner()) {
							Attack(GetTarget(), EQ::invslot::slotPrimary, true);
						}

						if(GetOwner() && GetTarget() && GetSpecialAbility(SpecialAbility::TripleAttack)) {
							tripleSuccess = true;
							Attack(GetTarget(), EQ::invslot::slotPrimary, true);
						}

						//quad attack, does this belong here??
						if(GetOwner() && GetTarget() && GetSpecialAbility(SpecialAbility::QuadrupleAttack)) {
							Attack(GetTarget(), EQ::invslot::slotPrimary, true);
						}
					}

					//Live AA - Flurry, Rapid Strikes ect (Flurry does not require Triple Attack).
					int16 flurrychance = aabonuses.FlurryChance + spellbonuses.FlurryChance + itembonuses.FlurryChance;

					if (GetTarget() && flurrychance)
					{
						if(zone->random.Roll(flurrychance))
						{
							MessageString(Chat::NPCFlurry, YOU_FLURRY);
							Attack(GetTarget(), EQ::invslot::slotPrimary, false);
							Attack(GetTarget(), EQ::invslot::slotPrimary, false);
						}
					}

					int16 ExtraAttackChanceBonus = spellbonuses.ExtraAttackChance[SBIndex::EXTRA_ATTACK_CHANCE] + itembonuses.ExtraAttackChance[SBIndex::EXTRA_ATTACK_CHANCE] + aabonuses.ExtraAttackChance[SBIndex::EXTRA_ATTACK_CHANCE];

					if (GetTarget() && ExtraAttackChanceBonus) {
						if(zone->random.Roll(ExtraAttackChanceBonus))
						{
							Attack(GetTarget(), EQ::invslot::slotPrimary, false);
						}
					}
				}

				// TODO: Do mercs berserk? Find this out on live...
				//if (GetClass() == Class::Warrior || GetClass() == Class::Berserker) {
				//      if(GetHP() > 0 && !berserk && GetHPRatio() < 30) {
				//              entity_list.MessageCloseString(this, false, 200, 0, BERSERK_START, GetName());
				//              berserk = true;
				//      }
				//      if (berserk && GetHPRatio() > 30) {
				//              entity_list.MessageCloseString(this, false, 200, 0, BERSERK_END, GetName());
				//              berserk = false;
				//      }
				//}

				//now off hand
				if(GetTarget() && attack_dw_timer.Check() && CanThisClassDualWield())
				{
					int weapontype = 0; // No weapon type
					bool bIsFist = true;

					// why are we checking 'weapontype' when we know it's set to '0' above?
					if (bIsFist || ((weapontype != EQ::item::ItemType2HSlash) && (weapontype != EQ::item::ItemType2HPiercing) && (weapontype != EQ::item::ItemType2HBlunt)))
					{
						float DualWieldProbability = 0.0f;

						int16 Ambidexterity = aabonuses.Ambidexterity + spellbonuses.Ambidexterity + itembonuses.Ambidexterity;
						DualWieldProbability = (GetSkill(EQ::skills::SkillDualWield) + GetLevel() + Ambidexterity) / 400.0f; // 78.0 max
						int16 DWBonus = spellbonuses.DualWieldChance + itembonuses.DualWieldChance;
						DualWieldProbability += DualWieldProbability*float(DWBonus)/ 100.0f;

						// Max 78% of DW
						if (zone->random.Roll(DualWieldProbability))
						{
							Attack(GetTarget(), EQ::invslot::slotSecondary);     // Single attack with offhand

							if(CanThisClassDoubleAttack()) {
								if(GetTarget() && GetTarget()->GetHP() > -10)
									Attack(GetTarget(), EQ::invslot::slotSecondary);     // Single attack with offhand
							}
						}
					}
				}
			}
		}
		else
		{
			if (AI_movement_timer->Check())
			{
				if(!IsRooted()) {
					LogAIDetail("Pursuing [{}] while engaged", GetTarget()->GetCleanName());

					// Casters move to the group, not to the mob
					Mob* destination = positions_as_caster ? GetCasterAnchor(GetTarget()) : GetTarget();

					RunTo(destination->GetX(), destination->GetY(), destination->GetZ());
					return;
				}

				//if(IsMoving())
				//	SendPositionUpdate();
				//else
				//	SentPositionPacket(0.0f, 0.0f, 0.0f, 0.0f, 0);
			}
		} // end not in combat range

		if(!IsMoving() && !spellend_timer.Enabled())
		{
			if(AI_EngagedCastCheck()) {
				MercMeditate(false);
			}
			else if(GetArchetype() == Archetype::Caster)
				MercMeditate(true);
		}
	}
	else
	{
		// Not engaged in combat
		chatter_next_time[static_cast<size_t>(MercChatter::LowMana)] = 0;
		SetTarget(0);
		SetHatedCount(0);
		confidence_timer.Disable();
		_check_confidence = false;

		if (GetPlayerState() & static_cast<uint32>(PlayerState::Aggressive))
			SendRemovePlayerState(PlayerState::Aggressive);

		if(!check_target_timer.Enabled())
			check_target_timer.Start(2000, false);

		if(!IsMoving() && AI_think_timer->Check() && !spellend_timer.Enabled())
		{
			if(!AI_IdleCastCheck() && !IsCasting() && CanRestNearOwner()) {
				// every archetype rests: melee and hybrid mercs need it to recover endurance
				MercMeditate(true);
			}
		}

		if(AI_movement_timer->Check()) {
			if(GetFollowID()) {
				Mob* follow = entity_list.GetMob(GetFollowID());

				if (follow) {
					const float dist = DistanceSquared(m_Position, follow->GetPosition());
					const float follow_distance = GetFollowDistance();

					SetRunAnimSpeed(0);

					if (dist > FollowTeleportDistance * FollowTeleportDistance || std::abs(follow->GetZ() - GetZ()) > FollowTeleportHeight) {
						// Pathing a long way or across floors can fail, so go straight to the owner like pets do
						follow_range_since = 0;
						Teleport(follow->GetPosition());
					}
					else if (dist > follow_distance * follow_distance) {
						if (dist < FollowRunDistance * FollowRunDistance) {
							WalkTo(follow->GetX(), follow->GetY(), follow->GetZ());
						}
						else {
							RunTo(follow->GetX(), follow->GetY(), follow->GetZ());
						}

						follow_range_since = 0;

						// the sitting-only player rate restarts its wait after a move; the merc full-refill rate keeps going
						if (rest_timer.Enabled() && RuleI(Mercs, RestRegenFullSeconds) <= 0)
							rest_timer.Disable();

						if (IsSitting()) {
							Stand();
						}
					}
					else {
						if (moved) {
							moved = false;
							StopNavigation();
						}
					}
				}
			}
		}
	}
}

void Merc::AI_Start(int32 iMoveDelay) {
	if (!pAIControlled)
		return;

	if (merc_spells.empty()) {
		AIautocastspell_timer->SetTimer(1000);
		AIautocastspell_timer->Disable();
	} else {
		AIautocastspell_timer->SetTimer(750);
		AIautocastspell_timer->Start(RandomTimer(0, 2000), false);
	}

	if (NPCTypedata_ours) {
		ProcessSpecialAbilities(NPCTypedata_ours->special_abilities);
	}

	SendTo(GetX(), GetY(), GetZ());
	SaveGuardSpot(GetPosition());
}

void Merc::AI_Stop() {
	NPC::AI_Stop();
	Mob::AI_Stop();
}

bool Merc::AI_EngagedCastCheck() {
	bool result = false;
	bool failedToCast = false;

	if (GetTarget() && AIautocastspell_timer->Check(false))
	{
		AIautocastspell_timer->Disable();       //prevent the timer from going off AGAIN while we are casting.

		LogAIDetail("Merc Engaged autocast check triggered");

		int8 mercClass = GetClass();

		switch(mercClass)
		{
		case TANK:
			if (!AICastSpell(GetChanceToCastBySpellType(SpellType_Nuke), SpellType_Nuke)) {
				if (!AICastSpell(GetChanceToCastBySpellType(SpellType_InCombatBuff), SpellType_InCombatBuff)) {
					failedToCast = true;
				}
			}
			break;
		case HEALER:
			if(!entity_list.Merc_AICheckCloseBeneficialSpells(this, GetChanceToCastBySpellType(SpellType_Heal), MercAISpellRange, SpellType_Heal)) {
				if(!entity_list.Merc_AICheckCloseBeneficialSpells(this, GetChanceToCastBySpellType(SpellType_Buff), MercAISpellRange, SpellType_Buff)) {
					failedToCast = true;
				}
			}
			break;
		case MELEEDPS:
			if (!AICastSpell(GetChanceToCastBySpellType(SpellType_Escape), SpellType_Escape)) {
				if (!AICastSpell(GetChanceToCastBySpellType(SpellType_Nuke), SpellType_Nuke)) {
					if (!AICastSpell(GetChanceToCastBySpellType(SpellType_InCombatBuff), SpellType_InCombatBuff)) {
						failedToCast = true;
					}
				}
			}
			break;
		case CASTERDPS:
			if (!AICastSpell(GetChanceToCastBySpellType(SpellType_Escape), SpellType_Escape)) {
				if (!AICastSpell(GetChanceToCastBySpellType(SpellType_Nuke), SpellType_Nuke)) {
					failedToCast = true;
				}
			}
			break;
		}

		if(!AIautocastspell_timer->Enabled()) {
			AIautocastspell_timer->Start(RandomTimer(100, 250), false);
		}

		if(!failedToCast)
			result = true;
	}

	return result;
}

bool Merc::AI_IdleCastCheck() {
	bool result = false;
	bool failedToCast = false;

	if (AIautocastspell_timer->Check(false)) {
#if MercAI_DEBUG_Spells >= 25
		LogAIDetail("Merc Non-Engaged autocast check triggered: [{}]", GetCleanName());
#endif
		AIautocastspell_timer->Disable();       //prevent the timer from going off AGAIN while we are casting.

		//Ok, IdleCastCheck depends of class.
		int8 mercClass = GetClass();

		switch(mercClass)
		{
		case TANK:
			failedToCast = true;
			break;
		case HEALER:
			if(!entity_list.Merc_AICheckCloseBeneficialSpells(this, 100, MercAISpellRange, SpellType_Cure)) {
				if(!entity_list.Merc_AICheckCloseBeneficialSpells(this, 100, MercAISpellRange, SpellType_Heal)) {
					if(!entity_list.Merc_AICheckCloseBeneficialSpells(this, 100, MercAISpellRange, SpellType_Resurrect)) {
						if(!entity_list.Merc_AICheckCloseBeneficialSpells(this, 100, MercAISpellRange, SpellType_Buff)) {
							failedToCast = true;
						}
					}
				}
			}
			result = true;
			break;
		case MELEEDPS:
			if(!entity_list.Merc_AICheckCloseBeneficialSpells(this, 100, MercAISpellRange, SpellType_Buff)) {
				failedToCast = true;
			}
			break;
		case CASTERDPS:
			failedToCast = true;
			break;
		}

		if(!AIautocastspell_timer->Enabled())
			AIautocastspell_timer->Start(RandomTimer(500, 1000), false);

		if(!failedToCast)
			result = true;
	}

	return result;
}

bool EntityList::Merc_AICheckCloseBeneficialSpells(Merc* caster, uint8 iChance, float iRange, uint32 iSpellTypes) {

	if ((iSpellTypes & SPELL_TYPES_DETRIMENTAL) != 0) {
		//according to live, you can buff and heal through walls...
		//now with PCs, this only applies if you can TARGET the target, but
		// according to Rogean, Live NPCs will just cast through walls/floors, no problem..
		//
		// This check was put in to address an idle-mob CPU issue
		LogError("Error: detrimental spells requested from AICheckCloseBeneficialSpells!!");
		return(false);
	}

	if(!caster)
		return false;

	if(!caster->AI_HasSpells())
		return false;

	if (iChance < 100) {
		int8 tmp = zone->random.Int(1, 100);
		if (tmp > iChance)
			return false;
	}

	int8 mercCasterClass = caster->GetClass();

	if(caster->HasGroup()) {
		if( mercCasterClass == HEALER) {
			if( iSpellTypes == SpellType_Heal )     {
				if(caster->AICastSpell(100, SpellType_Heal))
					return true;
			}

			if( iSpellTypes == SpellType_Cure )     {
				if(caster->AICastSpell(100, SpellType_Cure))
					return true;
			}

			if( iSpellTypes == SpellType_Resurrect )        {
				if(caster->AICastSpell(100, SpellType_Resurrect))
					return true;
			}
		}

		//Ok for the buffs..
		if( iSpellTypes == SpellType_Buff) {
			if(caster->AICastSpell(100, SpellType_Buff))
				return true;
		}
	}

	return false;
}

bool Merc::AIDoSpellCast(int32 spellid, Mob* tar, int32 mana_cost, uint32* oDontDoAgainBefore) {
	bool result = false;
	MercSpell mercSpell = GetMercSpellBySpellID(this, spellid);

	// manacost has special values, -1 is no mana cost, -2 is instant cast (no mana)
	int32 manaCost = mana_cost;

	if (manaCost == -1)
		manaCost = spells[spellid].mana;
	else if (manaCost == -2)
		manaCost = 0;

	// Out-of-combat buffs are discounted; combat buffs, heals and nukes keep the full cost
	if (mana_cost == -1 && !IsEngaged() && (mercSpell.type & SpellType_Buff) && IsBeneficialSpell(spellid) && !IsAnyHealSpell(spellid)) {
		manaCost  = manaCost * RuleI(Mercs, BuffManaCostPercent) / 100;
		mana_cost = manaCost;
	}

	int32 extraMana = 0;
	int32 hasMana = GetMana();

	float dist2 = 0;

	if (mercSpell.type & SpellType_Escape) {
		dist2 = 0;
	} else
		dist2 = DistanceSquared(m_Position, tar->GetPosition());

	if (((((spells[spellid].target_type==ST_GroupTeleport && mercSpell.type==SpellType_Heal)
		|| spells[spellid].target_type==ST_AECaster
		|| spells[spellid].target_type==ST_Group
		|| spells[spellid].target_type==ST_AEBard)
		&& dist2 <= spells[spellid].aoe_range*spells[spellid].aoe_range)
		|| dist2 <= GetActSpellRange(spellid, spells[spellid].range)*GetActSpellRange(spellid, spells[spellid].range)) && (manaCost <= GetMana() || GetMana() == GetMaxMana()))
	{
		SetRunAnimSpeed(0);
		SentPositionPacket(0.0f, 0.0f, 0.0f, 0.0f, 0);
		SetMoving(false);

		int32 cast_time = -1;
		const int32 base_cast_time = spells[spellid].cast_time;

		if (base_cast_time > 0 && RuleI(Mercs, CastTimePercent) != 100) {
			cast_time = GetActSpellCasttime(spellid, base_cast_time) * RuleI(Mercs, CastTimePercent) / 100;
		}

		result = CastSpell(spellid, tar->GetID(), EQ::spells::CastingSlot::Gem2, cast_time, mana_cost, oDontDoAgainBefore, -1, -1, 0, 0);

		if(IsCasting() && IsSitting())
			Stand();
	}

	// if the spell wasn't casted, then take back any extra mana that was given to the bot to cast that spell
	if(!result) {
		SetMana(hasMana);
		extraMana = false;
	}
	else { //handle spell recast and recast timers
		SetSpellTimeCanCast(mercSpell.spellid, spells[spellid].recast_time);

		if(spells[spellid].timer_id > 0) {
			SetSpellRecastTimer(spells[spellid].timer_id, spellid, spells[spellid].recast_time);
		}
	}

	return result;
}

bool Merc::AICastSpell(int8 iChance, uint32 iSpellTypes) {

	if(!AI_HasSpells())
		return false;

	// Passive healers do nothing, or with the rule only heal in an emergency
	if(IsPassiveHealer() && (!RuleB(Mercs, PassiveHealerEmergencyHeals) || iSpellTypes == SpellType_Buff || iSpellTypes == SpellType_Resurrect)) {
		return false;
	}

	if (iChance < 100) {
		if (zone->random.Int(0, 100) > iChance){
			return false;
		}
	}

	int8 mercClass = GetClass();
	uint8 mercLevel = GetLevel();

	bool checked_los = false;       //we do not check LOS until we are absolutely sure we need to, and we only do it once.
	bool castedSpell = false;
	bool isDiscipline = false;

	if(HasGroup()) {
		Group *g = GetGroup();

		if(g) {
			MercSpell selectedMercSpell;
			selectedMercSpell.spellid = 0;
			selectedMercSpell.stance = 0;
			selectedMercSpell.type = 0;
			selectedMercSpell.slot = 0;
			selectedMercSpell.proc_chance = 0;
			selectedMercSpell.time_cancast = 0;

			switch(mercClass)
			{
			case TANK:
			case MELEEDPS:
				isDiscipline = true;
				break;
			default:
				isDiscipline = false;
				break;
			}

			switch (iSpellTypes) {
							case SpellType_Heal: {
								Mob* tar = nullptr;
								const MercHealProfile& heal_profile = GetMercHealProfile(GetStance());
								const bool passive_healer = IsPassiveHealer();
								const int8 checkHPR = passive_healer ? EmergencyHealHPPercent : (IsEngaged() ? heal_profile.engaged_hp_percent : std::min<int8>(heal_profile.idle_hp_percent, RuleI(Mercs, HealerIdleHealPercent)));
								const int8 groupHealHPR = IsEngaged() ? heal_profile.group_heal_hp_percent : std::min<int8>(IdleGroupHealHPPercent, checkHPR);
								int8 numToHeal = g->GetNumberNeedingHealedInGroup(groupHealHPR, false);
								const int owner_heal_offset = passive_healer ? 0 : GetOwnerSettings().heal_offset;

								// A passive healer only heals in an emergency, so it has no group heals
								if(passive_healer) {
									numToHeal = 0;
								}

								// Whoever the mobs are hitting is a tank for this fight, whatever the group roles say
								Mob* const hate_top = GetTarget() ? GetTarget()->GetHateTop() : nullptr;
								std::vector<Mob*> hit_members;

								for(const auto& entry : hate_list.GetHateList()) {
									Mob* const hated_mob = entry->entity_on_hatelist;
									Mob* const hit_member = hated_mob && hated_mob->IsNPC() ? hated_mob->GetHateTop() : nullptr;

									if(hit_member && hit_member->GetGroup() == g && std::find(hit_members.begin(), hit_members.end(), hit_member) == hit_members.end()) {
										hit_members.push_back(hit_member);
									}
								}
								int tar_priority = HealPriorityOther;
								float tar_hp_loss = 0.0f;
								float lowest_member_hpr = 100.0f;

								for(int i = 0; i < MAX_GROUP_MEMBERS; i++) {
									Mob* member = g->members[i];

									if(!member || member->qglobal) {
										last_heal_check_hpr[i] = 0.0f;
										continue;
									}

									const float hpr = member->GetHPRatio();
									const float hp_loss = last_heal_check_hpr[i] - hpr;

									last_heal_check_hpr[i] = hpr;
									lowest_member_hpr = std::min(lowest_member_hpr, hpr);

									if(hpr >= std::min(checkHPR + (member == GetOwner() ? owner_heal_offset : 0), 100)) {
										continue;
									}

									// Another healer's heal is on its way to this member; only an emergency gets a second one
									if(member->DontHealMeBefore() > Timer::GetCurrentTime() && hpr >= EmergencyHealHPPercent) {
										continue;
									}

									if(IsEngaged() && ((member->GetClass() == Class::Necromancer && hpr >= 50)
										|| (member->GetClass() == Class::Shaman && hpr >= 80))) {
											//allow necros to lifetap & shaman to canni without wasting mana
											continue;
									}

									int priority = HealPriorityOther;

									if(hpr < EmergencyHealHPPercent) {
										priority = HealPriorityEmergency;
									}
									else if(std::find(hit_members.begin(), hit_members.end(), member) != hit_members.end() || g->HasRole(member, RoleTank)) {
										priority = HealPriorityTank;
									}
									else if(member == GetMercenaryOwner()) {
										priority = HealPriorityOwner;
									}

									if(!tar || priority < tar_priority || (priority == tar_priority && hpr < tar->GetHPRatio())) {
										tar = member;
										tar_priority = priority;
										tar_hp_loss = member == hate_top ? hp_loss : 0.0f;
									}
								}

								// Pets only get a heal when no group member needs one, and only when well down
								if(!tar) {
									const float pet_heal_hpr = checkHPR - PetHealHPOffset;

									for(int i = 0; i < MAX_GROUP_MEMBERS; i++) {
										Mob* member = g->members[i];
										Mob* pet = member && !member->qglobal ? member->GetPet() : nullptr;

										if(pet && pet->GetHPRatio() < pet_heal_hpr && (!tar || pet->GetHPRatio() < tar->GetHPRatio())) {
											tar = pet;
										}
									}
								}

								// Below the mana reserve only an emergency gets a heal
								if(GetManaRatio() < heal_profile.mana_reserve_percent) {
									numToHeal = 0;

									if(tar && tar->GetHPRatio() >= EmergencyHealHPPercent) {
										tar = nullptr;
										MercSay(MercChatter::LowMana, fmt::format("Low on mana ({}%).", static_cast<int>(GetManaRatio())));
									}
								}

								// A group heal needs three hurt members with one well down; a member in an emergency gets a single heal first
								if(numToHeal > 2 && lowest_member_hpr < GroupHealWorstHPPercent && lowest_member_hpr >= EmergencyHealHPPercent) {
									selectedMercSpell = GetBestMercSpellForGroupHeal(this);
								}

								if(tar && selectedMercSpell.spellid == 0) {
									// A target losing health fast gets the next faster heal
									float band_hpr = tar->GetHPRatio();

									if(tar_hp_loss > RapidHPLossPercent) {
										band_hpr = StepHealBandTowardFast(band_hpr);
									}

									if(band_hpr < VeryFastHealHPPercent) {
										//check for very fast heals first (casting time < 1 s)
										selectedMercSpell = GetBestMercSpellForVeryFastHeal(this);

										//check for fast heals next (casting time < 2 s)
										if(selectedMercSpell.spellid == 0) {
											selectedMercSpell = GetBestMercSpellForFastHeal(this);
										}

										//get regular heal
										if(selectedMercSpell.spellid == 0) {
											selectedMercSpell = GetBestMercSpellForRegularSingleTargetHeal(this);
										}
									}
									else if (band_hpr < EmergencyHealHPPercent) {
										//check for fast heals next (casting time < 2 s)
										selectedMercSpell = GetBestMercSpellForFastHeal(this);

										//get regular heal
										if(selectedMercSpell.spellid == 0) {
											selectedMercSpell = GetBestMercSpellForRegularSingleTargetHeal(this);
										}
									}
									else if (band_hpr < RegularHealHPPercent) {
										selectedMercSpell = GetBestMercSpellForPercentageHeal(this);

										//get regular heal
										if(selectedMercSpell.spellid == 0) {
											selectedMercSpell = GetBestMercSpellForRegularSingleTargetHeal(this);
										}
									}
									else {
										//check for heal over time. if not present, try it first; in a fight only for the one being hit
										if (heal_profile.allow_heal_over_time && (!IsEngaged() || tar == hate_top) && !tar->FindType(SpellEffect::HealOverTime)) {
											selectedMercSpell = GetBestMercSpellForHealOverTime(this);

											//get regular heal
											if (selectedMercSpell.spellid == 0) {
												selectedMercSpell = GetBestMercSpellForRegularSingleTargetHeal(this);
											}
										}
										else if (!heal_profile.allow_heal_over_time) {
											// stances without heal over time (Efficient, Reactive) heal this band directly
											selectedMercSpell = GetBestMercSpellForRegularSingleTargetHeal(this);
										}
									}
								}

								// The one being hit can be above the heal threshold and still benefit from a heal over time
								if(selectedMercSpell.spellid == 0 && IsEngaged() && !passive_healer && RuleB(Mercs, StanceBehavior) && heal_profile.allow_heal_over_time
									&& hate_top && hate_top->GetGroup() == g && GetManaRatio() >= heal_profile.mana_reserve_percent
									&& hate_top->DontHealMeBefore() <= Timer::GetCurrentTime()
									&& hate_top->GetHPRatio() < HealOverTimeMaxHPPercent && !hate_top->FindType(SpellEffect::HealOverTime)) {
									const MercSpell heal_over_time = GetBestMercSpellForHealOverTime(this);

									if(heal_over_time.spellid > 0) {
										tar = hate_top;
										selectedMercSpell = heal_over_time;
									}
								}

								if(selectedMercSpell.spellid == 0 && IsEngaged() && !passive_healer && RuleB(Mercs, UseDelayedHeals) && heal_profile.allow_delayed_heals && GetManaRatio() >= DelayedHealMinManaPercent) {
									// pre-empt damage on whoever the mob is hitting, falling back to the group tank
									Mob* delayed_heal_target = GetTarget() ? GetTarget()->GetHateTop() : nullptr;

									if(!delayed_heal_target || delayed_heal_target->GetGroup() != g) {
										delayed_heal_target = nullptr;

										for(int i = 0; i < MAX_GROUP_MEMBERS; i++) {
											if(g->members[i] && g->HasRole(g->members[i], RoleTank)) {
												delayed_heal_target = g->members[i];
											}
										}
									}

									if(delayed_heal_target) {
										const MercSpell delayed_heal = GetBestMercSpellForDelayedHeal(this);

										if(delayed_heal.spellid > 0 && delayed_heal_target->DontHealMeBefore() <= Timer::GetCurrentTime()
											&& delayed_heal_target->CanBuffStack(delayed_heal.spellid, mercLevel, true) >= 0) {
											tar = delayed_heal_target;
											selectedMercSpell = delayed_heal;
										}
									}
								}

								if(tar && selectedMercSpell.spellid > 0) {
									castedSpell = AIDoSpellCast(selectedMercSpell.spellid, tar, -1);
								}

								if(castedSpell) {
									const uint32 busy_until = Timer::GetCurrentTime() + spells[selectedMercSpell.spellid].cast_time + HealTargetBusyPaddingMs;

									if(IsGroupSpell(selectedMercSpell.spellid)) {
										for(int i = 0; i < MAX_GROUP_MEMBERS; i++) {
											if(g->members[i]) {
												g->members[i]->SetDontHealMeBefore(busy_until);
											}
										}
									}
									else {
										tar->SetDontHealMeBefore(busy_until);
									}
								}

								if(castedSpell) {
									if(tar && tar != this) { // [tar] was implicitly valid at this point..this change is to catch any bad logic
										//we don't need spam of bots healing themselves
										MercSay(MercChatter::Heal, fmt::format("Casting {} on {}.", spells[selectedMercSpell.spellid].name, tar->GetCleanName()), true);
									}
								}

								break;
												 }
							case SpellType_Root: {
								break;
												 }
							case SpellType_Buff: {

								if(GetClass() == HEALER && GetManaRatio() < 50) {
									return false;                   //mercs buff when Mana > 50%
								}

								const bool engaged = IsEngaged();
								const bool allow_rebuff = !engaged && !isDiscipline;
								Mob* engaged_buff_target = nullptr;

								// In a fight the only buff worth the mana is a missing one on whoever the mob is hitting
								if(engaged) {
									engaged_buff_target = GetTarget() ? GetTarget()->GetHateTop() : nullptr;

									if(!engaged_buff_target || GetManaRatio() < GetMercHealProfile(GetStance()).mana_reserve_percent + EngagedBuffExtraManaPercent || !engaged_buff_timer.Check()) {
										return false;
									}
								}

								std::list<MercSpell> buffSpellList = GetMercSpellsBySpellType(this, SpellType_Buff);

								for (auto itr = buffSpellList.begin();
								     itr != buffSpellList.end(); ++itr) {
									MercSpell selectedMercSpell = *itr;

									if (castedSpell) {
										break;
									}

									if(!((spells[selectedMercSpell.spellid].target_type == ST_Target || spells[selectedMercSpell.spellid].target_type == ST_Pet ||
										spells[selectedMercSpell.spellid].target_type == ST_Group || spells[selectedMercSpell.spellid].target_type == ST_GroupTeleport ||
										spells[selectedMercSpell.spellid].target_type == ST_Self))) {
											continue;
									}

									if(engaged && spells[selectedMercSpell.spellid].target_type != ST_Target) {
										continue;
									}

									if(spells[selectedMercSpell.spellid].target_type == ST_Self) {
										if( !IsImmuneToSpell(selectedMercSpell.spellid, this)
											&& CanCastBuff(this, selectedMercSpell.spellid, mercLevel, allow_rebuff)) {

												if( GetArchetype() == Archetype::Melee && IsEffectInSpell(selectedMercSpell.spellid, SpellEffect::IncreaseSpellHaste)) {
													continue;
												}

												uint32 TempDontBuffMeBeforeTime = DontBuffMeBefore();

												if(selectedMercSpell.spellid > 0) {
													if(isDiscipline) {
														castedSpell = UseDiscipline(selectedMercSpell.spellid, GetID());
													}
													else {
														castedSpell = AIDoSpellCast(selectedMercSpell.spellid, this, -1, &TempDontBuffMeBeforeTime);

														if(TempDontBuffMeBeforeTime != DontBuffMeBefore())
															SetDontBuffMeBefore(TempDontBuffMeBeforeTime);
													}
												}
										}
									}
									else {
										for( int i = 0; i < MAX_GROUP_MEMBERS; i++) {
											if (castedSpell) {
												break;
											}

											if(g->members[i]) {
												Mob* tar = g->members[i];

												if(engaged && tar != engaged_buff_target) {
													continue;
												}

												if( !tar->IsImmuneToSpell(selectedMercSpell.spellid, this)
													&& CanCastBuff(tar, selectedMercSpell.spellid, mercLevel, allow_rebuff)) {

														if( tar->GetArchetype() == Archetype::Melee && IsEffectInSpell(selectedMercSpell.spellid, SpellEffect::IncreaseSpellHaste)) {
															continue;
														}

														uint32 TempDontBuffMeBeforeTime = tar->DontBuffMeBefore();

														if(selectedMercSpell.spellid > 0) {
															if(isDiscipline) {
																castedSpell = UseDiscipline(selectedMercSpell.spellid, tar->GetID());
															}
															else {
																castedSpell = AIDoSpellCast(selectedMercSpell.spellid, tar, -1, &TempDontBuffMeBeforeTime);

																if(TempDontBuffMeBeforeTime != tar->DontBuffMeBefore())
																	tar->SetDontBuffMeBefore(TempDontBuffMeBeforeTime);
															}
														}
												}

												if(!castedSpell && !engaged && tar->GetPet()) {

													//don't cast group spells on pets
													if(IsGroupSpell(selectedMercSpell.spellid)
														|| spells[selectedMercSpell.spellid].target_type == ST_Group
														|| spells[selectedMercSpell.spellid].target_type == ST_GroupTeleport ) {
															continue;
													}

													if(!tar->GetPet()->IsImmuneToSpell(selectedMercSpell.spellid, this)
														&& CanCastBuff(tar->GetPet(), selectedMercSpell.spellid, mercLevel, allow_rebuff)) {

															uint32 TempDontBuffMeBeforeTime = tar->DontBuffMeBefore();

															if(selectedMercSpell.spellid > 0) {
																if(isDiscipline) {
																	castedSpell = UseDiscipline(selectedMercSpell.spellid, tar->GetPet()->GetID());
																}
																else {
																	castedSpell = AIDoSpellCast(selectedMercSpell.spellid, tar->GetPet(), -1, &TempDontBuffMeBeforeTime);

																	if(TempDontBuffMeBeforeTime != tar->GetPet()->DontBuffMeBefore())
																		tar->GetPet()->SetDontBuffMeBefore(TempDontBuffMeBeforeTime);
																}
															}
													}
												}
											}
										}
									}
								}
								break;
												 }
							case SpellType_Nuke: {
								switch(mercClass)
								{
								case TANK:
									//check for taunt; a merc told to assist leaves the aggro to the owner
									if(!IsAssistStance() && CheckAETaunt()) {
										//get AE taunt
										selectedMercSpell = GetBestMercSpellForAETaunt(this);
										LogMercenaries("[{}] AE Taunting", GetCleanName());
									}

									if(selectedMercSpell.spellid == 0 && !IsAssistStance() && CheckTaunt()) {
										//get taunt
										selectedMercSpell = GetBestMercSpellForTaunt(this);
									}

									//get hate disc
									if(selectedMercSpell.spellid == 0 && (!IsAssistStance() || GetTarget()->GetHateTop() != GetOwner())) {
										selectedMercSpell = GetBestMercSpellForHate(this);
									}

									break;
								case HEALER:
									break;
								case MELEEDPS:
									break;
								case CASTERDPS:
									Mob* tar = GetTarget();

									// Hold the reserve for the end of the fight unless the target is nearly dead
									if(GetManaRatio() < GetCasterManaReservePercent(GetStance()) && tar->GetHPRatio() >= CasterFinishingHPPercent) {
										MercSay(MercChatter::LowMana, fmt::format("Low on mana ({}%).", static_cast<int>(GetManaRatio())));
										break;
									}

									if(ShouldHoldForHate()) {
										break;
									}

									selectedMercSpell = GetBestMercSpellForAENuke(this, tar);

									if(selectedMercSpell.spellid == 0 && !tar->GetSpecialAbility(SpecialAbility::StunImmunity) && !tar->IsStunned()) {
										uint8 stunChance = 15;
										if(zone->random.Roll(stunChance)) {
											selectedMercSpell = GetBestMercSpellForStun(this);
										}
									}

									if(selectedMercSpell.spellid == 0) {
										uint8 lureChance = 25;
										if(zone->random.Roll(lureChance)) {
											selectedMercSpell = GetBestMercSpellForNukeByTargetResists(this, tar);
										}
									}

									if(selectedMercSpell.spellid == 0) {
										selectedMercSpell = GetBestMercSpellForNuke(this);
									}

									break;
								}

								if(selectedMercSpell.spellid > 0) {
									if(isDiscipline) {
										castedSpell = UseDiscipline(selectedMercSpell.spellid, GetTarget()->GetID());
									}
									else {
										castedSpell = AIDoSpellCast(selectedMercSpell.spellid, GetTarget(), -1);
									}
								}

								break;
												 }
							case SpellType_InCombatBuff: {
								std::list<MercSpell> buffSpellList = GetMercSpellsBySpellType(this, SpellType_InCombatBuff);
								Mob* tar = this;

								for (auto itr = buffSpellList.begin();
								     itr != buffSpellList.end(); ++itr) {
									MercSpell selectedMercSpell = *itr;

									if (castedSpell) {
										break;
									}

									if(!(spells[selectedMercSpell.spellid].target_type == ST_Self)) {
										continue;
									}

									if( !tar->IsImmuneToSpell(selectedMercSpell.spellid, this)
										&& (tar->CanBuffStack(selectedMercSpell.spellid, mercLevel, true) >= 0)) {

											uint32 TempDontBuffMeBeforeTime = tar->DontBuffMeBefore();

											if(selectedMercSpell.spellid > 0) {
												if(isDiscipline) {
													castedSpell = UseDiscipline(selectedMercSpell.spellid, GetID());
												}
												else {
													castedSpell = AIDoSpellCast(selectedMercSpell.spellid, this, -1);
												}
											}
									}
								}
								break;
														 }
							case SpellType_Cure: {
								Mob* tar = nullptr;
								Mob* const hate_top = GetTarget() ? GetTarget()->GetHateTop() : nullptr;
								const bool passive_healer = IsPassiveHealer();
								int tar_priority = HealPriorityOther;
								uint32 tar_counters = 0;

								// Owner first, then whoever the mob is hitting, then the member with the most counters
								for(int i = 0; i < MAX_GROUP_MEMBERS; i++) {
									Mob* member = g->members[i];

									if(!member || member->qglobal || !GetNeedsCured(member) || member->DontCureMeBefore() >= Timer::GetCurrentTime()
										|| (passive_healer && member->GetHPRatio() >= EmergencyHealHPPercent)) {
										continue;
									}

									int priority = HealPriorityOther;

									if(member == GetMercenaryOwner()) {
										priority = HealPriorityOwner;
									}
									else if(member == hate_top) {
										priority = HealPriorityTank;
									}

									const uint32 counters = GetCureCounterTotal(member);

									if(!tar || priority < tar_priority || (priority == tar_priority && counters > tar_counters)) {
										tar = member;
										tar_priority = priority;
										tar_counters = counters;
									}
								}

								if(tar && !(g->GetNumberNeedingHealedInGroup(IsEngaged() ? 25 : 40, false) > 0) && !(g->GetNumberNeedingHealedInGroup(IsEngaged() ? 40 : 60, false) > 2))
								{
									selectedMercSpell = GetBestMercSpellForCure(this, tar);

									if(selectedMercSpell.spellid == 0)
										break;

									uint32 TempDontCureMeBeforeTime = tar->DontCureMeBefore();

									castedSpell = AIDoSpellCast(selectedMercSpell.spellid, tar, spells[selectedMercSpell.spellid].mana, &TempDontCureMeBeforeTime);

									if(castedSpell) {
										if(IsGroupSpell(selectedMercSpell.spellid)){

											if(HasGroup()) {
												Group *g = GetGroup();

												if(g) {
													for( int i = 0; i<MAX_GROUP_MEMBERS; i++) {
														if(g->members[i] && !g->members[i]->qglobal) {
															if(TempDontCureMeBeforeTime != tar->DontCureMeBefore())
																g->members[i]->SetDontCureMeBefore(Timer::GetCurrentTime() + 4000);
														}
													}
												}
											}
										}
										else {
											if(TempDontCureMeBeforeTime != tar->DontCureMeBefore())
												tar->SetDontCureMeBefore(Timer::GetCurrentTime() + 4000);
										}
									}
								}
								break;
												 }
							case SpellType_Resurrect: {
								Corpse *corpse = GetGroupMemberCorpse();

								if(corpse) {
									selectedMercSpell = GetFirstMercSpellBySpellType(this, SpellType_Resurrect);

									if(selectedMercSpell.spellid == 0)
										break;

									uint32 TempDontRootMeBeforeTime = corpse->DontRootMeBefore();

									castedSpell = AIDoSpellCast(selectedMercSpell.spellid, corpse, spells[selectedMercSpell.spellid].mana, &TempDontRootMeBeforeTime);

									if(castedSpell) {
										MercSay(MercChatter::Resurrect, fmt::format("Resurrecting {}.", corpse->GetCleanName()));
									}

									//CastSpell(selectedMercSpell.spellid, corpse->GetID(), 1, -1, -1, &TempDontRootMeBeforeTime);
									corpse->SetDontRootMeBefore(TempDontRootMeBeforeTime);
								}

								break;
													  }
							case SpellType_Escape: {
								Mob* tar = GetTarget();
								uint8 hpr = (uint8)GetHPRatio();
								bool mayGetAggro = false;

								if(tar && (mercClass == CASTERDPS || mercClass == MELEEDPS)) {
									mayGetAggro = HasOrMayGetAggro(); //classes have hate reducing spells

									if (mayGetAggro) {
										selectedMercSpell = GetFirstMercSpellBySpellType(this, SpellType_Escape);

										if(selectedMercSpell.spellid == 0)
											break;

										if(isDiscipline) {
											castedSpell = UseDiscipline(selectedMercSpell.spellid, tar->GetID());
										}
										else {
											castedSpell = AIDoSpellCast(selectedMercSpell.spellid, tar, -1);
										}
									}
								}
								break;
												   }
			}
		}
	}

	return castedSpell;
}

void Merc::CheckHateList() {
	if(check_target_timer.Enabled())
		check_target_timer.Disable();

	if(!IsEngaged()) {
		if(IsHoldingBackOffense()) {
			return;
		}

		if(GetFollowID()) {
			Group* g = GetGroup();
			if(g) {
				Mob* MercOwner = GetOwner();
				bool added_owner_target = false;

				if(MercOwner && ShouldEngageOwnerTarget(MercOwner)) {
					Mob* owner_target = MercOwner->GetTarget();
					float range = g->HasRole(MercOwner, RolePuller) ? RuleI(Mercs, AggroRadiusPuller) : RuleI(Mercs, AggroRadius);

					// An aggressive merc goes after what the owner targets from further away
					if(RuleB(Mercs, StanceBehavior) && GetStance() == Stance::Aggressive) {
						range *= AggressiveRangeMultiplier;
					}

					range = range * range;
					if(DistanceSquaredNoZ(m_Position, owner_target->GetPosition()) < range) {
						AddToHateList(owner_target, 1);
						added_owner_target = true;
					}
				}

				if(!added_owner_target) {
					AddNPCsHatingGroup();
				}
			}
		}
	}
}

// Only mobs near the owner are looked at: mercs are not scanned for close mobs, the owner is
std::unordered_map<uint16, Mob*>& Merc::GetNearbyMobs(float distance) {
	Mob* owner = GetOwner();

	return owner ? owner->GetCloseMobList(distance) : GetCloseMobList(distance);
}

void Merc::AddNPCsHatingGroup() {
	Group* g = GetGroup();
	if(!g) {
		return;
	}

	const float radius = RuleI(Mercs, AggroRadius);
	const float radius_squared = radius * radius;

	for (const auto& [mob_id, mob] : GetNearbyMobs(radius)) {
		if(!mob || !mob->IsNPC() || mob->IsMerc() || mob->IsMezzed() || hate_list.IsEntOnHateList(mob) || !IsAttackAllowed(mob)) {
			continue;
		}

		if(DistanceSquaredNoZ(m_Position, mob->GetPosition()) > radius_squared) {
			continue;
		}

		NPC* npc = mob->CastToNPC();

		for(int counter = 0; counter < MAX_GROUP_MEMBERS; counter++) {
			Mob* groupMember = g->members[counter];
			if(!groupMember || !npc->IsOnHatelist(groupMember)) {
				continue;
			}

			float range = g->HasRole(groupMember, RolePuller) ? RuleI(Mercs, AggroRadiusPuller) : RuleI(Mercs, AggroRadius);
			range *= range;
			if(DistanceSquaredNoZ(m_Position, npc->GetPosition()) < range) {
				hate_list.AddEntToHateList(npc, 1);
				break;
			}
		}
	}
}

bool Merc::HasOrMayGetAggro() {
	bool mayGetAggro = false;

	if(GetTarget() && GetTarget()->GetHateTop()) {
		Mob *topHate = GetTarget()->GetHateTop();

		if(topHate == this)
			mayGetAggro = true; //I currently have aggro
		else {
			int64 myHateAmt = GetTarget()->GetHateAmount(this);
			int64 topHateAmt = GetTarget()->GetHateAmount(topHate);

			if(myHateAmt > 0 && topHateAmt > 0 && myHateAmt * 100 / topHateAmt > RuleI(Mercs, HateRiskPercent)) //I have this much of top hate, next action may give me aggro
				mayGetAggro = true;
		}
	}

	return mayGetAggro;
}

// A mob that walks up to a caster is not hitting it, so the caster backs out to its usual distance instead of
// standing in melee. Retries are spaced out and the spot must see the mob, so a bad path cannot loop.
bool Merc::StepBackFromTarget() {
	Mob* const target = GetTarget();
	const float step_back_distance = RuleI(Mercs, CasterStepBackDistance);
	const uint32 now = Timer::GetCurrentTime();

	if(!target || step_back_distance <= 0 || now < step_back_allowed_time || target->GetHateTop() == this) {
		return false;
	}

	if(DistanceSquaredNoZ(m_Position, target->GetPosition()) >= step_back_distance * step_back_distance) {
		return false;
	}

	const float away_x = GetX() - target->GetX();
	const float away_y = GetY() - target->GetY();
	const float away_length = std::sqrt(away_x * away_x + away_y * away_y);

	if(away_length < 1.0f) {
		return false;
	}

	const float goal_x = target->GetX() + away_x / away_length * CasterMinTargetDistance;
	const float goal_y = target->GetY() + away_y / away_length * CasterMinTargetDistance;

	step_back_allowed_time = now + StepBackRetryMs;
	step_back_until = now + StepBackMoveMs;

	if(!CheckLosFN(goal_x, goal_y, GetZ(), target->GetSize())) {
		return false;
	}

	RunTo(goal_x, goal_y, GetZ());

	return true;
}

// Balanced damage mercs let the group build hate instead of taking the mob; Burn stances spend everything
bool Merc::ShouldHoldForHate() {
	const uint8 stance = GetStance();

	if(!RuleB(Mercs, StanceBehavior) || stance == Stance::Burn || stance == Stance::AEBurn) {
		return false;
	}

	Mob* const tar = GetTarget();

	// A merc that already has the mob gains nothing by holding back
	return tar && tar->GetHateTop() != this && HasOrMayGetAggro();
}

// A mob that has picked the merc out but has not hit it yet is not on the hate list, so engagement alone is too late
bool Merc::IsTargetedByHostile() {
	if(!RuleB(Mercs, StandWhenTargeted)) {
		return false;
	}

	for(const auto& [mob_id, mob] : GetNearbyMobs(RuleI(Mercs, AggroRadius))) {
		if(!mob || !mob->IsNPC() || mob->IsMerc() || !IsAttackAllowed(mob)) {
			continue;
		}

		if(mob->GetTarget() == this || mob->CastToNPC()->IsOnHatelist(this)) {
			return true;
		}
	}

	return false;
}

// A merc that sits while its owner is still walking stands again on the next step, so it waits until the owner
// has stopped and the merc has been next to them for a moment
bool Merc::CanRestNearOwner() {
	Mob* const follow = GetFollowID() ? entity_list.GetMob(GetFollowID()) : nullptr;

	if(!follow) {
		return true;
	}

	const glm::vec4& owner_position = follow->GetPosition();

	const float follow_distance = GetFollowDistance();

	if(DistanceSquared(m_Position, owner_position) > follow_distance * follow_distance) {
		follow_range_since = 0;

		return false;
	}

	const uint32 now = Timer::GetCurrentTime();

	// IsMoving() can stay set after a client stops, so the owner counts as stopped once its position holds still
	if(!follow_range_since || owner_position.x != follow_owner_position.x || owner_position.y != follow_owner_position.y
		|| owner_position.z != follow_owner_position.z) {
		follow_range_since = now;
		follow_owner_position = owner_position;

		return false;
	}

	return now - follow_range_since >= IdleSitDelayMs;
}

bool Merc::IsHoldingBackOffense() {
	if(RuleB(Mercs, HoldWhenOwnerFeigned)) {
		Mob* owner = GetOwner();

		if(owner && owner->GetFeigned()) {
			return true;
		}
	}

	if(GetCalledTarget()) {
		return false;
	}

	if(GetClass() == HEALER) {
		return false;
	}

	return GetOwnerSettings().hold || (RuleB(Mercs, StanceBehavior) && GetStance() == Stance::Passive);
}

const MercOwnerSettings& Merc::GetOwnerSettings() {
	static const MercOwnerSettings default_settings{};
	Client* const owner = GetMercenaryOwner();

	return owner ? owner->GetMercSettings() : default_settings;
}

float Merc::GetFollowDistance() {
	const uint8 chosen = GetOwnerSettings().follow_distance;

	return chosen ? chosen : FollowDistance;
}

bool Merc::IsPassiveHealer() {
	return RuleB(Mercs, StanceBehavior) && GetClass() == HEALER && GetStance() == Stance::Passive;
}

bool Merc::EngagesOnOwnerAttack() {
	if (!RuleB(Mercs, StanceBehavior) || GetClass() == HEALER) {
		return false;
	}

	const uint8 stance = GetStance();

	return stance == Stance::Aggressive || stance == Stance::Burn || stance == Stance::AEBurn || stance == Stance::Assist;
}

bool Merc::IsAssistStance() {
	return RuleB(Mercs, StanceBehavior) && GetStance() == Stance::Assist;
}

// The owner's target is worth engaging once it hates the owner. Aggressive stances also join as soon as the owner
// turns on auto attack; a target alone (conning, buffing, looking) never sends the merc in.
bool Merc::ShouldEngageOwnerTarget(Mob* owner) {
	Mob* const owner_target = owner->GetTarget();

	if(!owner_target || !owner_target->IsNPC() || owner_target->IsMezzed() || !IsAttackAllowed(owner_target)) {
		return false;
	}

	if(owner_target->GetHateAmount(owner)) {
		return true;
	}

	const bool owner_attacking = owner->IsClient() && owner->CastToClient()->AutoAttackEnabled();

	// TOB has an Auto Assist checkbox: for the stances that join when the owner attacks, on = join any fight the
	// owner starts, off = only fights already on the owner. Healers never join on the owner's attack.
	if(owner->IsClient() && owner->CastToClient()->ClientVersion() >= EQ::versions::ClientVersion::TOB) {
		const bool stance_joins = RuleB(Mercs, StanceBehavior) ? EngagesOnOwnerAttack() : GetClass() != HEALER;

		return stance_joins && owner->CastToClient()->IsMercAutoAssistOn() && owner_attacking && owner->IsAttackAllowed(owner_target);
	}

	if(EngagesOnOwnerAttack()) {
		return owner_attacking && owner->IsAttackAllowed(owner_target);
	}

	// Auto attack alone only pulls when stance behavior is off, as it always did
	return !RuleB(Mercs, StanceBehavior) && owner_attacking;
}

// The Call Assist button names one mob for the merc to fight now, whatever its stance. It holds for Mercs:CallAssistSeconds
// while the merc is not fighting the mob, and afterwards until the mob is dead or can no longer be attacked.
Mob* Merc::GetCalledTarget() {
	if(!called_target_id) {
		return nullptr;
	}

	Mob* const called = entity_list.GetMob(called_target_id);
	const bool expired = (called && !hate_list.IsEntOnHateList(called)) || (Timer::GetCurrentTime() >= called_until && GetTarget() != called);

	if(!called || !called->IsNPC() || called->IsMezzed() || !IsAttackAllowed(called) || expired) {
		called_target_id = 0;

		return nullptr;
	}

	return called;
}

void Merc::CallAssist(Mob* target) {
	const uint32 now = Timer::GetCurrentTime();

	called_target_id = target->GetID();
	called_until = now + RuleI(Mercs, CallAssistSeconds) * 1000;
	called_heal_until = now + RuleI(Mercs, CallAssistHealWindowSeconds) * 1000;
	retarget_allowed_time = 0;

	AddToHateList(target, 1);

	MercSay(MercChatter::Assist, fmt::format("Assisting {}.", target->GetCleanName()));
}

bool Merc::IsValidAssistTarget(Mob* target) {
	return target && target->IsNPC() && !target->IsMezzed() && hate_list.IsEntOnHateList(target) && IsAttackAllowed(target);
}

// Damage mercs follow the owner's target, then the group's main assist
Mob* Merc::GetAssistTarget() {
	Mob* owner = GetOwner();
	Mob* candidate = owner ? owner->GetTarget() : nullptr;

	if(IsValidAssistTarget(candidate)) {
		return candidate;
	}

	Group* g = GetGroup();
	if(!g) {
		return nullptr;
	}

	for(int i = 0; i < MAX_GROUP_MEMBERS; i++) {
		Mob* member = g->members[i];

		if(member && member != this && g->HasRole(member, RoleAssist) && IsValidAssistTarget(member->GetTarget())) {
			return member->GetTarget();
		}
	}

	return nullptr;
}

// A tank peels mobs off group members who are not the group tank, the owner and the healer mercenary first
Mob* Merc::GetTankTarget() {
	Group* g = GetGroup();
	Mob* peel_target = nullptr;
	int peel_priority = 0;

	if(g) {
		for(const auto& entry : hate_list.GetHateList()) {
			Mob* mob = entry->entity_on_hatelist;

			if(!mob || !mob->IsNPC() || mob->IsMezzed()) {
				continue;
			}

			Mob* hit_member = mob->GetHateTop();

			if(!hit_member || hit_member == this || hit_member->GetGroup() != g || g->HasRole(hit_member, RoleTank)) {
				continue;
			}

			const int priority = (hit_member == GetOwner() || (hit_member->IsMerc() && hit_member->GetClass() == HEALER)) ? 0 : 1;

			if(!peel_target || priority < peel_priority) {
				peel_target = mob;
				peel_priority = priority;
			}
		}
	}

	if(peel_target) {
		return peel_target;
	}

	return GetAssistTarget();
}

void Merc::PickTarget() {
	Mob* const current = GetTarget();
	Mob* const called = GetCalledTarget();

	if(called && hate_list.IsEntOnHateList(called)) {
		if(called != current) {
			SetTarget(called);
			retarget_allowed_time = Timer::GetCurrentTime() + MinRetargetIntervalMs;
		}

		return;
	}

	const bool fighting_current = current && hate_list.IsEntOnHateList(current) && !current->IsMezzed();

	// Hold a target for a few seconds so an owner who tab targets does not make the merc ping-pong
	if(fighting_current && (Timer::GetCurrentTime() < retarget_allowed_time || !pick_target_timer.Check())) {
		return;
	}

	AddNPCsHatingGroup();

	Mob* choice = nullptr;

	switch(GetClass()) {
		case TANK:
			choice = IsAssistStance() ? GetAssistTarget() : GetTankTarget();
			break;
		case MELEEDPS:
		case CASTERDPS:
			choice = GetAssistTarget();
			break;
		default:
			break;
	}

	if(!choice) {
		choice = hate_list.GetMobWithMostHateOnList(this, nullptr, true);

		if(choice && !IsAttackAllowed(choice)) {
			choice = nullptr;
		}
	}

	if(choice && choice != current) {
		if(!current) {
			MercSay(MercChatter::Engage, fmt::format("Attacking {}.", choice->GetCleanName()));
		}

		SetTarget(choice);
		retarget_allowed_time = Timer::GetCurrentTime() + MinRetargetIntervalMs;
	}
}

// Balanced when the template offers it, unless the data marks another stance as the default
uint8 Merc::GetDefaultStance(uint32 merc_template_id) {
	const auto stances_it = zone->merc_stance_list.find(merc_template_id);

	if(stances_it == zone->merc_stance_list.end() || stances_it->second.empty()) {
		return Stance::Balanced;
	}

	const std::list<MercStanceInfo>& stances = stances_it->second;

	for(const auto& stance : stances) {
		if(stance.IsDefault) {
			return stance.StanceID;
		}
	}

	for(const auto& stance : stances) {
		if(stance.StanceID == Stance::Balanced) {
			return Stance::Balanced;
		}
	}

	return stances.front().StanceID;
}

bool Merc::CheckAENuke(Merc* caster, Mob* tar, int32 spell_id, uint8 &numTargets) {
	const float aoe_range = spells[spell_id].aoe_range;
	const float aoe_range_squared = aoe_range * aoe_range;
	Group* const group = caster->GetGroup();

	for (const auto& [mob_id, mob] : caster->GetNearbyMobs(aoe_range)) {
		if(!mob || !mob->IsNPC() || mob->IsMerc()) {
			continue;
		}

		if(DistanceSquaredNoZ(mob->GetPosition(), tar->GetPosition()) > aoe_range_squared) {
			continue;
		}

		// Pets, charmed mobs and anything else the merc may not attack are not hit, and do not count
		if(!caster->IsAttackAllowed(mob)) {
			continue;
		}

		if(mob->IsMezzed()) {
			numTargets = 0;
			return false;
		}

		bool hates_group = caster->hate_list.IsEntOnHateList(mob);

		if(!hates_group && group) {
			for(int i = 0; i < MAX_GROUP_MEMBERS; i++) {
				Mob* member = group->members[i];

				if(member && mob->CastToNPC()->IsOnHatelist(member)) {
					hates_group = true;
					break;
				}
			}
		}

		// An idle mob in range would be pulled by the AE
		if(!hates_group) {
			numTargets = 0;
			return false;
		}

		numTargets++;
	}

	return numTargets >= std::max(1, RuleI(Mercs, AEMinTargets));
}

int64 Merc::GetFocusEffect(focusType type, int32 spell_id, bool from_buff_tic) {

	int32 realTotal = 0;
	int32 realTotal2 = 0;
	int32 realTotal3 = 0;
	bool rand_effectiveness = false;

	//Improved Healing, Damage & Mana Reduction are handled differently in that some are random percentages
	//In these cases we need to find the most powerful effect, so that each piece of gear wont get its own chance
	if((type == focusManaCost || type == focusImprovedHeal || type == focusImprovedDamage)
		&& RuleB(Spells, LiveLikeFocusEffects))
	{
		rand_effectiveness = true;
	}

	//Check if item focus effect exists for the client.
	if (itembonuses.FocusEffects[type]){

		const EQ::ItemData* TempItem = nullptr;
		const EQ::ItemData* UsedItem = nullptr;
		int32 UsedFocusID = 0;
		int32 Total = 0;
		int32 focus_max = 0;
		int32 focus_max_real = 0;

		//item focus
		for (int x = EQ::invslot::EQUIPMENT_BEGIN; x <= EQ::invslot::EQUIPMENT_END; ++x)
		{
			TempItem = nullptr;
			if (equipment[x] == 0)
				continue;
			TempItem = database.GetItem(equipment[x]);
			if (TempItem && IsValidSpell(TempItem->Focus.Effect)) {
				if(rand_effectiveness) {
					focus_max = CalcFocusEffect(type, TempItem->Focus.Effect, spell_id, true);
					if (focus_max > 0 && focus_max_real >= 0 && focus_max > focus_max_real) {
						focus_max_real = focus_max;
						UsedItem = TempItem;
						UsedFocusID = TempItem->Focus.Effect;
					} else if (focus_max < 0 && focus_max < focus_max_real) {
						focus_max_real = focus_max;
						UsedItem = TempItem;
						UsedFocusID = TempItem->Focus.Effect;
					}
				}
				else {
					Total = CalcFocusEffect(type, TempItem->Focus.Effect, spell_id);
					if (Total > 0 && realTotal >= 0 && Total > realTotal) {
						realTotal = Total;
						UsedItem = TempItem;
						UsedFocusID = TempItem->Focus.Effect;
					} else if (Total < 0 && Total < realTotal) {
						realTotal = Total;
						UsedItem = TempItem;
						UsedFocusID = TempItem->Focus.Effect;
					}
				}
			}
		}

		if(UsedItem && rand_effectiveness && focus_max_real != 0)
			realTotal = CalcFocusEffect(type, UsedFocusID, spell_id);

		if (realTotal != 0 && UsedItem)
			MessageString(Chat::FocusEffect, BEGINS_TO_GLOW, UsedItem->Name);
	}

	//Check if spell focus effect exists for the client.
	if (spellbonuses.FocusEffects[type]){

		//Spell Focus
		int32 Total2 = 0;
		int32 focus_max2 = 0;
		int32 focus_max_real2 = 0;

		int buff_tracker = -1;
		int buff_slot = 0;
		int32 focusspellid = 0;
		int32 focusspell_tracker = 0;
		uint32 buff_max = GetMaxTotalSlots();
		for (buff_slot = 0; buff_slot < buff_max; buff_slot++) {
			focusspellid = buffs[buff_slot].spellid;
			if (focusspellid == 0 || focusspellid >= SPDAT_RECORDS)
				continue;

			if(rand_effectiveness) {
				focus_max2 = CalcFocusEffect(type, focusspellid, spell_id, true);
				if (focus_max2 > 0 && focus_max_real2 >= 0 && focus_max2 > focus_max_real2) {
					focus_max_real2 = focus_max2;
					buff_tracker = buff_slot;
					focusspell_tracker = focusspellid;
				} else if (focus_max2 < 0 && focus_max2 < focus_max_real2) {
					focus_max_real2 = focus_max2;
					buff_tracker = buff_slot;
					focusspell_tracker = focusspellid;
				}
			}
			else {
				Total2 = CalcFocusEffect(type, focusspellid, spell_id);
				if (Total2 > 0 && realTotal2 >= 0 && Total2 > realTotal2) {
					realTotal2 = Total2;
					buff_tracker = buff_slot;
					focusspell_tracker = focusspellid;
				} else if (Total2 < 0 && Total2 < realTotal2) {
					realTotal2 = Total2;
					buff_tracker = buff_slot;
					focusspell_tracker = focusspellid;
				}
			}
		}

		if(focusspell_tracker && rand_effectiveness && focus_max_real2 != 0)
			realTotal2 = CalcFocusEffect(type, focusspell_tracker, spell_id);

		if (!from_buff_tic && buff_tracker >= 0 && buffs[buff_tracker].hit_number > 0) {
			CheckNumHitsRemaining(NumHit::MatchingSpells, buff_tracker);
		}
	}


	// AA Focus
	/*if (aabonuses.FocusEffects[type]){

	int16 Total3 = 0;
	uint32 slots = 0;
	uint32 aa_AA = 0;
	uint32 aa_value = 0;

	for (int i = 0; i < MAX_PP_AA_ARRAY; i++)
	{
	aa_AA = aa[i]->AA;
	aa_value = aa[i]->value;
	if (aa_AA < 1 || aa_value < 1)
	continue;

	Total3 = CalcAAFocus(type, aa_AA, spell_id);
	if (Total3 > 0 && realTotal3 >= 0 && Total3 > realTotal3) {
	realTotal3 = Total3;
	}
	else if (Total3 < 0 && Total3 < realTotal3) {
	realTotal3 = Total3;
	}
	}
	}*/

	if(type == focusReagentCost && IsSummonPetSpell(spell_id) && GetAA(aaElementalPact))
		return 100;

	if(type == focusReagentCost && (IsEffectInSpell(spell_id, SpellEffect::SummonItem) || IsSacrificeSpell(spell_id)))
		return 0;
	//Summon Spells that require reagents are typically imbue type spells, enchant metal, sacrifice and shouldn't be affected
	//by reagent conservation for obvious reasons.

	return realTotal + realTotal2 + realTotal3;
}

int8 Merc::GetChanceToCastBySpellType(uint32 spellType) {
	int mercStance = (int)GetStance();
	int8 mercClass = GetClass();
	int8 chance = 0;

	switch (spellType) {
			case SpellType_Nuke: {
				switch(mercClass)
				{
				case TANK: {
					chance = 100;
					break;
						   }
				case HEALER:{
					break;
							}
				case MELEEDPS:{
					chance = 100;
					break;
							  }
				case CASTERDPS:{
					chance = 100;
					break;
							   }
				}
				break;
								 }
			case SpellType_Heal: {
				switch(mercClass)
				{
				case TANK: {
					break;
						   }
				case HEALER:{
					chance = 100;
					break;
							}
				case MELEEDPS:{
					break;
							  }
				case CASTERDPS:{
					break;
							   }
				}
				break;
								 }
			case SpellType_Root: {
				switch(mercClass)
				{
				case TANK: {
					break;
						   }
				case HEALER:{
					break;
							}
				case MELEEDPS:{
					break;
							  }
				case CASTERDPS:{
					break;
							   }
				}
				break;
								 }
			case SpellType_Buff: {
				switch(mercClass)
				{
				case TANK: {
					break;
						   }
				case HEALER:{
					chance = IsEngaged() && !IsEngagedBuffingAllowed(GetStance()) ? 0 : 100;
					break;
							}
				case MELEEDPS:{
					break;
							  }
				case CASTERDPS:{
					break;
							   }
				}
				break;
								 }
			case SpellType_InCombatBuff: {
				switch(mercClass)
				{
				case TANK: {
					chance = 50;
					break;
						   }
				case HEALER:{
					break;
							}
				case MELEEDPS:{
					chance = 50;
					break;
							  }
				case CASTERDPS:{
					break;
							   }
				}
				break;
										 }
			case SpellType_Escape: {
				switch(mercClass)
				{
				case TANK: {
					break;
						   }
				case HEALER:{
					break;
							}
				case MELEEDPS:{
					chance = 100;
					break;
							  }
				case CASTERDPS:{
					chance = 100;
					break;
							   }
				}
				break;
								   }
			default:
				chance = 0;
				break;
	}

	return chance;
}

bool Merc::CheckStance(int16 stance) {

	//checks of current stance matches stances listed as valid for spell in database
	//stance = 0 for all stances, stance # for only that stance & -stance# for all but that stance
	if (stance == 0 || (stance > 0 && stance == GetStance()) || (stance < 0 && std::abs(stance) != GetStance())) {
			return true;
	}

	return false;
}

std::list<MercSpell> Merc::GetMercSpellsBySpellType(Merc* caster, uint32 spellType) {
	std::list<MercSpell> result;

	if(caster && caster->AI_HasSpells()) {
		const std::vector<MercSpell>& mercSpellList = caster->GetMercSpells();

		for (int i = mercSpellList.size() - 1; i >= 0; i--) {
			if (!IsValidSpell(mercSpellList[i].spellid)) {
				// this is both to quit early to save cpu and to avoid casting bad spells
				// Bad info from database can trigger this incorrectly, but that should be fixed in DB, not here
				continue;
			}

			if((mercSpellList[i].type & spellType) && caster->CheckStance(mercSpellList[i].stance)) {
				MercSpell mercSpell;
				mercSpell.spellid = mercSpellList[i].spellid;
				mercSpell.stance = mercSpellList[i].stance;
				mercSpell.type = mercSpellList[i].type;
				mercSpell.slot = mercSpellList[i].slot;
				mercSpell.proc_chance = mercSpellList[i].proc_chance;
				mercSpell.time_cancast = mercSpellList[i].time_cancast;

				result.push_back(mercSpell);
			}
		}
	}

	return result;
}

MercSpell Merc::GetFirstMercSpellBySpellType(Merc* caster, uint32 spellType) {
	MercSpell result;

	result.spellid = 0;
	result.stance = 0;
	result.type = 0;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	if(caster && caster->AI_HasSpells()) {
		const std::vector<MercSpell>& mercSpellList = caster->GetMercSpells();

		for (int i = mercSpellList.size() - 1; i >= 0; i--) {
			if (!IsValidSpell(mercSpellList[i].spellid)) {
				// this is both to quit early to save cpu and to avoid casting bad spells
				// Bad info from database can trigger this incorrectly, but that should be fixed in DB, not here
				continue;
			}

			if((mercSpellList[i].type & spellType)
				&& caster->CheckStance(mercSpellList[i].stance)
				&& CheckSpellRecastTimers(caster, mercSpellList[i])) {
					result.spellid = mercSpellList[i].spellid;
					result.stance = mercSpellList[i].stance;
					result.type = mercSpellList[i].type;
					result.slot = mercSpellList[i].slot;
					result.proc_chance = mercSpellList[i].proc_chance;
					result.time_cancast = mercSpellList[i].time_cancast;

					break;
			}
		}
	}

	return result;
}

MercSpell Merc::GetMercSpellBySpellID(Merc* caster, int32 spellid) {
	MercSpell result;

	result.spellid = 0;
	result.stance = 0;
	result.type = 0;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	if(caster && caster->AI_HasSpells()) {
		const std::vector<MercSpell>& mercSpellList = caster->GetMercSpells();

		for (int i = mercSpellList.size() - 1; i >= 0; i--) {
			if (!IsValidSpell(mercSpellList[i].spellid)) {
				// this is both to quit early to save cpu and to avoid casting bad spells
				// Bad info from database can trigger this incorrectly, but that should be fixed in DB, not here
				continue;
			}

			if((mercSpellList[i].spellid == spellid)
				&& caster->CheckStance(mercSpellList[i].stance)) {
					result.spellid = mercSpellList[i].spellid;
					result.stance = mercSpellList[i].stance;
					result.type = mercSpellList[i].type;
					result.slot = mercSpellList[i].slot;
					result.proc_chance = mercSpellList[i].proc_chance;
					result.time_cancast = mercSpellList[i].time_cancast;

					break;
			}
		}
	}

	return result;
}

std::list<MercSpell> Merc::GetMercSpellsForSpellEffect(Merc* caster, int spellEffect) {
	std::list<MercSpell> result;

	if(caster && caster->AI_HasSpells()) {
		const std::vector<MercSpell>& mercSpellList = caster->GetMercSpells();

		for (int i = mercSpellList.size() - 1; i >= 0; i--) {
			if (!IsValidSpell(mercSpellList[i].spellid)) {
				// this is both to quit early to save cpu and to avoid casting bad spells
				// Bad info from database can trigger this incorrectly, but that should be fixed in DB, not here
				continue;
			}

			if(IsEffectInSpell(mercSpellList[i].spellid, spellEffect) && caster->CheckStance(mercSpellList[i].stance)) {
				MercSpell MercSpell;
				MercSpell.spellid = mercSpellList[i].spellid;
				MercSpell.stance = mercSpellList[i].stance;
				MercSpell.type = mercSpellList[i].type;
				MercSpell.slot = mercSpellList[i].slot;
				MercSpell.proc_chance = mercSpellList[i].proc_chance;
				MercSpell.time_cancast = mercSpellList[i].time_cancast;

				result.push_back(MercSpell);
			}
		}
	}

	return result;
}

std::list<MercSpell> Merc::GetMercSpellsForSpellEffectAndTargetType(Merc* caster, int spellEffect, SpellTargetType targetType) {
	std::list<MercSpell> result;

	if(caster && caster->AI_HasSpells()) {
		const std::vector<MercSpell>& mercSpellList = caster->GetMercSpells();

		for (int i = mercSpellList.size() - 1; i >= 0; i--) {
			if (!IsValidSpell(mercSpellList[i].spellid)) {
				// this is both to quit early to save cpu and to avoid casting bad spells
				// Bad info from database can trigger this incorrectly, but that should be fixed in DB, not here
				continue;
			}

			if(IsEffectInSpell(mercSpellList[i].spellid, spellEffect) && caster->CheckStance(mercSpellList[i].stance)) {
				if(spells[mercSpellList[i].spellid].target_type == targetType) {
					MercSpell MercSpell;
					MercSpell.spellid = mercSpellList[i].spellid;
					MercSpell.stance = mercSpellList[i].stance;
					MercSpell.type = mercSpellList[i].type;
					MercSpell.slot = mercSpellList[i].slot;
					MercSpell.proc_chance = mercSpellList[i].proc_chance;
					MercSpell.time_cancast = mercSpellList[i].time_cancast;

					result.push_back(MercSpell);
				}
			}
		}
	}

	return result;
}

MercSpell Merc::GetBestMercSpellForVeryFastHeal(Merc* caster) {
	MercSpell result;

	result.spellid = 0;
	result.stance = 0;
	result.type = 0;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	if(caster) {
		std::list<MercSpell> mercSpellList = GetMercSpellsForSpellEffect(caster, SpellEffect::CurrentHP);

		for (auto mercSpellListItr = mercSpellList.begin(); mercSpellListItr != mercSpellList.end();
		     ++mercSpellListItr) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if(IsVeryFastHealSpell(mercSpellListItr->spellid)
				&& CheckSpellRecastTimers(caster, *mercSpellListItr)) {
					result.spellid = mercSpellListItr->spellid;
					result.stance = mercSpellListItr->stance;
					result.type = mercSpellListItr->type;
					result.slot = mercSpellListItr->slot;
					result.proc_chance = mercSpellListItr->proc_chance;
					result.time_cancast = mercSpellListItr->time_cancast;

					break;
			}
		}
	}

	return result;
}

MercSpell Merc::GetBestMercSpellForFastHeal(Merc* caster) {
	MercSpell result;

	result.spellid = 0;
	result.stance = 0;
	result.type = 0;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	if(caster) {
		std::list<MercSpell> mercSpellList = GetMercSpellsForSpellEffect(caster, SpellEffect::CurrentHP);

		for (auto mercSpellListItr = mercSpellList.begin(); mercSpellListItr != mercSpellList.end();
		     ++mercSpellListItr) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if(IsFastHealSpell(mercSpellListItr->spellid)
				&& CheckSpellRecastTimers(caster, *mercSpellListItr)) {
					result.spellid = mercSpellListItr->spellid;
					result.stance = mercSpellListItr->stance;
					result.type = mercSpellListItr->type;
					result.slot = mercSpellListItr->slot;
					result.proc_chance = mercSpellListItr->proc_chance;
					result.time_cancast = mercSpellListItr->time_cancast;

					break;
			}
		}
	}

	return result;
}

MercSpell Merc::GetBestMercSpellForHealOverTime(Merc* caster) {
	MercSpell result;

	result.spellid = 0;
	result.stance = 0;
	result.type = 0;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	if(caster) {
		std::list<MercSpell> mercHoTSpellList = GetMercSpellsForSpellEffect(caster, SpellEffect::HealOverTime);

		for (auto mercSpellListItr = mercHoTSpellList.begin(); mercSpellListItr != mercHoTSpellList.end();
		     ++mercSpellListItr) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if(IsHealOverTimeSpell(mercSpellListItr->spellid)) {

				if (mercSpellListItr->spellid <= 0 || mercSpellListItr->spellid >= SPDAT_RECORDS) {
					// this is both to quit early to save cpu and to avoid casting bad spells
					// Bad info from database can trigger this incorrectly, but that should be fixed in DB, not here
					continue;
				}

				if(CheckSpellRecastTimers(caster, *mercSpellListItr)) {
					result.spellid = mercSpellListItr->spellid;
					result.stance = mercSpellListItr->stance;
					result.type = mercSpellListItr->type;
					result.slot = mercSpellListItr->slot;
					result.proc_chance = mercSpellListItr->proc_chance;
					result.time_cancast = mercSpellListItr->time_cancast;
				}

				break;
			}
		}
	}

	return result;
}

MercSpell Merc::GetBestMercSpellForPercentageHeal(Merc* caster) {
	MercSpell result;

	result.spellid = 0;
	result.stance = 0;
	result.type = 0;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	if(caster && caster->AI_HasSpells()) {
		std::list<MercSpell> mercSpellList = GetMercSpellsForSpellEffect(caster, SpellEffect::CurrentHP);

		for (auto mercSpellListItr = mercSpellList.begin(); mercSpellListItr != mercSpellList.end();
		     ++mercSpellListItr) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if(IsCompleteHealSpell(mercSpellListItr->spellid)
				&& CheckSpellRecastTimers(caster, *mercSpellListItr)) {
					result.spellid = mercSpellListItr->spellid;
					result.stance = mercSpellListItr->stance;
					result.type = mercSpellListItr->type;
					result.slot = mercSpellListItr->slot;
					result.proc_chance = mercSpellListItr->proc_chance;
					result.time_cancast = mercSpellListItr->time_cancast;

					break;
			}
		}
	}

	return result;
}

MercSpell Merc::GetBestMercSpellForRegularSingleTargetHeal(Merc* caster) {
	MercSpell result;

	result.spellid = 0;
	result.stance = 0;
	result.type = 0;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	if(caster) {
		std::list<MercSpell> mercSpellList = GetMercSpellsForSpellEffect(caster, SpellEffect::CurrentHP);

		for (auto mercSpellListItr = mercSpellList.begin(); mercSpellListItr != mercSpellList.end();
		     ++mercSpellListItr) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if(IsRegularSingleTargetHealSpell(mercSpellListItr->spellid)
				&& CheckSpellRecastTimers(caster, *mercSpellListItr)) {
					result.spellid = mercSpellListItr->spellid;
					result.stance = mercSpellListItr->stance;
					result.type = mercSpellListItr->type;
					result.slot = mercSpellListItr->slot;
					result.proc_chance = mercSpellListItr->proc_chance;
					result.time_cancast = mercSpellListItr->time_cancast;

					break;
			}
		}
	}

	return result;
}

MercSpell Merc::GetFirstMercSpellForSingleTargetHeal(Merc* caster) {
	MercSpell result;

	result.spellid = 0;
	result.stance = 0;
	result.type = 0;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	if(caster) {
		std::list<MercSpell> mercSpellList = GetMercSpellsForSpellEffect(caster, SpellEffect::CurrentHP);

		for (auto mercSpellListItr = mercSpellList.begin(); mercSpellListItr != mercSpellList.end();
		     ++mercSpellListItr) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if((IsRegularSingleTargetHealSpell(mercSpellListItr->spellid)
				|| IsFastHealSpell(mercSpellListItr->spellid))
				&& CheckSpellRecastTimers(caster, *mercSpellListItr)) {
					result.spellid = mercSpellListItr->spellid;
					result.stance = mercSpellListItr->stance;
					result.type = mercSpellListItr->type;
					result.slot = mercSpellListItr->slot;
					result.proc_chance = mercSpellListItr->proc_chance;
					result.time_cancast = mercSpellListItr->time_cancast;

					break;
			}
		}
	}

	return result;
}

MercSpell Merc::GetBestMercSpellForGroupHeal(Merc* caster) {
	MercSpell result;

	result.spellid = 0;
	result.stance = 0;
	result.type = 0;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	if(caster) {
		std::list<MercSpell> mercSpellList = GetMercSpellsForSpellEffect(caster, SpellEffect::CurrentHP);

		for (auto mercSpellListItr = mercSpellList.begin(); mercSpellListItr != mercSpellList.end();
		     ++mercSpellListItr) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if(IsRegularGroupHealSpell(mercSpellListItr->spellid)
				&& CheckSpellRecastTimers(caster, *mercSpellListItr)) {
					result.spellid = mercSpellListItr->spellid;
					result.stance = mercSpellListItr->stance;
					result.type = mercSpellListItr->type;
					result.slot = mercSpellListItr->slot;
					result.proc_chance = mercSpellListItr->proc_chance;
					result.time_cancast = mercSpellListItr->time_cancast;

					break;
			}
		}
	}

	return result;
}

MercSpell Merc::GetBestMercSpellForGroupHealOverTime(Merc* caster) {
	MercSpell result;

	result.spellid = 0;
	result.stance = 0;
	result.type = 0;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	if(caster) {
		std::list<MercSpell> mercHoTSpellList = GetMercSpellsForSpellEffect(caster, SpellEffect::HealOverTime);

		for (auto mercSpellListItr = mercHoTSpellList.begin(); mercSpellListItr != mercHoTSpellList.end();
		     ++mercSpellListItr) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if(IsGroupHealOverTimeSpell(mercSpellListItr->spellid)) {

				if (mercSpellListItr->spellid <= 0 || mercSpellListItr->spellid >= SPDAT_RECORDS) {
					// this is both to quit early to save cpu and to avoid casting bad spells
					// Bad info from database can trigger this incorrectly, but that should be fixed in DB, not here
					continue;
				}

				if(CheckSpellRecastTimers(caster, *mercSpellListItr)) {
					result.spellid = mercSpellListItr->spellid;
					result.stance = mercSpellListItr->stance;
					result.type = mercSpellListItr->type;
					result.slot = mercSpellListItr->slot;
					result.proc_chance = mercSpellListItr->proc_chance;
					result.time_cancast = mercSpellListItr->time_cancast;
				}

				break;
			}
		}
	}

	return result;
}

MercSpell Merc::GetBestMercSpellForGroupCompleteHeal(Merc* caster) {
	MercSpell result;

	result.spellid = 0;
	result.stance = 0;
	result.type = 0;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	if(caster) {
		std::list<MercSpell> mercSpellList = GetMercSpellsForSpellEffect(caster, SpellEffect::CompleteHeal);

		for (auto mercSpellListItr = mercSpellList.begin(); mercSpellListItr != mercSpellList.end();
		     ++mercSpellListItr) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if(IsGroupCompleteHealSpell(mercSpellListItr->spellid)
				&& CheckSpellRecastTimers(caster, *mercSpellListItr)) {
					result.spellid = mercSpellListItr->spellid;
					result.stance = mercSpellListItr->stance;
					result.type = mercSpellListItr->type;
					result.slot = mercSpellListItr->slot;
					result.proc_chance = mercSpellListItr->proc_chance;
					result.time_cancast = mercSpellListItr->time_cancast;

					break;
			}
		}
	}

	return result;
}

MercSpell Merc::GetBestMercSpellForAETaunt(Merc* caster) {
	MercSpell result;

	result.spellid = 0;
	result.stance = 0;
	result.type = 0;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	if(caster) {
		std::list<MercSpell> mercSpellList = GetMercSpellsForSpellEffect(caster, SpellEffect::Taunt);

		for (auto mercSpellListItr = mercSpellList.begin(); mercSpellListItr != mercSpellList.end();
		     ++mercSpellListItr) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if((spells[mercSpellListItr->spellid].target_type == ST_AECaster
				|| spells[mercSpellListItr->spellid].target_type == ST_AETarget
				|| spells[mercSpellListItr->spellid].target_type == ST_UndeadAE)
				&& CheckSpellRecastTimers(caster, *mercSpellListItr)) {
					result.spellid = mercSpellListItr->spellid;
					result.stance = mercSpellListItr->stance;
					result.type = mercSpellListItr->type;
					result.slot = mercSpellListItr->slot;
					result.proc_chance = mercSpellListItr->proc_chance;
					result.time_cancast = mercSpellListItr->time_cancast;

					break;
			}
		}
	}

	return result;
}

MercSpell Merc::GetBestMercSpellForTaunt(Merc* caster) {
	MercSpell result;

	result.spellid = 0;
	result.stance = 0;
	result.type = 0;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	if(caster) {
		std::list<MercSpell> mercSpellList = GetMercSpellsForSpellEffect(caster, SpellEffect::Taunt);

		for (auto mercSpellListItr = mercSpellList.begin(); mercSpellListItr != mercSpellList.end();
		     ++mercSpellListItr) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if((spells[mercSpellListItr->spellid].target_type == ST_Target)
				&& CheckSpellRecastTimers(caster, *mercSpellListItr)) {
					result.spellid = mercSpellListItr->spellid;
					result.stance = mercSpellListItr->stance;
					result.type = mercSpellListItr->type;
					result.slot = mercSpellListItr->slot;
					result.proc_chance = mercSpellListItr->proc_chance;
					result.time_cancast = mercSpellListItr->time_cancast;

					break;
			}
		}
	}

	return result;
}

MercSpell Merc::GetBestMercSpellForHate(Merc* caster) {
	MercSpell result;

	result.spellid = 0;
	result.stance = 0;
	result.type = 0;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	if(caster) {
		const std::vector<MercSpell>& mercSpellList = caster->GetMercSpells();

		// best spell is last in the list
		for (int i = mercSpellList.size() - 1; i >= 0; i--) {
			if(IsHateGeneratingSpell(mercSpellList[i].spellid)
				&& caster->CheckStance(mercSpellList[i].stance)
				&& CheckSpellRecastTimers(caster, mercSpellList[i])) {
				result = mercSpellList[i];

				break;
			}
		}
	}

	return result;
}

MercSpell Merc::GetBestMercSpellForDelayedHeal(Merc* caster) {
	MercSpell result;

	result.spellid = 0;
	result.stance = 0;
	result.type = 0;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	if(caster) {
		const std::vector<MercSpell>& mercSpellList = caster->GetMercSpells();

		// best spell is last in the list
		for (int i = mercSpellList.size() - 1; i >= 0; i--) {
			if((mercSpellList[i].type & SpellType_Heal)
				&& IsDelayedHealSpell(mercSpellList[i].spellid)
				&& caster->CheckStance(mercSpellList[i].stance)
				&& CheckSpellRecastTimers(caster, mercSpellList[i])) {
				result = mercSpellList[i];

				break;
			}
		}
	}

	return result;
}

MercSpell Merc::GetBestMercSpellForCure(Merc* caster, Mob *tar) {
	MercSpell result;
	bool spellSelected = false;

	result.spellid = 0;
	result.stance = 0;
	result.type = 0;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	if(!tar)
		return result;

	int countNeedsCured = 0;
	bool isPoisoned = tar->FindType(SpellEffect::PoisonCounter);
	bool isDiseased = tar->FindType(SpellEffect::DiseaseCounter);
	bool isCursed = tar->FindType(SpellEffect::CurseCounter);
	bool isCorrupted = tar->FindType(SpellEffect::CorruptionCounter);

	if(caster && caster->AI_HasSpells()) {
		std::list<MercSpell> cureList = GetMercSpellsBySpellType(caster, SpellType_Cure);

		if(tar->HasGroup()) {
			Group *g = tar->GetGroup();

			if(g) {
				for( int i = 0; i<MAX_GROUP_MEMBERS; i++) {
					if(g->members[i] && !g->members[i]->qglobal) {
						if(caster->GetNeedsCured(g->members[i]))
							countNeedsCured++;
					}
				}
			}
		}

		//Check for group cure first
		if(countNeedsCured > 2) {
			for (auto itr = cureList.begin(); itr != cureList.end(); ++itr) {
				MercSpell selectedMercSpell = *itr;

				if(IsGroupSpell(itr->spellid) && CheckSpellRecastTimers(caster, *itr)) {
					if(selectedMercSpell.spellid == 0)
						continue;

					if(isPoisoned && IsEffectInSpell(itr->spellid, SpellEffect::PoisonCounter)) {
						spellSelected = true;
					}
					else if(isDiseased && IsEffectInSpell(itr->spellid, SpellEffect::DiseaseCounter)) {
						spellSelected = true;
					}
					else if(isCursed && IsEffectInSpell(itr->spellid, SpellEffect::CurseCounter)) {
						spellSelected = true;
					}
					else if(isCorrupted && IsEffectInSpell(itr->spellid, SpellEffect::CorruptionCounter)) {
						spellSelected = true;
					}
					else if(IsEffectInSpell(itr->spellid, SpellEffect::DispelDetrimental)) {
						spellSelected = true;
					}

					if(spellSelected)
					{
						result.spellid = itr->spellid;
						result.stance = itr->stance;
						result.type = itr->type;
						result.slot = itr->slot;
						result.proc_chance = itr->proc_chance;
						result.time_cancast = itr->time_cancast;

						break;
					}
				}
			}
		}

		//no group cure for target- try to find single target spell
		if(!spellSelected) {
			for (auto itr = cureList.begin(); itr != cureList.end(); ++itr) {
				MercSpell selectedMercSpell = *itr;

				if(CheckSpellRecastTimers(caster, *itr)) {
					if(selectedMercSpell.spellid == 0)
						continue;

					if(isPoisoned && IsEffectInSpell(itr->spellid, SpellEffect::PoisonCounter)) {
						spellSelected = true;
					}
					else if(isDiseased && IsEffectInSpell(itr->spellid, SpellEffect::DiseaseCounter)) {
						spellSelected = true;
					}
					else if(isCursed && IsEffectInSpell(itr->spellid, SpellEffect::CurseCounter)) {
						spellSelected = true;
					}
					else if(isCorrupted && IsEffectInSpell(itr->spellid, SpellEffect::CorruptionCounter)) {
						spellSelected = true;
					}
					else if(IsEffectInSpell(itr->spellid, SpellEffect::DispelDetrimental)) {
						spellSelected = true;
					}

					if(spellSelected)
					{
						result.spellid = itr->spellid;
						result.stance = itr->stance;
						result.type = itr->type;
						result.slot = itr->slot;
						result.proc_chance = itr->proc_chance;
						result.time_cancast = itr->time_cancast;

						break;
					}
				}
			}
		}
	}

	return result;
}

MercSpell Merc::GetBestMercSpellForStun(Merc* caster) {
	MercSpell result;
	bool spellSelected = false;

	result.spellid = 0;
	result.stance = 0;
	result.type = 0;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	if(caster) {
		std::list<MercSpell> mercSpellList = GetMercSpellsForSpellEffect(caster, SpellEffect::Stun);

		for (auto mercSpellListItr = mercSpellList.begin(); mercSpellListItr != mercSpellList.end();
		     ++mercSpellListItr) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if(CheckSpellRecastTimers(caster, *mercSpellListItr)) {
				result.spellid = mercSpellListItr->spellid;
				result.stance = mercSpellListItr->stance;
				result.type = mercSpellListItr->type;
				result.slot = mercSpellListItr->slot;
				result.proc_chance = mercSpellListItr->proc_chance;
				result.time_cancast = mercSpellListItr->time_cancast;

				break;
			}
		}
	}

	return result;
}

MercSpell Merc::GetBestMercSpellForAENuke(Merc* caster, Mob* tar) {
	MercSpell result;
	bool spellSelected = false;

	result.spellid = 0;
	result.stance = 0;
	result.type = 0;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	if(caster && caster->GetOwnerSettings().area_nukes) {
		uint8 initialCastChance = 0;
		uint8 castChanceFalloff = 75;

		switch(caster->GetStance())
		{
		case Stance::AEBurn:
			initialCastChance = 50;
			break;
		case Stance::Balanced:
			initialCastChance = RuleB(Mercs, BalancedCasterAE) ? 25 : 0;
			break;
		case Stance::Burn:
			initialCastChance = 0;
			break;
		}

		//check of we even want to cast an AE nuke
		if(zone->random.Roll(initialCastChance)) {

			result = GetBestMercSpellForAERainNuke(caster, tar);

			//check if we have a spell & allow for other AE nuke types
			if(result.spellid == 0 && zone->random.Roll(castChanceFalloff)) {

				result = GetBestMercSpellForPBAENuke(caster, tar);

				//check if we have a spell & allow for other AE nuke types
				if(result.spellid == 0 && zone->random.Roll(castChanceFalloff)) {

					result = GetBestMercSpellForTargetedAENuke(caster, tar);
				}
			}
		}
	}

	return result;
}

MercSpell Merc::GetBestMercSpellForTargetedAENuke(Merc* caster, Mob* tar) {
	MercSpell result;
	int castChance = 50;            //used to cycle through multiple spells (first has 50% overall chance, 2nd has 25%, etc.)
	bool spellSelected = false;

	result.spellid = 0;
	result.stance = 0;
	result.type = 0;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	if (!caster) {
		return result;
	}

	std::list<MercSpell> mercSpellList = GetMercSpellsBySpellType(caster, SpellType_Nuke);

	for (auto mercSpellListItr = mercSpellList.begin(); mercSpellListItr != mercSpellList.end();
		    ++mercSpellListItr) {
		// Assuming all the spells have been loaded into this list by level and in descending order
		if(IsAENukeSpell(mercSpellListItr->spellid) && !IsAERainNukeSpell(mercSpellListItr->spellid)
			&& !IsPBAENukeSpell(mercSpellListItr->spellid) && CheckSpellRecastTimers(caster, *mercSpellListItr)) {
				uint8 numTargets = 0;
				if(CheckAENuke(caster, tar, mercSpellListItr->spellid, numTargets)) {
					if(zone->random.Roll(castChance)) {
						result.spellid = mercSpellListItr->spellid;
						result.stance = mercSpellListItr->stance;
						result.type = mercSpellListItr->type;
						result.slot = mercSpellListItr->slot;
						result.proc_chance = mercSpellListItr->proc_chance;
						result.time_cancast = mercSpellListItr->time_cancast;
					}
				}

				break;
		}
	}

	return result;
}

MercSpell Merc::GetBestMercSpellForPBAENuke(Merc* caster, Mob* tar) {
	MercSpell result;
	int castChance = 50;            //used to cycle through multiple spells (first has 50% overall chance, 2nd has 25%, etc.)
	bool spellSelected = false;

	result.spellid = 0;
	result.stance = 0;
	result.type = 0;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	if (!caster) {
		return result;
	}

	std::list<MercSpell> mercSpellList = GetMercSpellsBySpellType(caster, SpellType_Nuke);

	for (auto mercSpellListItr = mercSpellList.begin(); mercSpellListItr != mercSpellList.end();
		    ++mercSpellListItr) {
		// Assuming all the spells have been loaded into this list by level and in descending order
		if(IsPBAENukeSpell(mercSpellListItr->spellid) && CheckSpellRecastTimers(caster, *mercSpellListItr)) {
			uint8 numTargets = 0;
			if(CheckAENuke(caster, caster, mercSpellListItr->spellid, numTargets)) {
				if(zone->random.Roll(castChance)) {
					result.spellid = mercSpellListItr->spellid;
					result.stance = mercSpellListItr->stance;
					result.type = mercSpellListItr->type;
					result.slot = mercSpellListItr->slot;
					result.proc_chance = mercSpellListItr->proc_chance;
					result.time_cancast = mercSpellListItr->time_cancast;
				}
			}

			break;
		}
	}

	return result;
}

MercSpell Merc::GetBestMercSpellForAERainNuke(Merc* caster, Mob* tar) {
	MercSpell result;
	int castChance = 50;            //used to cycle through multiple spells (first has 50% overall chance, 2nd has 25%, etc.)
	bool spellSelected = false;

	result.spellid = 0;
	result.stance = 0;
	result.type = 0;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	if (!caster) {
		return result;
	}

	std::list<MercSpell> mercSpellList = GetMercSpellsBySpellType(caster, SpellType_Nuke);

	for (auto mercSpellListItr = mercSpellList.begin(); mercSpellListItr != mercSpellList.end();
		    ++mercSpellListItr) {
		// Assuming all the spells have been loaded into this list by level and in descending order
		if(IsAERainNukeSpell(mercSpellListItr->spellid) && zone->random.Roll(castChance) && CheckSpellRecastTimers(caster, *mercSpellListItr)) {
			uint8 numTargets = 0;
			if(CheckAENuke(caster, tar, mercSpellListItr->spellid, numTargets)) {
				result.spellid = mercSpellListItr->spellid;
				result.stance = mercSpellListItr->stance;
				result.type = mercSpellListItr->type;
				result.slot = mercSpellListItr->slot;
				result.proc_chance = mercSpellListItr->proc_chance;
				result.time_cancast = mercSpellListItr->time_cancast;
			}

			break;
		}
	}

	return result;
}

MercSpell Merc::GetBestMercSpellForNuke(Merc* caster) {
	MercSpell result;

	result.spellid = 0;
	result.stance = 0;
	result.type = 0;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	if(caster) {
		std::list<MercSpell> mercSpellList = GetMercSpellsBySpellType(caster, SpellType_Nuke);

		// Balanced mercs step down one nuke every few casts to save mana; the rest use the best nuke off cooldown
		bool use_cheaper_nuke = false;

		if(RuleB(Mercs, StanceBehavior) && caster->GetStance() == Stance::Balanced) {
			caster->nuke_cast_count++;
			use_cheaper_nuke = caster->nuke_cast_count % NukesPerCheaperNuke == 0;
		}

		MercSpell best_nuke = result;

		for (auto mercSpellListItr = mercSpellList.begin(); mercSpellListItr != mercSpellList.end();
		     ++mercSpellListItr) {
			// Assuming all the spells have been loaded into this list by level and in descending order
			if(!IsPureNukeSpell(mercSpellListItr->spellid) || IsAENukeSpell(mercSpellListItr->spellid)
				|| !CheckSpellRecastTimers(caster, *mercSpellListItr)) {
				continue;
			}

			if(use_cheaper_nuke) {
				best_nuke = *mercSpellListItr;
				use_cheaper_nuke = false;

				continue;
			}

			result = *mercSpellListItr;

			break;
		}

		// Only one nuke was available, so cast it
		if(result.spellid == 0) {
			result = best_nuke;
		}
	}

	return result;
}

MercSpell Merc::GetBestMercSpellForNukeByTargetResists(Merc* caster, Mob* target) {
	MercSpell result;
	bool spellSelected = false;

	result.spellid = 0;
	result.stance = 0;
	result.type = 0;
	result.slot = 0;
	result.proc_chance = 0;
	result.time_cancast = 0;

	if(!target)
		return result;

	if(caster) {
		const int lureResisValue = -100;
		const int maxTargetResistValue = 300;
		bool selectLureNuke = false;

		if((target->GetMR() > maxTargetResistValue) && (target->GetCR() > maxTargetResistValue) && (target->GetFR() > maxTargetResistValue))
			selectLureNuke = true;

		std::list<MercSpell> mercSpellList = GetMercSpellsBySpellType(caster, SpellType_Nuke);

		for (auto mercSpellListItr = mercSpellList.begin(); mercSpellListItr != mercSpellList.end();
		     ++mercSpellListItr) {
			// Assuming all the spells have been loaded into this list by level and in descending order

			if(IsPureNukeSpell(mercSpellListItr->spellid) && !IsAENukeSpell(mercSpellListItr->spellid) && CheckSpellRecastTimers(caster, *mercSpellListItr)) {
				if(selectLureNuke && (spells[mercSpellListItr->spellid].resist_difficulty < lureResisValue)) {
					spellSelected = true;
				}
				else {
					if(((target->GetMR() < target->GetCR()) || (target->GetMR() < target->GetFR())) && (GetSpellResistType(mercSpellListItr->spellid) == RESIST_MAGIC)
						&& (spells[mercSpellListItr->spellid].resist_difficulty > lureResisValue))
					{
						spellSelected = true;
					}
					else if(((target->GetCR() < target->GetMR()) || (target->GetCR() < target->GetFR())) && (GetSpellResistType(mercSpellListItr->spellid) == RESIST_COLD)
						&& (spells[mercSpellListItr->spellid].resist_difficulty > lureResisValue))
					{
						spellSelected = true;
					}
					else if(((target->GetFR() < target->GetCR()) || (target->GetFR() < target->GetMR())) && (GetSpellResistType(mercSpellListItr->spellid) == RESIST_FIRE)
						&& (spells[mercSpellListItr->spellid].resist_difficulty > lureResisValue))
					{
						spellSelected = true;
					}
				}
			}

			if(spellSelected) {
				result.spellid = mercSpellListItr->spellid;
				result.stance = mercSpellListItr->stance;
				result.type = mercSpellListItr->type;
				result.slot = mercSpellListItr->slot;
				result.proc_chance = mercSpellListItr->proc_chance;
				result.time_cancast = mercSpellListItr->time_cancast;

				break;
			}
		}
	}

	return result;
}

bool Merc::GetNeedsCured(Mob *tar) {
	bool needCured = false;

	if(tar) {
		if(tar->FindType(SpellEffect::PoisonCounter) || tar->FindType(SpellEffect::DiseaseCounter) || tar->FindType(SpellEffect::CurseCounter) || tar->FindType(SpellEffect::CorruptionCounter)) {
			uint32 buff_count = tar->GetMaxTotalSlots();
			int buffsWithCounters = 0;
			needCured = true;

			for (unsigned int j = 0; j < buff_count; j++) {
				if (IsValidSpell(tar->GetBuffs()[j].spellid)) {
					if(CalculateCounters(tar->GetBuffs()[j].spellid) > 0) {
						buffsWithCounters++;

						if(buffsWithCounters == 1 && (tar->GetBuffs()[j].ticsremaining < 2 || (int32)((tar->GetBuffs()[j].ticsremaining * 6) / tar->GetBuffs()[j].counters) < 2)) {
							// Spell has ticks remaining but may have too many counters to cure in the time remaining;
							// We should try to just wait it out. Could spend entire time trying to cure spell instead of healing, buffing, etc.
							// Since this is the first buff with counters, don't try to cure. Cure spell will be wasted, as cure will try to
							// remove counters from the first buff that has counters remaining.
							needCured = false;
							break;
						}
					}
				}
			}
		}
	}

	return needCured;
}

// Tells the owner what the merc is doing, unless they turned the messages off with $merc announce
void Merc::MercSay(MercChatter kind, const std::string& message, bool to_group) {
	Client* const owner = GetMercenaryOwner();

	if(!owner || !owner->GetMercSettings().announce) {
		return;
	}

	uint32& next_time = chatter_next_time[static_cast<size_t>(kind)];
	const uint32 now = Timer::GetCurrentTime();

	if(kind != MercChatter::Heal && kind != MercChatter::Assist) {
		if(now < next_time) {
			return;
		}

		// Low mana is said once per fight; AI_Process rearms it when the merc leaves combat
		next_time = kind == MercChatter::LowMana ? UINT32_MAX : now + ChatterRepeatMs;
	}

	Group* const group = to_group ? GetGroup() : nullptr;

	if(group) {
		group->GroupMessage(this, Language::CommonTongue, Language::MaxValue, message.c_str());
	}
	else {
		owner->Message(Chat::White, "%s", message.c_str());
	}
}

bool Merc::UseDiscipline(int32 spell_id, int32 target) {
	// Dont let client waste a reuse timer if they can't use the disc
	if (IsStunned() || IsFeared() || IsMezzed() || IsAmnesiad())
	{
		return(false);
	}

	//make sure we can use it..
	if(!IsValidSpell(spell_id)) {
		return(false);
	}

	const SPDat_Spell_Struct &spell = spells[spell_id];

	if(spell.recast_time > 0)
	{
		if(CheckDisciplineRecastTimers(this, spell_id, spells[spell_id].timer_id)) {
			if(spells[spell_id].timer_id > 0) {
				SetDisciplineRecastTimer(spells[spell_id].timer_id, spell_id, spell.recast_time);
			}

			SetSpellTimeCanCast(spell_id, spells[spell_id].recast_time);
		}
		else {
			return(false);
		}
	}

	if(GetEndurance() > spell.endurance_cost) {
		SetEndurance(GetEndurance() - spell.endurance_cost);
	} else {
		//too fatigued to use this skill right now.
		return(false);
	}

	if(IsCasting())
		InterruptSpell();

	CastSpell(spell_id, target, EQ::spells::CastingSlot::Discipline);

	return(true);
}

void Merc::SetSpellRecastTimer(uint16 timer_id, int32 spellid, uint32 recast_delay) {
	if(timer_id > 0) {
		MercTimer timer;
		timer.timerid = timer_id;
		timer.timertype = 1;
		timer.spellid = spellid;
		timer.time_cancast = Timer::GetCurrentTime() + recast_delay;
		timers[timer_id] = timer;
	}
}

int32 Merc::GetSpellRecastTimer(Merc *caster, uint16 timer_id) {
	int32 result = 0;
	if(caster && timer_id > 0) {
		if(caster->timers.find(timer_id) != caster->timers.end()) {
			result = caster->timers[timer_id].time_cancast;
		}
	}
	return result;
}

bool Merc::CheckSpellRecastTimers(Merc *caster, const MercSpell& merc_spell) {
	if(caster && merc_spell.spellid > 0 && merc_spell.time_cancast < Timer::GetCurrentTime()) { //checks spell recast
		if(GetSpellRecastTimer(caster, spells[merc_spell.spellid].timer_id) < Timer::GetCurrentTime()) { //checks for spells on the same timer
			return true; //can cast spell
		}
	}
	return false;
}

void Merc::SetDisciplineRecastTimer(uint16 timer_id, int32 spellid, uint32 recast_delay) {
	if(timer_id > 0) {
		MercTimer timer;
		timer.timerid = timer_id;
		timer.timertype = 2;
		timer.spellid = spellid;
		timer.time_cancast = Timer::GetCurrentTime() + recast_delay;
		timers[timer_id] = timer;
	}
}

int32 Merc::GetDisciplineRecastTimer(Merc *caster, uint16 timer_id) {
	int32 result = 0;
	if(caster && timer_id > 0) {
		if(caster->timers.find(timer_id) != caster->timers.end()) {
			result = caster->timers[timer_id].time_cancast;
		}
	}
	return result;
}

int32 Merc::GetDisciplineRemainingTime(Merc *caster, uint16 timer_id) {
	int32 result = 0;
	if(caster && timer_id > 0) {
		int32 time_cancast = GetDisciplineRecastTimer(caster, timer_id);
		if(time_cancast > Timer::GetCurrentTime())
			result = time_cancast - Timer::GetCurrentTime();
	}
	return result;
}

bool Merc::CheckDisciplineRecastTimers(Merc *caster, int32 spell_id, uint16 timer_id) {
	if(caster) {
		MercSpell mercSpell = GetMercSpellBySpellID(caster, spell_id);
		if(mercSpell.spellid > 0 && mercSpell.time_cancast < Timer::GetCurrentTime()) { //checks spell recast
			if(timer_id > 0 && !(GetDisciplineRecastTimer(caster, timer_id) < Timer::GetCurrentTime())) { //checks for spells on the same timer
				return false; //can't cast spell
			}
			return true;
		}
	}
	return false;
}

void Merc::SetSpellTimeCanCast(int32 spellid, uint32 recast_delay) {
	for (int i = 0; i < merc_spells.size(); i++) {
		if(merc_spells[i].spellid == spellid) {
			merc_spells[i].time_cancast = Timer::GetCurrentTime() + recast_delay;
		}
	}
}

bool Merc::CheckTaunt() {
	Mob* tar = GetTarget();
	//Only taunt if we are not top on target's hate list
	//This ensures we have taunt available to regain aggro if needed
	if(tar && tar->GetHateTop() && tar->GetHateTop() != this) {
		return true;
	}
	return false;
}

bool Merc::CheckAETaunt() {
	//need to check area for mobs needing taunted
	MercSpell mercSpell = GetBestMercSpellForAETaunt(this);
	Group* g = GetGroup();

	if(mercSpell.spellid == 0 || !g) {
		return false;
	}

	const int spell_range = GetActSpellRange(mercSpell.spellid, spells[mercSpell.spellid].range);
	const int spell_range_squared = spell_range * spell_range;
	int fighting_count = 0;
	bool untargeted_mob = false;

	for (const auto& [mob_id, mob] : GetNearbyMobs(spell_range)) {
		if(!mob || !mob->IsNPC() || mob->IsMerc() || !IsAttackAllowed(mob)) {
			continue;
		}

		if(DistanceSquaredNoZ(m_Position, mob->GetPosition()) > spell_range_squared) {
			continue;
		}

		if(mob->IsMezzed()) {
			if(SpellBreaksMez(mercSpell.spellid)) {
				return false;
			}

			continue;
		}

		NPC* npc = mob->CastToNPC();
		bool fighting_group = false;

		for(int i = 0; i < MAX_GROUP_MEMBERS; i++) {
			Mob* member = g->members[i];

			if(member && npc->IsOnHatelist(member)) {
				fighting_group = true;

				if(GetTarget() != npc && member->GetTarget() != npc) {
					untargeted_mob = true;
				}
			}
		}

		if(fighting_group) {
			fighting_count++;
		}
	}

	if(untargeted_mob && fighting_count >= std::max(1, RuleI(Mercs, AEMinTargets))) {
		LogMercenaries("[{}]: Attempting AE Taunt", GetCleanName());
		return true;
	}

	return false;
}

Corpse* Merc::GetGroupMemberCorpse() {
	Corpse* corpse = nullptr;

	if(HasGroup()) {
		Group* g = GetGroup();

		if(g) {
			for(int i = 0; i < MAX_GROUP_MEMBERS; i++) {
				if(g->members[i] && g->members[i]->IsClient()) {
					corpse = entity_list.GetCorpseByOwnerWithinRange(g->members[i]->CastToClient(), this, RuleI(Mercs, ResurrectRadius));

					if(corpse && !corpse->IsRezzed()) {
						return corpse;
					}
				}
			}
		}
	}
	return 0;
}

bool Merc::TryHide() {
	if(GetClass() != MELEEDPS) {
		return false;
	}

	//don't hide if already hidden
	if(hidden == true) {
		return false;
	}

	auto outapp = new EQApplicationPacket(OP_SpawnAppearance, sizeof(SpawnAppearance_Struct));
	SpawnAppearance_Struct* sa_out = (SpawnAppearance_Struct*)outapp->pBuffer;
	sa_out->spawn_id = GetID();
	sa_out->type = 0x03;
	sa_out->parameter = 1;
	entity_list.QueueClients(this, outapp, true);
	safe_delete(outapp);
	hidden = true;

	return true;
}

//Checks if Merc still has confidence. Can be checked to begin fleeing, or to regain confidence after confidence loss - true = confident, false = confidence loss
bool Merc::CheckConfidence() {
	if (!RuleB(Mercs, ConfidenceFlee)) {
		return true;
	}

	bool result = true;
	int ConfidenceLossChance = 0;
	float ConfidenceCheck = 0;
	int ConfidenceRating = 2 * GetProficiencyID();

	std::list<NPC*> npc_list;
	entity_list.GetNPCList(npc_list);

	for (auto itr = npc_list.begin(); itr != npc_list.end(); ++itr) {
		NPC* mob = *itr;
		float ConRating = 1.0;
		int CurrentCon = 0;

		if(!mob) continue;

		if(!mob->IsEngaged()) continue;

		if(mob->IsFeared() || mob->IsMezzed() || mob->IsStunned() || mob->IsRooted() || mob->IsCharmed()) continue;

		if(!mob->CheckAggro(this)) continue;

		float AggroRange = mob->GetAggroRange();

		// Square it because we will be using DistNoRoot

		AggroRange = AggroRange * AggroRange;

		if(DistanceSquared(m_Position, mob->GetPosition()) > AggroRange) continue;

		CurrentCon = GetLevelCon(mob->GetLevel());
		switch(CurrentCon) {


					case ConsiderColor::Gray: {
						ConRating = 0;
						break;
					}

					case ConsiderColor::Green: {
						ConRating = 0.1;
						break;
									}

					case ConsiderColor::LightBlue: {
						ConRating = 0.2;
						break;
										}

					case ConsiderColor::DarkBlue: {
						ConRating = 0.6;
						break;
								   }

					case ConsiderColor::White: {
						ConRating = 1.0;
						break;
									}

					case ConsiderColor::Yellow: {
						ConRating = 1.2;
						break;
									 }

					case ConsiderColor::Red: {
						ConRating = 1.5;
						break;
								  }

					default: {
						ConRating = 0;
						break;
							 }
		}

		ConfidenceCheck += ConRating;
	}

	if(ConfidenceRating < ConfidenceCheck) {
		ConfidenceLossChance = 25 - ( 5 * (GetTierID() - 1));
	}

	if(zone->random.Roll(ConfidenceLossChance)) {
		result = false;
	}

	return result;
}

void Merc::MercMeditate(bool isSitting) {
	// Don't try to meditate if engaged or dead
	if (IsEngaged() || GetAppearance() == eaDead)
	{
		return;
	}
	if (isSitting) {
		// Something is about to hit the merc: do not sit, and get up if it already did
		if (IsTargetedByHostile()) {
			if (IsSitting()) {
				Stand();
			}

			return;
		}

		// Sit below the threshold and stay down until every bar is full, so the merc does not
		// stand and sit again as regeneration ticks across the line. Endurance counts because
		// damage dealers only regain it at the rest rate; a merc with no endurance pool is skipped.
		const bool has_endurance = GetMaxEndurance() > 0;

		if (GetManaRatio() < RestBelowPercent || GetHPRatio() < RestBelowPercent ||
			(has_endurance && GetEndurancePercent() < RestBelowPercent))
		{
			if(!IsSitting()) {
				Sit();
			}
		}
		else if (IsSitting() && GetManaRatio() >= 100.0f && GetHPRatio() >= 100.0f &&
			(!has_endurance || GetEndurance() >= GetMaxEndurance()))
		{
			Stand();
		}
	}
	else
	{
		if (IsSitting()) {
			Stand();
		}
	}
}


void Merc::Sit() {
	if(IsMoving()) {
		moved = false;
		// SetHeading(CalculateHeadingToTarget(GetTarget()->GetX(), GetTarget()->GetY()));
		SentPositionPacket(0.0f, 0.0f, 0.0f, 0.0f, 0);
		SetMoving(false);
	}

	SetAppearance(eaSitting);
}

void Merc::Stand() {
	SetAppearance(eaStanding);
}

bool Merc::IsSitting() const {
	bool result = false;

	if(GetAppearance() == eaSitting && !IsMoving())
		result = true;

	return result;
}

bool Merc::IsStanding() {
	bool result = false;

	if(GetAppearance() == eaStanding)
		result = true;

	return result;
}

float Merc::GetMaxMeleeRangeToTarget(Mob* target) {
	float result = 0;

	if (target) {
		float size_mod = GetSize();
		float other_size_mod = target->GetSize();

		if (GetRace() == Race::LavaDragon || GetRace() == Race::Wurm || GetRace() == Race::GhostDragon) //For races with a fixed size
		{
			size_mod = 60.0f;
		} else if (size_mod < 6.0) {
			size_mod = 8.0f;
		}

		if (target->GetRace() == Race::LavaDragon || target->GetRace() == Race::Wurm || target->GetRace() == Race::GhostDragon) //For races with a fixed size
		{
			other_size_mod = 60.0f;
		} else if (other_size_mod < 6.0) {
			other_size_mod = 8.0f;
		}

		if (other_size_mod > size_mod) {
			size_mod = other_size_mod;
		}

		// this could still use some work, but for now it's an improvement....

		if (size_mod > 29) {
			size_mod *= size_mod;
		} else if (size_mod > 19) {
			size_mod *= size_mod * 2;
		} else {
			size_mod *= size_mod * 4;
		}

		// prevention of ridiculously sized hit boxes
		if (size_mod > 10000)
			size_mod = size_mod / 7;

		result = size_mod;
	}

	return result;
}

void Merc::DoClassAttacks(Mob *target) {
	if(target == nullptr)
		return; //gotta have a target for all these

	bool ca_time = classattack_timer.Check(false);

	//only check attack allowed if we are going to do something
	if(ca_time && !IsAttackAllowed(target))
		return;

	if(!ca_time)
		return;

	float HasteModifier = GetHaste() * 0.01f;

	int level = GetLevel();
	int reuse = TauntReuseTime * 1000;      //make this very long since if they dont use it once, they prolly never will
	bool did_attack = false;
	//class specific stuff...
	switch(GetClass()) {
			case MELEEDPS:
				if(level >= 10) {
					reuse = BackstabReuseTime * 1000;
					TryBackstab(target, reuse);
					did_attack = true;
				}
				break;
			case TANK:{
				// Tanks taunt like a warrior when they are not on top of the mob's hate
				if(!IsAssistStance() && target->IsNPC() && CheckTaunt() && taunt_timer.Check()) {
					Taunt(target->CastToNPC(), false);
				}

				if(level >= RuleI(Combat, NPCBashKickLevel)){
					if(zone->random.Int(0, 100) > 25) //tested on live, warrior mobs both kick and bash, kick about 75% of the time, casting doesn't seem to make a difference.
					{
						DoAnim(animKick, 0, false);
						int64 dmg = GetBaseSkillDamage(EQ::skills::SkillKick);

						if (GetWeaponDamage(target, (const EQ::ItemData*)nullptr) <= 0)
							dmg = DMG_INVULNERABLE;

						reuse = KickReuseTime * 1000;
						DoSpecialAttackDamage(target, EQ::skills::SkillKick, dmg, 1, -1, reuse);
						did_attack = true;
					}
					else
					{
						DoAnim(animTailRake, 0, false);
						int64 dmg = GetBaseSkillDamage(EQ::skills::SkillBash);

						if (GetWeaponDamage(target, (const EQ::ItemData*)nullptr) <= 0)
							dmg = DMG_INVULNERABLE;

						reuse = BashReuseTime * 1000;
						DoSpecialAttackDamage(target, EQ::skills::SkillBash, dmg, 1, -1, reuse);
						did_attack = true;
					}
				}
				break;
					  }
	}

	classattack_timer.Start(reuse / HasteModifier);
}

bool Merc::Attack(Mob* other, int Hand, bool bRiposte, bool IsStrikethrough, bool IsFromSpell, ExtraAttackOptions *opts)
{
	if (!other) {
		SetTarget(nullptr);
		LogError("A null Mob object was passed to Merc::Attack() for evaluation!");
		return false;
	}

	return NPC::Attack(other, Hand, bRiposte, IsStrikethrough, IsFromSpell, opts);
}

void Merc::Damage(Mob* other, int64 damage, int32 spell_id, EQ::skills::SkillType attack_skill, bool avoidable, int8 buffslot, bool iBuffTic, eSpecialAttacks special)
{
	if(IsDead() || IsCorpse())
		return;

	if(spell_id==0)
		spell_id = SPELL_UNKNOWN;

	NPC::Damage(other, damage, spell_id, attack_skill, avoidable, buffslot, iBuffTic, special);

	//Not needed since we're using NPC damage.
	//CommonDamage(other, damage, spell_id, attack_skill, avoidable, buffslot, iBuffTic);
}

bool Merc::FindTarget() {
	bool found = false;
	Mob* target = hate_list.GetMobWithMostHateOnList(this, nullptr, true);

	if(target && IsAttackAllowed(target)) {
		found = true;
		SetTarget(target);
	}

	return found;
}

void Merc::SetTarget(Mob* mob) {
	NPC::SetTarget(mob);
}

Mob* Merc::GetOwnerOrSelf() {
	Mob* Result = nullptr;

	if(GetMercenaryOwner())
		Result = GetMercenaryOwner();
	else
		Result = this;

	return Result;
}

bool Merc::Death(Mob* killer_mob, int64 damage, int32 spell, EQ::skills::SkillType attack_skill, KilledByTypes killed_by, bool is_buff_tic)
{
	if (!NPC::Death(killer_mob, damage, spell, attack_skill, killed_by, is_buff_tic)) {
		return false;
	}

	Save();

	// If client is in zone, suspend merc, else depop it.
	if (!Suspend()) {
		Depop();
	}

	return true;
}

Client* Merc::GetMercenaryOwner() {
	Client* mercOwner = nullptr;

	if(GetOwner())
	{
		if(GetOwner()->IsClient())
		{
			mercOwner = GetOwner()->CastToClient();
		}
	}

	return mercOwner;
}

Mob* Merc::GetOwner() {
	Mob* Result = nullptr;

	Result = entity_list.GetMob(GetOwnerID());

	if(!Result) {
		SetOwnerID(0);
	}

	return Result;
}

const char* Merc::GetRandomName(){
	// creates up to a 10 char name
	static char name[17];
	char vowels[18]="aeiouyaeiouaeioe";
	char cons[48]="bcdfghjklmnpqrstvwxzybcdgklmnprstvwbcdgkpstrkd";
	char rndname[17]="\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0";
	char paircons[33]="ngrkndstshthphsktrdrbrgrfrclcr";
	bool valid = false;

	while(!valid) {
		int rndnum=zone->random.Int(0, 75),n=1;
		bool dlc=false;
		bool vwl=false;
		bool dbl=false;
		if (rndnum>63)
		{       // rndnum is 0 - 75 where 64-75 is cons pair, 17-63 is cons, 0-16 is vowel
			rndnum=(rndnum-61)*2;   // name can't start with "ng" "nd" or "rk"
			rndname[0]=paircons[rndnum];
			rndname[1]=paircons[rndnum+1];
			n=2;
		}
		else if (rndnum>16)
		{
			rndnum-=17;
			rndname[0]=cons[rndnum];
		}
		else
		{
			rndname[0]=vowels[rndnum];
			vwl=true;
		}
		int namlen=zone->random.Int(5, 10);
		for (int i=n;i<namlen;i++)
		{
			dlc=false;
			if (vwl)        //last char was a vowel
			{                       // so pick a cons or cons pair
				rndnum=zone->random.Int(0, 62);
				if (rndnum>46)
				{       // pick a cons pair
					if (i>namlen-3) // last 2 chars in name?
					{       // name can only end in cons pair "rk" "st" "sh" "th" "ph" "sk" "nd" or "ng"
						rndnum=zone->random.Int(0, 7)*2;
					}
					else
					{       // pick any from the set
						rndnum=(rndnum-47)*2;
					}
					rndname[i]=paircons[rndnum];
					rndname[i+1]=paircons[rndnum+1];
					dlc=true;       // flag keeps second letter from being doubled below
					i+=1;
				}
				else
				{       // select a single cons
					rndname[i]=cons[rndnum];
				}
			}
			else
			{               // select a vowel
				rndname[i]=vowels[zone->random.Int(0, 16)];
			}
			vwl=!vwl;
			if (!dbl && !dlc)
			{       // one chance at double letters in name
				if (!zone->random.Int(0, i+9))     // chances decrease towards end of name
				{
					rndname[i+1]=rndname[i];
					dbl=true;
					i+=1;
				}
			}
		}

		rndname[0]=toupper(rndname[0]);

		if(!database.CheckNameFilter(rndname)) {
			valid = false;
		}
		else if(rndname[0] < 'A' && rndname[0] > 'Z') {
			//name must begin with an upper-case letter.
			valid = false;
		}
		else if (!database.IsNameUsed(rndname)) {
			valid = true;
		}
		else {
			valid = false;
		}
	}

	memset(name, 0, 17);
	strcpy(name, rndname);
	return name;
}

bool Merc::LoadMercenarySpells() {
	// loads mercs spells into list
	merc_spells.clear();

	// Called again on level-up; clear the permanent procs so they do not stack
	for (int i = 0; i < m_max_procs; i++) {
		PermaProcs[i].spellID         = SPELL_UNKNOWN;
		PermaProcs[i].chance          = 0;
		PermaProcs[i].base_spellID    = SPELL_UNKNOWN;
		PermaProcs[i].level_override  = -1;
		PermaProcs[i].proc_reuse_time = 0;
	}

	const std::list<MercSpellEntry>& spellList = zone->merc_spells_list[GetClass()];

	if (spellList.size() == 0) {
		AIautocastspell_timer->Disable();
		return false;
	}

	uint8 proficiency_id = GetProficiencyID();
	int16 attack_proc_spell = -1;
	int8 proc_chance = 0;

	std::vector<const MercSpellEntry*> usable_entries;

	for (const auto& entry : spellList) {
		if (proficiency_id == entry.proficiencyid && GetLevel() >= entry.minlevel && GetLevel() <= entry.maxlevel && entry.spellid > 0) {
			usable_entries.push_back(&entry);
		}
	}

	// The pickers walk merc_spells from the back and take the first match, so the best spell
	// (highest minlevel, then lowest slot) must be last. A stable sort keeps the database order for ties.
	std::stable_sort(usable_entries.begin(), usable_entries.end(), [](const MercSpellEntry* a, const MercSpellEntry* b) {
		if (a->minlevel != b->minlevel) {
			return a->minlevel < b->minlevel;
		}

		return a->slot > b->slot;
	});

	for (const auto* entry : usable_entries) {
		MercSpell mercSpell;

		mercSpell.spellid = entry->spellid;
		mercSpell.type = entry->type;
		mercSpell.stance = entry->stance;
		mercSpell.slot = entry->slot;
		mercSpell.proc_chance = entry->proc_chance;
		mercSpell.time_cancast = 0;

		merc_spells.push_back(mercSpell);

		if (entry->proc_chance > 0) {
			AddProcToWeapon(entry->spellid, true, entry->proc_chance);
		}
	}

	if (merc_spells.empty())
		AIautocastspell_timer->Disable();
	else {
		HasAISpell = true;
		AIautocastspell_timer->Trigger();
	}

	return true;
}

bool Merc::Save() {

	if(database.SaveMercenary(this)){
		return true;
	}

	return false;
}

Merc* Merc::LoadMercenary(Client *c, MercTemplate* merc_template, uint32 merchant_id, bool updateFromDB) {

	if(c)
	{
		if(c->GetMercenaryID())
		{
			merc_template = zone->GetMercTemplate(c->GetMercInfo().MercTemplateID);
		}
	}

	//get mercenary data
	if(merc_template)
	{
		//TODO: Maybe add a way of updating client merc stats in a seperate function? like, for example, on leveling up.

		const NPCType* npc_type_to_copy = nullptr;
		if (c) {
			npc_type_to_copy = content_db.GetMercenaryType(merc_template->MercNPCID, merc_template->RaceID, c->GetLevel());
		}

		if(npc_type_to_copy != nullptr)
		{
			//This is actually a very terrible method of assigning stats, and should be changed at some point. See the comment in merc's deconstructor.
			auto npc_type = new NPCType;
			memset(npc_type, 0, sizeof(NPCType));
			memcpy(npc_type, npc_type_to_copy, sizeof(NPCType));
			if(c && !updateFromDB)
			{
				if(c->GetMercInfo().merc_name[0] == 0)
				{
					snprintf(c->GetMercInfo().merc_name, 64, "%s", GetRandomName()); //sanity check.
				}
				snprintf(npc_type->name, 64, "%s", c->GetMercInfo().merc_name);
			}

			npc_type->race = merc_template->RaceID;

			// Use the Gender and Size of the Merchant if possible
			uint8 tmpgender = Gender::Male;
			float tmpsize = 6.0f;
			if(merchant_id > 0)
			{
				NPC* tar = entity_list.GetNPCByID(merchant_id);
				if(tar)
				{
					tmpgender = tar->GetGender();
					tmpsize = tar->GetSize();
				}
				else
				{
					tmpgender = Mob::GetDefaultGender(npc_type->race, c->GetMercInfo().Gender);
				}

			}
			else
			{
				tmpgender = c->GetMercInfo().Gender;
				tmpsize = c->GetMercInfo().MercSize;
			}

			std::string tmp_lastname = c->GetName();
			tmp_lastname += "'s Mercenary";

			// not sure what to do if too long
			if (tmp_lastname.length() < sizeof(npc_type->lastname))
				strn0cpy(npc_type->lastname, tmp_lastname.c_str(), sizeof(npc_type->lastname));
			npc_type->gender = tmpgender;
			npc_type->size = tmpsize;
			npc_type->loottable_id = 0; // Loottable has to be 0, otherwise we'll be leavin' some corpses!
			npc_type->npc_id = 0; //NPC ID has to be 0, otherwise db gets all confuzzled.
			npc_type->class_ = merc_template->ClassID;
			npc_type->maxlevel = 0; //We should hard-set this to override scalerate's functionality in the NPC class when it is constructed.
			npc_type->no_target_hotkey = 1;

			auto merc = new Merc(npc_type, c->GetX(), c->GetY(), c->GetZ(), 0);
			merc->GiveNPCTypeData(npc_type); // for clean up, works a bit like pets

			if(merc)
			{
				bool restore_saved_stats = false;

				merc->SetMercData( merc_template->MercTemplateID );
				database.LoadMercenaryEquipment(merc);
				merc->UpdateMercStats(c, true);

				if(updateFromDB)
				{
					database.LoadCurrentMercenary(c);

					merc->SetMercID(c->GetMercInfo().mercid);
					snprintf(merc->name, 64, "%s", c->GetMercInfo().merc_name);
					merc->SetSuspended(c->GetMercInfo().IsSuspended);
					merc->gender = c->GetMercInfo().Gender;
					merc->size = c->GetMercInfo().MercSize;
					merc->luclinface = c->GetMercInfo().face;
					merc->hairstyle = c->GetMercInfo().luclinHairStyle;
					merc->haircolor = c->GetMercInfo().luclinHairColor;
					merc->eyecolor1 = c->GetMercInfo().luclinEyeColor;
					merc->eyecolor2 = c->GetMercInfo().luclinEyeColor2;
					merc->beardcolor = c->GetMercInfo().luclinBeardColor;
					merc->beard = c->GetMercInfo().luclinBeard;
					merc->drakkin_heritage = c->GetMercInfo().drakkinHeritage;
					merc->drakkin_tattoo = c->GetMercInfo().drakkinTattoo;
					merc->drakkin_details = c->GetMercInfo().drakkinDetails;
					restore_saved_stats = true;
				}
				else
				{
					// Give Random Features to newly hired Mercs
					merc->RandomizeFeatures(false, true);
				}

				if(merc->GetMercenaryID()) {
					database.LoadMercenaryBuffs(merc);
					// restored buffs carry stat, regen and max HP/mana effects the stats set up above did not include
					merc->CalcBonuses();
				}

				// Set after the buffs so a max HP, mana or endurance buff does not clamp the saved values
				if(restore_saved_stats) {
					const bool saved_dead = c->GetMercInfo().hp <= 0;

					merc->SetHP(saved_dead ? merc->GetMaxHP() : c->GetMercInfo().hp);
					merc->SetMana(saved_dead ? merc->GetMaxMana() : c->GetMercInfo().mana);
					merc->SetEndurance(c->GetMercInfo().endurance);
				}

				merc->LoadMercenarySpells();
			}

			LogMercenaries("LoadMerc Successful for [{}] [{}].", merc->GetName(), c->GetName());
			return merc;
		}
	}

	return 0;
}

void Merc::UpdateMercInfo(Client *c) {
	snprintf(c->GetMercInfo().merc_name, 64, "%s", name);
	c->GetMercInfo().mercid = GetMercenaryID();
	c->GetMercInfo().IsSuspended = IsSuspended();
	c->GetMercInfo().Gender = GetGender();
	c->GetMercInfo().MercSize = GetSize();
	c->GetMercInfo().hp = GetHP();
	c->GetMercInfo().mana = GetMana();
	c->GetMercInfo().endurance = GetEndurance();
	c->GetMercInfo().face = luclinface;
	c->GetMercInfo().luclinHairStyle = hairstyle;
	c->GetMercInfo().luclinHairColor = haircolor;
	c->GetMercInfo().luclinEyeColor = eyecolor1;
	c->GetMercInfo().luclinEyeColor2 = eyecolor2;
	c->GetMercInfo().luclinBeardColor = beardcolor;
	c->GetMercInfo().luclinBeard = beard;
	c->GetMercInfo().drakkinHeritage = drakkin_heritage;
	c->GetMercInfo().drakkinTattoo = drakkin_tattoo;
	c->GetMercInfo().drakkinDetails = drakkin_details;
}

void Merc::UpdateMercStats(Client *c, bool setmax)
{
	if (c->GetMercInfo().MercTemplateID > 0) {
		LogMercenaries("Updating Mercenary Stats for [{}] [{}].", GetName(),
			c->GetName());
		const NPCType *npc_type = content_db.GetMercenaryType(
		    zone->GetMercTemplate(c->GetMercInfo().MercTemplateID)->MercNPCID, GetRace(), c->GetLevel());
		if (npc_type) {
			max_hp = npc_type->max_hp;
			base_hp = npc_type->max_hp;
			max_mana = npc_type->Mana;
			base_mana = npc_type->Mana;
			max_end = npc_type->max_hp;  // Hack since Endurance does not exist for NPCType yet
			base_end = npc_type->max_hp; // Hack since Endurance does not exist for NPCType yet
			hp_regen = npc_type->hp_regen;
			hp_regen_per_second = npc_type->hp_regen_per_second;
			mana_regen = npc_type->mana_regen;
			max_dmg = npc_type->max_dmg;
			min_dmg = npc_type->min_dmg;

			_baseAC = npc_type->AC;
			_baseATK = npc_type->ATK;
			_baseSTR = npc_type->STR;
			_baseSTA = npc_type->STA;
			_baseDEX = npc_type->DEX;
			_baseAGI = npc_type->AGI;
			_baseWIS = npc_type->WIS;
			_baseINT = npc_type->INT;
			_baseCHA = npc_type->CHA;
			_baseATK = npc_type->ATK;
			_baseMR = npc_type->MR;
			_baseFR = npc_type->FR;
			_baseDR = npc_type->DR;
			_basePR = npc_type->PR;
			_baseCR = npc_type->CR;
			_baseCorrup = npc_type->Corrup;

			uint32 scalepercent = (int)(npc_type->scalerate * RuleI(Mercs, ScaleRate) / 100);

			ScaleStats(scalepercent, setmax);

			level = npc_type->level;
			attack_count = npc_type->attack_count;
			attack_delay = npc_type->attack_delay;
			spellscale = npc_type->spellscale;
			healscale = npc_type->healscale;

			CalcBonuses();
			CalcMaxHP();
			CalcMaxMana();
			CalcMaxEndurance();
		}
	}
}

void Merc::ScaleStats(int scalepercent, bool setmax) {

	LogMercenaries("Scaling Mercenary Stats to [{}] Percent for [{}].", scalepercent, GetName());

	if (scalepercent <= 0)
		return;

	float scalerate = (float)scalepercent / 100.0f;

	if ((int64)((float)base_hp * scalerate) > 1)
	{
		max_hp = (int64)((float)base_hp * scalerate);
		base_hp = max_hp;
		if (setmax)
			current_hp = max_hp;
	}

	if (base_mana)
	{
		max_mana = (int64)((float)base_mana * scalerate);
		base_mana = max_mana;
		if (setmax)
			current_mana = max_mana;
	}

	if (base_end)
	{
		max_end = (int64)((float)base_end * scalerate);
		base_end = max_end;
		if (setmax)
			cur_end = max_end;
	}

	if (_baseAC)
	{
		AC = (int)((float)_baseAC * scalerate);
		_baseAC = AC;
	}

	if (_baseATK)
	{
		ATK = (int)((float)_baseATK * scalerate);
		_baseATK = ATK;
	}

	if (_baseSTR)
	{
		STR = (int)((float)_baseSTR * scalerate);
		_baseSTR = STR;
	}
	if (_baseSTA)
	{
		STA = (int)((float)_baseSTA * scalerate);
		_baseSTA = STA;
	}
	if (_baseAGI)
	{
		AGI = (int)((float)_baseAGI * scalerate);
		_baseAGI = AGI;
	}
	if (_baseDEX)
	{
		DEX = (int)((float)_baseDEX * scalerate);
		_baseDEX = DEX;
	}
	if (_baseINT)
	{
		INT = (int)((float)_baseINT * scalerate);
		_baseINT = INT;
	}
	if (_baseWIS)
	{
		WIS = (int)((float)_baseWIS * scalerate);
		_baseWIS = WIS;
	}
	if (_baseCHA)
	{
		CHA = (int)((float)_baseCHA * scalerate);
		_baseCHA = CHA;
	}

	if (_baseMR)
	{
		MR = (int)((float)_baseMR * scalerate);
		_baseMR = MR;
	}
	if (_baseCR)
	{
		CR = (int)((float)_baseCR * scalerate);
		_baseCR = CR;
	}
	if (_baseDR)
	{
		DR = (int)((float)_baseDR * scalerate);
		_baseDR = DR;
	}
	if (_baseFR)
	{
		FR = (int)((float)_baseFR * scalerate);
		_baseFR = FR;
	}
	if (_basePR)
	{
		PR = (int)((float)_basePR * scalerate);
		_basePR = PR;
	}
	if (_baseCorrup)
	{
		Corrup = (int)((float)_baseCorrup * scalerate);
		_baseCorrup = Corrup;
	}

	if (max_dmg)
	{
		max_dmg = (int)((float)max_dmg * scalerate);
	}
	if (min_dmg)
	{
		min_dmg = (int)((float)min_dmg * scalerate);
	}

	return;
}

void Merc::UpdateMercAppearance() {
	// Copied from Bot Code:
	uint32 itemID = 0;
	uint8 materialFromSlot = EQ::textures::materialInvalid;
	for (int i = EQ::invslot::EQUIPMENT_BEGIN; i <= EQ::invslot::EQUIPMENT_END; ++i) {
		itemID = equipment[i];
		if(itemID != 0) {
			materialFromSlot = EQ::InventoryProfile::CalcMaterialFromSlot(i);
			if (materialFromSlot != EQ::textures::materialInvalid)
				SendWearChange(materialFromSlot);
		}
	}

	if (UpdateActiveLight())
		SendAppearancePacket(AppearanceType::Light, GetActiveLightType());
}

void Merc::UpdateEquipmentLight()
{
	m_Light.Type[EQ::lightsource::LightEquipment] = 0;
	m_Light.Level[EQ::lightsource::LightEquipment] = 0;

	for (int index = EQ::invslot::EQUIPMENT_BEGIN; index <= EQ::invslot::EQUIPMENT_END; ++index) {
		if (index == EQ::invslot::slotAmmo) { continue; }

		auto item = database.GetItem(equipment[index]);
		if (item == nullptr) { continue; }

		if (EQ::lightsource::IsLevelGreater(item->Light, m_Light.Type[EQ::lightsource::LightEquipment])) {
			m_Light.Type[EQ::lightsource::LightEquipment] = item->Light;
			m_Light.Level[EQ::lightsource::LightEquipment] = EQ::lightsource::TypeToLevel(m_Light.Type[EQ::lightsource::LightEquipment]);
		}
	}

	uint8 general_light_type = 0;
	for (auto iter = m_loot_items.begin(); iter != m_loot_items.end(); ++iter) {
		auto item = database.GetItem((*iter)->item_id);
		if (item == nullptr) { continue; }

		if (!item->IsClassCommon()) { continue; }
		if (item->Light < 9 || item->Light > 13) { continue; }

		if (EQ::lightsource::TypeToLevel(item->Light))
			general_light_type = item->Light;
	}

	if (EQ::lightsource::IsLevelGreater(general_light_type, m_Light.Type[EQ::lightsource::LightEquipment]))
		m_Light.Type[EQ::lightsource::LightEquipment] = general_light_type;

	m_Light.Level[EQ::lightsource::LightEquipment] = EQ::lightsource::TypeToLevel(m_Light.Type[EQ::lightsource::LightEquipment]);
}

void Merc::AddItem(uint8 slot, uint32 item_id) {
	equipment[slot] = item_id;
	UpdateEquipmentLight();
}

bool Merc::Spawn(Client *owner) {

	if(!owner)
		return false;

	MercTemplate* merc_template = zone->GetMercTemplate(GetMercenaryTemplateID());

	if(!merc_template)
		return false;

	entity_list.AddMerc(this, true, true);

	SentPositionPacket(0.0f, 0.0f, 0.0f, 0.0f, 0);

	LogMercenaries("Spawn Mercenary [{}].", GetName());

	//UpdateMercAppearance();

	return true;
}

void Client::SendMercResponsePackets(uint32 ResponseType)
{
	switch (ResponseType)
	{
	case 0: // Mercenary Spawned Successfully?
		SendMercMerchantResponsePacket(0);
		break;
	case 1: //You do not have enough funds to make that purchase!
		SendMercMerchantResponsePacket(1);
		break;
	case 2: //Mercenary does not exist!
		SendMercMerchantResponsePacket(2);
		break;
	case 3: //Mercenary failed to spawn!
		SendMercMerchantResponsePacket(3);
		break;
	case 4: //Mercenaries are not allowed in raids!
		SendMercMerchantResponsePacket(4);
		break;
	case 5: //You already have a pending mercenary purchase!
		SendMercMerchantResponsePacket(5);
		break;
	case 6: //You have the maximum number of mercenaries.  You must dismiss one before purchasing a new one!
		SendMercMerchantResponsePacket(6);
		break;
	case 7: //You must dismiss your suspended mercenary before purchasing a new one!
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			SendMercMerchantResponsePacket(7);
		else
			//You have the maximum number of mercenaries.  You must dismiss one before purchasing a new one!
			SendMercMerchantResponsePacket(6);
		break;
	case 8: //You can not purchase a mercenary because your group is full!
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			SendMercMerchantResponsePacket(8);
		else
			SendMercMerchantResponsePacket(7);
		break;
	case 9: //You can not purchase a mercenary because you are in combat!
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			//Mercenary failed to spawn!
			SendMercMerchantResponsePacket(3);
		else
			SendMercMerchantResponsePacket(8);
		break;
	case 10: //You have recently dismissed a mercenary and must wait a few more seconds before you can purchase a new one!
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			//Mercenary failed to spawn!
			SendMercMerchantResponsePacket(3);
		else
			SendMercMerchantResponsePacket(9);
		break;
	case 11: //An error occurred created your mercenary!
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			SendMercMerchantResponsePacket(9);
		else
			SendMercMerchantResponsePacket(10);
		break;
	case 12: //Upkeep Charge Message
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			SendMercMerchantResponsePacket(10);
		else
			SendMercMerchantResponsePacket(11);
		break;
	case 13: // ???
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			SendMercMerchantResponsePacket(11);
		else
			SendMercMerchantResponsePacket(12);
		break;
	case 14: //You ran out of funds to pay for your mercenary!
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			SendMercMerchantResponsePacket(12);
		else
			SendMercMerchantResponsePacket(13);
		break;
	case 15: // ???
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			SendMercMerchantResponsePacket(13);
		else
			SendMercMerchantResponsePacket(14);
		break;
	case 16: //Your mercenary is about to be suspended due to insufficient funds!
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			SendMercMerchantResponsePacket(14);
		else
			SendMercMerchantResponsePacket(15);
		break;
	case 17: //There is no mercenary liaison nearby!
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			SendMercMerchantResponsePacket(15);
		else
			SendMercMerchantResponsePacket(16);
		break;
	case 18: //You are too far from the liaison!
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			SendMercMerchantResponsePacket(16);
		else
			SendMercMerchantResponsePacket(17);
		break;
	case 19: //You do not meet the requirements for that mercenary!
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			SendMercMerchantResponsePacket(17);
		else
			SendMercMerchantResponsePacket(18);
		break;
	case 20: //You are unable to interact with the liaison!
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			//You are too far from the liaison!
			SendMercMerchantResponsePacket(16);
		else
			SendMercMerchantResponsePacket(19);
		break;
	case 21: //You do not have a high enough membership level to purchase this mercenary!
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			//You do not meet the requirements for that mercenary!
			SendMercMerchantResponsePacket(17);
		else
			SendMercMerchantResponsePacket(20);
		break;
	case 22: //Your purchase has failed because this mercenary requires a Gold membership!
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			//You do not meet the requirements for that mercenary!
			SendMercMerchantResponsePacket(17);
		else
			SendMercMerchantResponsePacket(21);
		break;
	case 23: //Your purchase has failed because this mercenary requires at least a Silver membership!
		if (ClientVersion() < EQ::versions::ClientVersion::RoF)
			//You do not meet the requirements for that mercenary!
			SendMercMerchantResponsePacket(17);
		else
			SendMercMerchantResponsePacket(22);
		break;
	default: //Mercenary failed to spawn!
		SendMercMerchantResponsePacket(3);
		break;
	}
	LogMercenaries("SendMercResponsePackets [{}] for [{}].", ResponseType, GetName());

}

void Client::UpdateMercTimer()
{
	Merc *merc = GetMerc();

	if(merc && !merc->IsSuspended())
	{
		if(GetMercTimer()->Check())
		{
			uint32 upkeep = merc->CalcUpkeepCost(merc->GetMercenaryTemplateID(), GetLevel());

			if(CheckCanRetainMerc(upkeep))
			{
				if(RuleB(Mercs, ChargeMercUpkeepCost))
				{
					TakeMoneyFromPP((upkeep * 100), true);
				}
			}
			else
			{
				merc->Suspend();
				return;
			}

			// Reset the upkeep timer
			GetMercInfo().MercTimerRemaining = RuleI(Mercs, UpkeepIntervalMS);
			SendMercTimer(merc);
			GetMercTimer()->Start(RuleI(Mercs, UpkeepIntervalMS));
			GetMercTimer()->SetTimer(GetMercInfo().MercTimerRemaining);

			// Send upkeep charge message
			SendMercResponsePackets(12);

			// Warn that mercenary is about to be suspended due to insufficient funds (on next upkeep)
			if (RuleB(Mercs, ChargeMercUpkeepCost) && upkeep > 0 && !HasMoney(upkeep * 100))
			{
				SendMercResponsePackets(16);
			}

			LogMercenaries("UpdateMercTimer Complete for [{}].", GetName());

			// Normal upkeep charge message
			//Message(Chat::LightGray, "You have been charged a mercenary upkeep cost of %i plat, and %i gold and your mercenary upkeep cost timer has been reset to 15 minutes.", upkeep_plat, upkeep_gold, (int)(RuleI(Mercs, UpkeepIntervalMS) / 1000 / 60));

			// Message below given when too low level to be charged
			//Message(Chat::LightGray, "Your mercenary waived an upkeep cost of %i plat, and %i gold or %i %s and your mercenary upkeep cost timer has been reset to %i minutes", upkeep_plat, upkeep_gold, 1, "Bayle Marks", (int)(RuleI(Mercs, UpkeepIntervalMS) / 1000 / 60));
		}
	}
}

bool Client::CheckCanHireMerc(Mob* merchant, uint32 template_id) {

	if (!CheckCanSpawnMerc(template_id))
	{
		return false;
	}

	MercTemplate* mercTemplate = zone->GetMercTemplate(template_id);

	// Check if all merc slots are full (counts both active and suspended mercs)
	if (GetFirstFreeMercSlot() < 0) {
		SendMercResponsePackets(6);
		return false;
	}

	//check for valid merchant
	if(!merchant) {
		SendMercResponsePackets(17);
		return false;
	}

	//check for merchant too far away
	if(DistanceSquared(m_Position, merchant->GetPosition()) > USE_NPC_RANGE2) {
		SendMercResponsePackets(18);
		return false;
	}

	//check for sufficient funds and remove them last
	if(RuleB(Mercs, ChargeMercPurchaseCost)) {
		uint32 cost = Merc::CalcPurchaseCost(template_id, GetLevel()) * 100;  // Cost is in gold
		if(cost > 0 && !HasMoney(cost)) {
			SendMercResponsePackets(1);
			return false;
		}
	}

	LogMercenaries("CheckCanHireMerc True for [{}].", GetName());

	return true;
}

bool Client::CheckCanRetainMerc(uint32 upkeep) {
	Merc* merc = GetMerc();

	//check for sufficient funds
	if(RuleB(Mercs, ChargeMercPurchaseCost)) {
		if(merc) {
			if(upkeep > 0 && !HasMoney(upkeep * 100)) {
				SendMercResponsePackets(14);
				return false;
			}
		}
	}

	return true;
}

bool Client::CheckCanSpawnMerc(uint32 template_id) {

	// Check if mercs are enabled globally
	if(!RuleB(Mercs, AllowMercs))
	{
		return false;
	}

	// Check if zone allows mercs
	if(!zone->AllowMercs())
	{
		SendMercResponsePackets(3);
		return false;
	}

	MercTemplate* mercTemplate = zone->GetMercTemplate(template_id);

	// Invalid merc data
	if(!mercTemplate)
	{
		SendMercResponsePackets(11);
		return false;
	}

	// Check client version
	if(static_cast<unsigned int>(ClientVersion()) < mercTemplate->ClientVersion)
	{
		SendMercResponsePackets(3);
		return false;
	}

	// Check for raid
	if(HasRaid())
	{
		SendMercResponsePackets(4);
		return false;
	}

	// Check group size
	if(GetGroup() &&  GetGroup()->GroupCount() >= MAX_GROUP_MEMBERS)	// database.GroupCount(GetGroup()->GetID())
	{
		SendMercResponsePackets(8);
		return false;
	}

	// Check in combat
	if(GetAggroCount() > 0)
	{
		SendMercResponsePackets(9);
		return false;
	}

	LogMercenaries("CheckCanSpawnMerc True for [{}].", GetName());

	return true;
}

bool Client::CheckCanUnsuspendMerc() {

	if (!CheckCanSpawnMerc(GetMercInfo().MercTemplateID))
	{
		return false;
	}

	MercTemplate* mercTemplate = zone->GetMercTemplate(GetMercInfo().MercTemplateID);

	if(!GetPTimers().Expired(&database, pTimerMercSuspend, false))
	{
		SendMercResponsePackets(10);
		//TODO: find this packet response and tell them properly.
		Message(0, "You must wait %i seconds before unsuspending your mercenary.", GetPTimers().GetRemainingTime(pTimerMercSuspend));
		return false;
	}

	LogMercenaries("CheckCanUnsuspendMerc True for [{}].", GetName());

	return true;
}

void Client::CheckMercSuspendTimer() {

	if(GetMercInfo().SuspendedTime != 0)
	{
		//if(time(nullptr) >= GetMercInfo().SuspendedTime)
		if (p_timers.Expired(&database, pTimerMercSuspend, false))
		{
			GetMercInfo().SuspendedTime = 0;
			SendMercResponsePackets(0);
			SendMercSuspendResponsePacket(GetMercInfo().SuspendedTime);
			LogMercenaries("CheckMercSuspendTimer Ready for [{}].", GetName());
		}
	}
}

void Client::SuspendMercCommand() {

	if(GetMercInfo().MercTemplateID != 0)
	{
		if(GetMercInfo().IsSuspended)
		{
			if(!CheckCanUnsuspendMerc())
			{
				LogMercenaries("SuspendMercCommand Unable to Unsuspend Merc for [{}].", GetName());

				return;
			}

			// Suspend any currently active merc before unsuspending this one
			Merc* active_merc = GetMerc();
			if (active_merc) {
				active_merc->Suspend();
				SetMerc(nullptr);
			}

			// Get merc, assign it to client & spawn
			auto tmpl_it = zone->merc_templates.find(GetMercInfo().MercTemplateID);
			if (tmpl_it == zone->merc_templates.end()) {
				SendMercResponsePackets(3);
				LogMercenaries("SuspendMercCommand Invalid template for [{}].", GetName());
				return;
			}
			Merc* merc = Merc::LoadMercenary(this, &tmpl_it->second, 0, true);
			if(merc)
			{
				if (!SpawnMerc(merc, false))
				{
					LogMercenaries("SuspendMercCommand Unable to Spawn Merc for [{}].", GetName());
					return;
				}

				LogMercenaries("SuspendMercCommand Successful Unsuspend for [{}].", GetName());
			}
			else
			{
				//merc failed to spawn
				SendMercResponsePackets(3);
				LogMercenaries("SuspendMercCommand Failed to Spawn Merc for [{}].", GetName());
			}
		}
		else
		{
			Merc* CurrentMerc = GetMerc();


			if (!RuleB(Mercs, AllowMercSuspendInCombat))
			{
				if (!CheckCanSpawnMerc(GetMercInfo().MercTemplateID))
				{
					return;
				}
			}

			if(CurrentMerc && GetMercenaryID())
			{
				CurrentMerc->Suspend();
				LogMercenaries("SuspendMercCommand Successful Suspend for [{}].", GetName());
			}
			else
			{
				// Reset Merc Suspend State
				GetMercInfo().IsSuspended = true;
				//GetMercInfo().SuspendedTime = time(nullptr) + RuleI(Mercs, SuspendIntervalS);
				//GetMercInfo().MercTimerRemaining = GetMercTimer()->GetRemainingTime();
				//GetMercInfo().Stance = GetStance();
				GetMercTimer()->Disable();
				SendMercSuspendResponsePacket(GetMercInfo().SuspendedTime);
				SendMercTimer(nullptr);
				LogMercenaries("SuspendMercCommand Failed to Get Merc to Suspend. Resetting Suspend State for [{}].", GetName());
			}
		}
	}
	else
	{
		SpawnMercOnZone();
		LogMercenaries("SuspendMercCommand Request Failed to Load Merc for [{}].  Trying SpawnMercOnZone.", GetName());
	}
}


// Handles all client zone change event
void Merc::ProcessClientZoneChange(Client* mercOwner) {

	if(mercOwner)
	{
		Zone();
	}
}

void Client::LoadMercSettings() {
	const std::string announce = GetBucket(MercAnnounceBucket);

	m_merc_settings.announce = announce.empty() ? RuleB(Mercs, AnnounceDefault) : announce != "0";
	m_merc_settings.area_nukes = GetBucket(MercAreaNukesBucket) != "0";
	m_merc_settings.hold = false;
	m_merc_settings.follow_distance = static_cast<uint8>(std::clamp<int>(Strings::ToInt(GetBucket(MercFollowBucket)), 0, MercFollowDistanceMax));
	m_merc_settings.heal_offset = static_cast<uint8>(std::clamp<int>(Strings::ToInt(GetBucket(MercHealOffsetBucket)), 0, MercHealOffsetMax));
}

void Client::SetMercAutoAssist(bool on) {
	m_merc_auto_assist = on;

	SetBucket(MercAutoAssistBucket, on ? "1" : "0");
}

// The Call Assist button and $merc assist call: the merc attacks the target now
bool Client::CallMercAssist() {
	Merc* const merc = GetMerc();
	Mob* const target = GetTarget();

	if (!merc || !target || !target->IsNPC() || !IsAttackAllowed(target) || !merc->IsAttackAllowed(target)) {
		return false;
	}

	merc->CallAssist(target);

	return true;
}

void Client::SpawnMercOnZone() {

	if(!RuleB(Mercs, AllowMercs))
		return;

	// The client does not resend its Auto Assist checkbox at zone-in; unset means on
	m_merc_auto_assist = GetBucket(MercAutoAssistBucket) != "0";
	LoadMercSettings();

	if (GetMerc())
		return;

	if(database.LoadMercenaryInfo(this))
	{
		// Find the active (non-suspended) merc slot, or fall back to the first owned slot
		int active_slot = -1;
		int first_owned_slot = -1;
		int max_slots = std::min(RuleI(Mercs, MaxMercSlots), MAXMERCS);
		for (int slot = 0; slot < max_slots; slot++) {
			if (GetMercInfo(slot).mercid != 0) {
				if (first_owned_slot < 0) {
					first_owned_slot = slot;
				}
				if (!GetMercInfo(slot).IsSuspended) {
					active_slot = slot;
					break;
				}
			}
		}

		if (active_slot >= 0) {
			SetMercSlot(static_cast<uint8>(active_slot));
			GetMercInfo().SuspendedTime = 0;
			// Get merc, assign it to client & spawn
			auto tmpl_it = zone->merc_templates.find(GetMercInfo().MercTemplateID);
			// Checked before loading so a zone that refuses mercs does not consume the saved buffs
			if (tmpl_it != zone->merc_templates.end() && CheckCanSpawnMerc(GetMercInfo().MercTemplateID)) {
				Merc* merc = Merc::LoadMercenary(this, &tmpl_it->second, 0, true);
				if (merc) {
					SpawnMerc(merc, false);
				}
			}
			LogMercenaries("SpawnMercOnZone Normal Merc (slot [{}]) for [{}].", active_slot, GetName());
		} else if (first_owned_slot >= 0) {
			SetMercSlot(static_cast<uint8>(first_owned_slot));
			int32 TimeDiff = GetMercInfo().SuspendedTime - time(nullptr);
			if (TimeDiff > 0) {
				if (!GetPTimers().Enabled(pTimerMercSuspend)) {
					GetPTimers().Start(pTimerMercSuspend, TimeDiff);
				}
			}
			SendMercTimer(GetMerc());
			LogMercenaries("SpawnMercOnZone Suspended Merc (slot [{}]) for [{}].", first_owned_slot, GetName());
		} else {
			LogMercenaries("SpawnMercOnZone No valid merc slots found for [{}].", GetName());
		}

		// Send merc personal info for all owned mercs (populates the Manage tab)
		if (GetNumberOfMercenaries() > 0) {
			SendMercPersonalInfo();
		}
	}
	else
	{
		LogMercenaries("SpawnMercOnZone No merc info in database for [{}].", GetName());
	}
}

void Client::SendMercTimer(Merc* merc) {

	if (GetMercInfo().mercid == 0)
	{
		return;
	}

	if (!merc)
	{
		SendMercTimerPacket(NO_MERC_ID, MERC_STATE_SUSPENDED, GetMercInfo().SuspendedTime, GetMercInfo().MercTimerRemaining, RuleI(Mercs, SuspendIntervalMS));
		LogMercenaries("SendMercTimer No Merc for [{}].", GetName());
	}
	else if (merc->IsSuspended())
	{
		SendMercTimerPacket(NO_MERC_ID, MERC_STATE_SUSPENDED, GetMercInfo().SuspendedTime, GetMercInfo().MercTimerRemaining, RuleI(Mercs, SuspendIntervalMS));
		LogMercenaries("SendMercTimer Suspended Merc for [{}].", GetName());
	}
	else
	{
		SendMercTimerPacket(merc->GetID(), MERC_STATE_NORMAL, NOT_SUSPENDED_TIME, GetMercInfo().MercTimerRemaining, RuleI(Mercs, SuspendIntervalMS));
		LogMercenaries("SendMercTimer Normal Merc for [{}].", GetName());
	}

}

// On false the merc has been freed; callers must not use the pointer afterwards
bool Client::SpawnMerc(Merc* merc, bool setMaxStats) {

	if (!merc)
	{
		return false;
	}

	if (!CheckCanSpawnMerc(merc->GetMercenaryTemplateID()))
	{
		// Not yet owned or in the entity list, so it can be deleted directly
		safe_delete(merc);
		return false;
	}

	merc->Spawn(this);
	merc->SetSuspended(false);
	SetMerc(merc);
	merc->SendOwnerLinkToZone(true);
	merc->Unsuspend(setMaxStats);

	// A merc hired before stances were recorded has none; give it the template's default
	if(GetMercInfo().Stance == Stance::Unknown) {
		GetMercInfo().Stance = Merc::GetDefaultStance(merc->GetMercenaryTemplateID());
	}

	merc->SetStance(GetMercInfo().Stance);
	merc->SendStatTable(true);

	LogMercenaries("SpawnMerc Success for [{}].", GetName());

	return true;
}

bool Merc::Suspend() {

	Client* mercOwner = GetMercenaryOwner();

	if(!mercOwner)
		return false;

	SetSuspended(true);

	mercOwner->GetMercInfo().IsSuspended = true;
	mercOwner->GetMercInfo().SuspendedTime = time(nullptr) + RuleI(Mercs, SuspendIntervalS);
	mercOwner->GetMercInfo().MercTimerRemaining = mercOwner->GetMercTimer()->GetRemainingTime();
	mercOwner->GetMercInfo().Stance = GetStance();
	Save();
	mercOwner->GetMercTimer()->Disable();
	mercOwner->SendMercSuspendResponsePacket(mercOwner->GetMercInfo().SuspendedTime);
	mercOwner->SendMercTimer(this);

	Depop();

	// Start the timer to send the packet that refreshes the Unsuspend Button
	mercOwner->GetPTimers().Start(pTimerMercSuspend, RuleI(Mercs, SuspendIntervalS));

	LogMercenaries("Suspend Complete for [{}].", mercOwner->GetName());

	return true;
}

bool Client::MercOnlyOrNoGroup() {

	if (!GetGroup())
	{
		return true;
	}
	if (GetMerc())
	{
		if (GetMerc()->GetGroup() == GetGroup())
		{
			if (GetGroup()->GroupCount() < 3)
			{
				return true;
			}
		}
	}
	return false;
}

bool Merc::Unsuspend(bool setMaxStats) {

	Client* mercOwner = nullptr;

	if(GetMercenaryOwner()) {
		mercOwner = GetMercenaryOwner();
	}

	if(!mercOwner)
		return false;

	if(GetID())
	{
		// Set time remaining to max on unsuspend - there is a charge for unsuspending as well
		SetSuspended(false);

		mercOwner->GetMercInfo().mercid = GetMercenaryID();
		mercOwner->GetMercInfo().IsSuspended = false;

		mercOwner->SendMercenaryUnsuspendPacket(0);
		mercOwner->SendMercenaryUnknownPacket(1);
		mercOwner->GetMercInfo().SuspendedTime = 0;
		// Reset the upkeep timer
		mercOwner->GetMercInfo().MercTimerRemaining = RuleI(Mercs, UpkeepIntervalMS);
		mercOwner->GetMercTimer()->Start(RuleI(Mercs, UpkeepIntervalMS));
		//mercOwner->GetMercTimer()->SetTimer(mercOwner->GetMercInfo().MercTimerRemaining);
		mercOwner->SendMercTimer(this);
		if(!mercOwner->GetPTimers().Expired(&database, pTimerMercSuspend, false))
			mercOwner->GetPTimers().Clear(&database, pTimerMercSuspend);

		if (MercJoinClientGroup())
		{
			if(setMaxStats)
			{
				RestoreHealth();
				RestoreMana();
				RestoreEndurance();
			}

			//check for sufficient funds and remove them last
			if(RuleB(Mercs, ChargeMercUpkeepCost))
			{
				uint32 cost = CalcUpkeepCost(GetMercenaryTemplateID(), GetLevel()) * 100;    // Cost is in gold
				if(cost > 0 && !mercOwner->HasMoney(cost))
				{
					mercOwner->SendMercResponsePackets(1);
					Suspend();
					return false;
				}
			}
			Save();
		}
	}

	return true;
}

bool Client::DismissMerc(uint32 MercID) {

	bool Dismissed = true;
	if (!database.DeleteMercenary(MercID))
	{
		LogMercenaries("Dismiss Failed Database Query for MercID: [{}], Client: [{}].", MercID, GetName());
		Dismissed = false;
	}
	else
	{
		LogMercenaries("Dismiss Successful for [{}].", GetName());
	}

	if (GetMerc())
	{
		GetMerc()->Depop();
	}

	// Clear the dismissed merc's slot data so it becomes available
	memset(&GetMercInfo(), 0, sizeof(MercInfo));

	SetMerc(nullptr);

	// Update the client with remaining mercs or clear if none left
	if (GetNumberOfMercenaries() > 0) {
		SendMercPersonalInfo();
	} else {
		SendClearMercInfo();
	}

	return Dismissed;
}

void Merc::Zone() {
	Save();
	Depop();
}

void Merc::Depop(bool start_spawn_timer) {

	if(m_merc_depopped) {
		return;
	}

	m_merc_depopped = true;

	// Zone shutdown deletes mobs before groups, so the group and owner may already be gone
	if(!is_zone_loaded) {
		p_depop = true;
		NPC::Depop(false);

		return;
	}

	WipeHateList();

	if(IsCasting())
	{
		InterruptSpell();
	}

	entity_list.RemoveFromHateLists(this);

	if(GetGroup())
	{
		RemoveMercFromGroup(this, GetGroup());
	}

	SendOwnerLinkToZone(false);

	Client* owner = GetMercenaryOwner();
	if(owner) {
		owner->UpdateXTargetType(MyMercenary, nullptr);
		owner->UpdateXTargetType(MyMercenaryTarget, nullptr);
	}

	entity_list.RemoveMerc(GetID());

	if(HasPet())
	{
		GetPet()->Depop();
	}

	p_depop = true;

	NPC::Depop(false);
}

bool Merc::RemoveMercFromGroup(Merc* merc, Group* group) {

	bool Result = false;

	if(merc && group)
	{
		uint32 groupID = group->GetID();
		if(merc->HasGroup())
		{
			if(!group->IsLeader(merc))
			{
				merc->SetFollowID(0);

				if (group->GroupCount() <= 2 && merc->GetGroup() == group && is_zone_loaded)
				{
					group->DisbandGroup();
				}
				else if(group->DelMember(merc, true))
				{
					if(merc->GetMercenaryCharacterID() != 0)
					{
						Group::RemoveFromGroup(merc);
					}
				}
			}
			else
			{
				// A merc is group leader - Disband and re-group each member with their mercs
				for(int i = 0; i < MAX_GROUP_MEMBERS; i++)
				{
					if(!group->members[i])
						continue;

					if(!group->members[i]->IsClient())
						continue;

					Client *groupMember = group->members[i]->CastToClient();
					groupMember->LeaveGroup();
					if (groupMember->GetMerc())
					{
						groupMember->GetMerc()->MercJoinClientGroup();
					}
				}
				// Group should be removed by now, but just in case:
				Group *oldGroup = entity_list.GetGroupByID(groupID);
				if (oldGroup != nullptr)
				{
					oldGroup->DisbandGroup();
				}
			}

			Result = true;
		}
	}

	return Result;
}

bool Merc::MercJoinClientGroup() {

	Client* mercOwner = nullptr;

	if(GetMercenaryOwner())
	{
		mercOwner = GetMercenaryOwner();
	}

	if(!mercOwner)
	{
		Suspend();
		return false;
	}

	if(GetID())
	{
		if (HasGroup())
		{
			RemoveMercFromGroup(this, GetGroup());
		}

		Group* g = entity_list.GetGroupByClient(mercOwner);

		//nobody from our group is here... start a new group
		if(!g)
		{
			g = new Group(mercOwner);

			if(!g)
			{
				delete g;
				g = nullptr;
				return false;
			}

			entity_list.AddGroup(g);

			if(g->GetID() == 0)
			{

				delete g;
				g = nullptr;
				return false;
			}

			if (AddMercToGroup(this, g))
			{
				g->AddToGroup(mercOwner);
				database.SetGroupLeaderName(g->GetID(), mercOwner->GetName());
				database.RefreshGroupFromDB(mercOwner);
				g->SaveGroupLeaderAA();
				LogMercenaries("Mercenary joined new group: [{}] [{}].", GetName(), mercOwner->GetName());
			}
			else
			{
				g->DisbandGroup();
				Suspend();
				LogMercenaries("Mercenary disbanded new group: [{}] [{}].", GetName(), mercOwner->GetName());
			}

		}
		else if (AddMercToGroup(this, mercOwner->GetGroup()))
		{
			// Group already exists
			database.RefreshGroupFromDB(mercOwner);
			// Update members that are out of zone
			GetGroup()->SendGroupJoinOOZ(this);
			LogMercenaries("Mercenary [{}] joined existing group with [{}].", GetName(), mercOwner->GetName());
		}
		else
		{
			Suspend();
			LogMercenaries("Mercenary failed to join the group - Suspending [{}] for [{}].", GetName(), mercOwner->GetName());
		}
	}

	return true;
}

bool Merc::AddMercToGroup(Merc* merc, Group* group) {
	bool Result = false;

	if(merc && group) {
		// Remove merc from current group if it's not the destination group
		if(merc->HasGroup())
		{
			if(merc->GetGroup() == group && merc->GetMercenaryOwner())
			{
				// Merc is already in the destination group
				merc->SetFollowID(merc->GetMercenaryOwner()->GetID());
				return true;
			}
			merc->RemoveMercFromGroup(merc, merc->GetGroup());
		}
		//Try and add the member, followed by checking if the merc owner exists.
		if(group->AddMember(merc) && merc->GetMercenaryOwner())
		{
			merc->SetFollowID(merc->GetMercenaryOwner()->GetID());
			Result = true;
		}
		else
		{
			//Suspend it if the member is not added or the merc's owner is not valid.
			merc->Suspend();
		}
	}

	return Result;
}

void Client::InitializeMercInfo() {

	for(int i=0; i<MAXMERCS; i++)
	{
		m_mercinfo[i] = MercInfo();
	}

}

Merc* Client::GetMerc() {

	if(GetMercenaryID() == 0)
	{
		LogMercenariesDetail("GetMerc - GetMercenaryID: 0 for [{}].", GetName());
		return (nullptr);
	}

	Merc* tmp = entity_list.GetMercByID(GetMercenaryID());
	if(tmp == nullptr)
	{
		SetMercID(0);
		LogMercenariesDetail("GetMerc No Merc for [{}].", GetName());
		return (nullptr);
	}

	if(tmp->GetOwnerID() != GetID())
	{
		SetMercID(0);
		LogMercenariesDetail("GetMerc Owner Mismatch - OwnerID: [{}], ClientID: [{}], Client: [{}].", tmp->GetOwnerID(), GetID(), GetName());
		return (nullptr);
	}

	return (tmp);
}

uint8 Client::GetNumberOfMercenaries()
{
	uint8 count = 0;

	for (int slot_id = 0; slot_id < MAXMERCS; slot_id++) {
		if (m_mercinfo[slot_id].mercid != 0) {
			count++;
		}
	}

	return count;
}

// Owned merc slots in the order the merc list is sent. TOB keeps slot order and marks the active merc by index;
// older clients mark the first entry, so the active merc goes first for them.
std::vector<int> Client::GetMercDisplayOrder()
{
	std::vector<int> order;
	const int max_slots = std::min(RuleI(Mercs, MaxMercSlots), MAXMERCS);
	const bool active_first = ClientVersion() < EQ::versions::ClientVersion::TOB;

	// Entries SendMercPersonalInfo skips must not take a row here, or the client's row index drifts from the slot
	auto is_listed = [&](int slot) {
		const auto& info = m_mercinfo[slot];

		if (info.mercid == 0 || zone->merc_templates.find(info.MercTemplateID) == zone->merc_templates.end()) {
			return false;
		}

		const auto stance_it = zone->merc_stance_list.find(info.MercTemplateID);

		return stance_it == zone->merc_stance_list.end() || stance_it->second.size() <= MAX_MERC_STANCES;
	};

	if (active_first && GetMercSlot() < max_slots && is_listed(GetMercSlot())) {
		order.push_back(GetMercSlot());
	}

	for (int slot = 0; slot < max_slots; slot++) {
		if (active_first && slot == GetMercSlot()) {
			continue;
		}

		if (is_listed(slot)) {
			order.push_back(slot);
		}
	}

	return order;
}

int Client::GetFirstFreeMercSlot()
{
	int max_slots = std::min(RuleI(Mercs, MaxMercSlots), MAXMERCS);
	for (int slot_id = 0; slot_id < max_slots; slot_id++) {
		if (m_mercinfo[slot_id].mercid == 0) {
			return slot_id;
		}
	}
	return -1;
}

void Merc::SetMercData( uint32 template_id ) {

	MercTemplate* merc_template = zone->GetMercTemplate(template_id);
	SetMercTemplateID( merc_template->MercTemplateID );
	SetMercType( merc_template->MercType );
	SetMercSubType( merc_template->MercSubType );
	SetProficiencyID( merc_template->ProficiencyID );
	SetTierID( merc_template->TierID );
	SetCostFormula( merc_template->CostFormula );
	SetMercNameType( merc_template->MercNameType );

}

MercTemplate* Zone::GetMercTemplate( uint32 template_id ) {
	return &merc_templates[template_id];
}

void Client::SetMerc(Merc* newmerc) {

	Merc* oldmerc = GetMerc();
	if (oldmerc)
	{
		oldmerc->SetOwnerID(0);
	}

	if (!newmerc)
	{
		SetMercID(0);
		GetMercInfo().mercid = 0;
		GetMercInfo().MercTemplateID = 0;
		GetMercInfo().myTemplate = nullptr;
		GetMercInfo().IsSuspended = false;
		GetMercInfo().SuspendedTime = 0;
		GetMercInfo().Gender = Gender::Male;
		GetMercInfo().State = 0;
		memset(GetMercInfo().merc_name, 0, 64);
		LogMercenaries("SetMerc No Merc for [{}].", GetName());
	}
	else
	{
		SetMercID(newmerc->GetID());
		//Client* oldowner = entity_list.GetClientByID(newmerc->GetOwnerID());
		newmerc->SetOwnerID(GetID());
		newmerc->SetMercCharacterID(CharacterID());
		newmerc->SetClientVersion((uint8)ClientVersion());

		// the merc was built before it had an owner, so add the Merc tab gear now
		if (IsMercEquipmentEnabled()) {
			newmerc->RecalculateEquipmentBonuses();
		}

		GetMercInfo().mercid = newmerc->GetMercenaryID();
		GetMercInfo().MercTemplateID = newmerc->GetMercenaryTemplateID();
		GetMercInfo().myTemplate = zone->GetMercTemplate(GetMercInfo().MercTemplateID);
		GetMercInfo().IsSuspended = newmerc->IsSuspended();
		GetMercInfo().SuspendedTime = 0;
		GetMercInfo().Gender = newmerc->GetGender();
		GetMercInfo().State = newmerc->IsSuspended() ? MERC_STATE_SUSPENDED : MERC_STATE_NORMAL;
		snprintf(GetMercInfo().merc_name, 64, "%s", newmerc->GetName());
		LogMercenaries("SetMerc New Merc for [{}].", GetName());
	}

	UpdateXTargetType(MyMercenary, newmerc);
}

void Client::UpdateMercLevel() {
	Merc* merc = GetMerc();
	if (merc)
	{
		merc->UpdateMercStats(this, false);

		// Spells and gear are chosen by level, so a merc that levels in zone needs both again
		database.LoadMercenaryEquipment(merc);
		merc->CalcBonuses();
		merc->LoadMercenarySpells();
		merc->UpdateMercAppearance();
		merc->SendAppearancePacket(AppearanceType::WhoLevel, GetLevel(), true, true);
	}
}

void Client::SendMercMerchantResponsePacket(int32 response_type) {
	// This response packet brings up the Mercenary Manager window
	if (ClientVersion() >= EQ::versions::ClientVersion::SoD)
	{
		auto outapp = new EQApplicationPacket(OP_MercenaryHire, sizeof(MercenaryMerchantResponse_Struct));
		MercenaryMerchantResponse_Struct* mmr = (MercenaryMerchantResponse_Struct*)outapp->pBuffer;
		mmr->ResponseType = response_type;              // send specified response type
		FastQueuePacket(&outapp);
		LogMercenariesDetail("Sent SendMercMerchantResponsePacket ResponseType: [{}], Client: [{}].", response_type, GetName());
	}
}

void Client::SendMercenaryUnknownPacket(uint8 type) {

	auto outapp = new EQApplicationPacket(OP_MercenaryUnknown1, 1);
	outapp->WriteUInt8(type);
	FastQueuePacket(&outapp);
	LogMercenariesDetail("Sent SendMercenaryUnknownPacket Type: [{}], Client: [{}].", type, GetName());

}

void Client::SendMercenaryUnsuspendPacket(uint8 type) {

	auto outapp = new EQApplicationPacket(OP_MercenaryUnsuspendResponse, 1);
	outapp->WriteUInt8(type);
	FastQueuePacket(&outapp);
	LogMercenariesDetail("Sent SendMercenaryUnsuspendPacket Type: [{}], Client: [{}].", type, GetName());

}

void Client::SendMercSuspendResponsePacket(uint32 suspended_time) {

	auto outapp = new EQApplicationPacket(OP_MercenarySuspendResponse, sizeof(SuspendMercenaryResponse_Struct));
	SuspendMercenaryResponse_Struct* smr = (SuspendMercenaryResponse_Struct*)outapp->pBuffer;
	smr->SuspendTime = suspended_time;              // Seen 0 (not suspended) or c9 c2 64 4f (suspended on Sat Mar 17 11:58:49 2012) - Unix Timestamp
	FastQueuePacket(&outapp);
	LogMercenariesDetail("Sent SendMercSuspendResponsePacket Time: [{}], Client: [{}].", suspended_time, GetName());

}

void Client::SendMercTimerPacket(int32 entity_id, int32 merc_state, int32 suspended_time, int32 update_interval, int32 unk01) {

	// Send Mercenary Status/Timer packet
	auto outapp = new EQApplicationPacket(OP_MercenaryTimer, sizeof(MercenaryStatus_Struct));
	MercenaryStatus_Struct* mss = (MercenaryStatus_Struct*)outapp->pBuffer;
	mss->MercEntityID = entity_id; // Seen 0 (no merc spawned) or unknown value when merc is spawned
	mss->MercState = merc_state; // Seen 5 (normal) or 1 (suspended)
	mss->SuspendedTime = suspended_time; // Seen 0 for not suspended or Unix Timestamp for suspended merc
	mss->UpdateInterval = update_interval; // Seen 900000 - 15 minutes in ms
	mss->MercUnk01 = unk01; // Seen 180000 - 3 minutes in ms - Used for the unsuspend button refresh timer
	FastQueuePacket(&outapp);
	LogMercenariesDetail("Sent SendMercTimerPacket EndID: [{}], State: [{}], SuspendTime: [{}], Interval: [{}], Unk1: [{}], Client: [{}].", entity_id, merc_state, suspended_time, update_interval, unk01, GetName());

}

static void QueueMercOwnerLink(Client *c, uint16 merc_id, uint16 owner_id)
{
	auto outapp = std::make_unique<EQApplicationPacket>(OP_MercenaryOwnerLink, sizeof(MercenaryOwnerLink_Struct));
	auto link   = reinterpret_cast<MercenaryOwnerLink_Struct *>(outapp->pBuffer);

	link->merc_entity_id  = merc_id;
	link->owner_entity_id = owner_id;
	c->QueuePacket(outapp.get());
}

// TOB+ draws merc name tags and owner links from these, not from the spawn packet
void Merc::SendOwnerLinkToZone(bool spawned)
{
	Client *owner = GetMercenaryOwner();
	if (!owner) {
		return;
	}

	const uint16 merc_id = spawned ? GetID() : 0;
	for (const auto &[id, c] : entity_list.GetClientList()) {
		if (c->ClientVersion() >= EQ::versions::ClientVersion::TOB) {
			QueueMercOwnerLink(c, merc_id, owner->GetID());
		}
	}

	if (owner->ClientVersion() >= EQ::versions::ClientVersion::TOB) {
		auto outapp = std::make_unique<EQApplicationPacket>(OP_MercenaryU32Field274, sizeof(MercenaryActiveEntity_Struct));
		reinterpret_cast<MercenaryActiveEntity_Struct *>(outapp->pBuffer)->merc_entity_id = merc_id;
		owner->QueuePacket(outapp.get());
	}
}

std::array<int32, MercenaryStat::Count> Merc::BuildStatTable()
{
	std::array<int32, MercenaryStat::Count> t{};

	t[MercenaryStat::MaxHPBase]            = CalcBaseHP();
	t[MercenaryStat::MaxHP]                = GetMaxHP();
	t[MercenaryStat::MaxManaBase]          = CalcBaseMana();
	t[MercenaryStat::MaxMana]              = GetMaxMana();
	t[MercenaryStat::MaxEnduranceBase]     = CalcBaseEndurance();
	t[MercenaryStat::MaxEndurance]         = GetMaxEndurance();
	t[MercenaryStat::ACBase]               = _baseAC;
	t[MercenaryStat::AC]                   = GetAC();
	t[MercenaryStat::EvasionBase]          = 0;
	t[MercenaryStat::Evasion]              = GetAvoidance();
	t[MercenaryStat::OffenseBase]          = _baseATK;
	t[MercenaryStat::Offense]              = GetATK();
	t[MercenaryStat::AccuracyBase]         = 0;
	t[MercenaryStat::Accuracy]             = GetAccuracy();
	// a plain percent: 0 when not hasted, so a slow does not go negative
	t[MercenaryStat::Haste]                = std::max(0, GetHaste() - 100);
	t[MercenaryStat::STRBase]              = _baseSTR;
	t[MercenaryStat::STR]                  = GetSTR();
	t[MercenaryStat::STABase]              = _baseSTA;
	t[MercenaryStat::STA]                  = GetSTA();
	t[MercenaryStat::INTBase]              = _baseINT;
	t[MercenaryStat::INT]                  = GetINT();
	t[MercenaryStat::WISBase]              = _baseWIS;
	t[MercenaryStat::WIS]                  = GetWIS();
	t[MercenaryStat::AGIBase]              = _baseAGI;
	t[MercenaryStat::AGI]                  = GetAGI();
	t[MercenaryStat::DEXBase]              = _baseDEX;
	t[MercenaryStat::DEX]                  = GetDEX();
	t[MercenaryStat::CHABase]              = _baseCHA;
	t[MercenaryStat::CHA]                  = GetCHA();
	t[MercenaryStat::HPRegenBase]          = hp_regen;
	t[MercenaryStat::HPRegen]              = CalcHPRegen();
	t[MercenaryStat::ManaRegenBase]        = mana_regen;
	t[MercenaryStat::ManaRegen]            = CalcManaRegenValue(IsSitting());
	// mirrors the level term of CalcEnduranceRegen() before any bonuses
	t[MercenaryStat::EnduranceRegenBase]   = GetLevel() * 4 / 10 + 2;
	t[MercenaryStat::EnduranceRegen]       = CalcEnduranceRegen();
	t[MercenaryStat::HealAmount]           = GetHealAmt();
	t[MercenaryStat::SpellDamage]          = GetSpellDmg();
	t[MercenaryStat::HeroicSTR]            = GetHeroicSTR();
	t[MercenaryStat::HeroicSTA]            = GetHeroicSTA();
	t[MercenaryStat::HeroicDEX]            = GetHeroicDEX();
	t[MercenaryStat::HeroicAGI]            = GetHeroicAGI();
	t[MercenaryStat::HeroicWIS]            = GetHeroicWIS();
	t[MercenaryStat::HeroicINT]            = GetHeroicINT();
	t[MercenaryStat::HeroicCHA]            = GetHeroicCHA();

	return t;
}

// The Merc tab stays blank until it has received a table; later sends carry only the pairs that changed
void Merc::SendStatTable(bool full)
{
	Client *owner = GetMercenaryOwner();
	if (!owner || owner->ClientVersion() < EQ::versions::ClientVersion::TOB || IsSuspended()) {
		return;
	}

	const auto stats = BuildStatTable();

	std::vector<uint32> changed;
	for (uint32 index = 0; index < MercenaryStat::Count; ++index) {
		if (full || stats[index] != m_last_stat_table[index]) {
			changed.push_back(index);
		}
	}

	if (changed.empty()) {
		return;
	}

	m_last_stat_table = stats;

	SerializeBuffer buffer(sizeof(uint32) + changed.size() * 2 * sizeof(uint32));
	buffer.WriteUInt32(static_cast<uint32>(changed.size()));
	for (const uint32 index : changed) {
		buffer.WriteUInt32(index);
		buffer.WriteInt32(stats[index]);
	}

	auto outapp = std::make_unique<EQApplicationPacket>(OP_MercenaryTable44, std::move(buffer));
	owner->QueuePacket(outapp.get());
}

// existing mercs for a client that just zoned in
void Client::SendMercOwnerLinks()
{
	if (ClientVersion() < EQ::versions::ClientVersion::TOB) {
		return;
	}

	for (const auto &[id, merc] : entity_list.GetMercList()) {
		Client *owner = merc->GetMercenaryOwner();
		if (owner && !merc->IsSuspended()) {
			QueueMercOwnerLink(this, merc->GetID(), owner->GetID());
		}
	}
}

void Client::SendMercAssignPacket(uint32 entityID, uint32 unk01, uint32 unk02) {
	auto outapp = new EQApplicationPacket(OP_MercenaryAssign, sizeof(MercenaryAssign_Struct));
	MercenaryAssign_Struct* mas = (MercenaryAssign_Struct*)outapp->pBuffer;
	mas->MercEntityID = entityID;
	mas->MercUnk01 = unk01;
	mas->MercUnk02 = unk02;
	FastQueuePacket(&outapp);
	LogMercenariesDetail("Sent SendMercAssignPacket EndID: [{}], Unk1: [{}], Unk2: [{}], Client: [{}].", entityID, unk01, unk02, GetName());
}

void NPC::LoadMercenaryTypes()
{
	const std::string& query = fmt::format(
		SQL(
			SELECT DISTINCT MTyp.dbstring, MTyp.clientversion
			FROM merc_merchant_entries MME, merc_merchant_template_entries MMTE,
			merc_types MTyp, merc_templates MTem
			WHERE MME.merchant_id = {}
			AND MME.merc_merchant_template_id = MMTE.merc_merchant_template_id
			AND MMTE.merc_template_id = MTem.merc_template_id
			AND MTem.merc_type_id = MTyp.merc_type_id
		),
		GetNPCTypeID()
	);

	auto results = database.QueryDatabase(query);
	if (!results.Success() || !results.RowCount()) {
		return;
	}

	for (auto row : results) {
		mercTypeList.push_back(
			MercType{
				.Type = Strings::ToUnsignedInt(row[0]),
				.ClientVersion = Strings::ToUnsignedInt(row[1])
			}
		);
	}
}

void NPC::LoadMercenaries()
{
	const std::string& query = fmt::format(
		SQL(
			SELECT DISTINCT MTem.merc_template_id, MTyp.dbstring AS merc_type_id,
			MTem.dbstring AS merc_subtype_id, 0 AS CostFormula,
			CASE WHEN MTem.clientversion > MTyp.clientversion
			THEN MTem.clientversion
			ELSE MTyp.clientversion END AS clientversion, MTem.merc_npc_type_id
			FROM merc_merchant_entries MME, merc_merchant_template_entries MMTE,
			merc_types MTyp, merc_templates MTem
			WHERE MME.merchant_id = {} AND
			MME.merc_merchant_template_id = MMTE.merc_merchant_template_id
			AND MMTE.merc_template_id = MTem.merc_template_id
			AND MTem.merc_type_id = MTyp.merc_type_id
		),
		GetNPCTypeID()
	);

	auto results = database.QueryDatabase(query);
	if (!results.Success() || !results.RowCount()) {
		return;
	}

	for (auto row : results) {
		mercDataList.push_back(
			MercData{
				.MercTemplateID = Strings::ToUnsignedInt(row[0]),
				.MercType = Strings::ToUnsignedInt(row[1]),
				.MercSubType = Strings::ToUnsignedInt(row[2]),
				.CostFormula = Strings::ToUnsignedInt(row[3]),
				.ClientVersion = Strings::ToUnsignedInt(row[4]),
				.NPCID = Strings::ToUnsignedInt(row[5])
			}
		);
	}
}

int NPC::GetNumMercenaryTypes(uint32 clientVersion) {

	int count = 0;
	std::list<MercType> mercTypeList = GetMercenaryTypesList();

	for (auto mercTypeListItr = mercTypeList.begin(); mercTypeListItr != mercTypeList.end(); ++mercTypeListItr) {
		if(mercTypeListItr->ClientVersion <= clientVersion)
			count++;
	}

	return count;
}

int NPC::GetNumberOfMercenaries(uint32 clientVersion) {

	int count = 0;
	std::list<MercData> mercDataList = GetMercenariesList();

	for (auto mercListItr = mercDataList.begin(); mercListItr != mercDataList.end(); ++mercListItr) {
		if(mercListItr->ClientVersion <= clientVersion)
			count++;
	}

	return count;
}

std::list<MercType> NPC::GetMercenaryTypesList(uint32 clientVersion) {

	std::list<MercType> result;

	if(GetNumMercenaryTypes() > 0)
	{
		for (auto mercTypeListItr = mercTypeList.begin(); mercTypeListItr != mercTypeList.end();
		     ++mercTypeListItr) {
			if(mercTypeListItr->ClientVersion <= clientVersion)
			{
				MercType mercType;
				mercType.Type = mercTypeListItr->Type;
				mercType.ClientVersion = mercTypeListItr->ClientVersion;
				result.push_back(mercType);
			}
		}
	}

	return result;
}

std::list<MercData> NPC::GetMercenariesList(uint32 clientVersion) {

	std::list<MercData> result;

	if(GetNumberOfMercenaries() > 0)
	{
		for (auto mercListItr = mercDataList.begin(); mercListItr != mercDataList.end(); ++mercListItr) {
			if(mercListItr->ClientVersion <= clientVersion)
			{
				MercTemplate *merc_template = zone->GetMercTemplate(mercListItr->MercTemplateID);

				if(merc_template)
				{
					MercData mercData;
					mercData.MercTemplateID = mercListItr->MercTemplateID;
					mercData.MercType = merc_template->MercType;
					mercData.MercSubType = merc_template->MercSubType;
					mercData.CostFormula = merc_template->CostFormula;
					mercData.ClientVersion = merc_template->ClientVersion;
					mercData.NPCID = merc_template->MercNPCID;
					result.push_back(mercData);
				}
			}
		}
	}

	return result;
}

uint32 Merc::CalcPurchaseCost(uint32 templateID , uint8 level, uint8 currency_type) {

	uint32 cost = 0;

	MercTemplate *mercData = zone->GetMercTemplate(templateID);

	if(mercData)
	{
		//calculate cost in coin - cost in gold
		if(currency_type == 0)
		{
			int levels_above_cutoff;
			switch (mercData->CostFormula)
			{
			case 0:
				levels_above_cutoff = level > 10 ? (level - 10) : 0;
				cost = levels_above_cutoff * 300;
				cost += level >= 10 ? 100 : 0;
				cost /= 100;
				break;
			default:
				break;
			}
		}
		else if(currency_type == 19)
		{
			// cost in Bayle Marks
			cost = 1;
		}
	}

	return cost;
}

uint32 Merc::CalcUpkeepCost(uint32 templateID , uint8 level, uint8 currency_type) {

	uint32 cost = 0;

	MercTemplate *mercData = zone->GetMercTemplate(templateID);

	if(mercData)
	{
		//calculate cost in coin - cost in gold
		if(currency_type == 0)
		{
			int levels_above_cutoff;
			switch (mercData->CostFormula)
			{
			case 0:
				levels_above_cutoff = level > 10 ? (level - 10) : 0;
				cost = levels_above_cutoff * 300;
				cost += level >= 10 ? 100 : 0;
				cost /= 100;
				break;
			default:
				break;
			}
		}
		else if(currency_type == 19)
		{
			// cost in Bayle Marks
			cost = 1;
		}
	}

	return cost;
}

void Merc::Signal(int signal_id)
{
	if (parse->MercHasQuestSub(EVENT_SIGNAL)) {
		parse->EventMerc(EVENT_SIGNAL, this, nullptr, std::to_string(signal_id), 0);
	}
}

void Merc::SendPayload(int payload_id, std::string payload_value)
{
	if (parse->MercHasQuestSub(EVENT_PAYLOAD)) {
		const auto& export_string = fmt::format("{} {}", payload_id, payload_value);

		parse->EventMerc(EVENT_PAYLOAD, this, nullptr, export_string, 0);
	}
}
