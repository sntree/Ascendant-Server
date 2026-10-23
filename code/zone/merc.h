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
#pragma once

#include "zone/npc.h"

#include <array>

class Client;
class Corpse;
class Group;
class Mob;
class Raid;
struct MercTemplate;
struct NPCType;
struct NewSpawn_Struct;

namespace EQ
{
	struct ItemData;
}

constexpr int MAXMERCS = 11;
constexpr const char* MercAutoAssistBucket = "merc_auto_assist";
constexpr const char* MercAnnounceBucket = "merc_announce";
constexpr const char* MercAreaNukesBucket = "merc_ae";
constexpr const char* MercFollowBucket = "merc_follow";
constexpr const char* MercHealOffsetBucket = "merc_heal";
constexpr uint8 MercFollowDistanceMin = 10;
constexpr uint8 MercFollowDistanceMax = 60;
constexpr uint8 MercHealOffsetMax = 20;
#define TANK 1
#define HEALER 2
#define MELEEDPS 9
#define CASTERDPS 12

#define NO_MERC_ID 0
#define MERC_STATE_NORMAL 5
#define MERC_STATE_SUSPENDED 1
#define NOT_SUSPENDED_TIME 0

const int MercAISpellRange = 100; // TODO: Write a method that calcs what the merc's spell range is based on spell, equipment, AA, whatever and replace this

struct MercSpell {
	int32 spellid; // <= 0 = no spell
	uint32 type; // 0 = never, must be one (and only one) of the defined values
	int16 stance; // 0 = all, + = only this stance, - = all except this stance
	int16 slot;
	uint16 proc_chance;
	uint32 time_cancast; // when we can cast this spell next
};

// Short merc chat lines; each kind repeats at most every few seconds so a long fight does not flood the chat window
enum class MercChatter : uint8 {
	Engage,
	Assist,
	Hold,
	Leash,
	LowMana,
	SkippedMez,
	Resurrect,
	Heal,
	Count
};

// Per-character choices made with $merc, kept in data buckets and loaded at zone-in
struct MercOwnerSettings {
	bool announce = true;
	bool area_nukes = true;
	bool hold = false; // not saved: $merc hold lasts until $merc back or the next zone-in
	uint8 follow_distance = 0; // 0 = the default distance
	uint8 heal_offset = 0; // percent added to the HP at which a healer heals the owner
};

struct MercTimer {
	uint16 timerid; // EndurTimerIndex
	uint8 timertype; // 1 = spell, 2 = disc
	int32 spellid; // <= 0 = no spell
	uint32 time_cancast; // when we can cast this spell next
};

class Merc : public NPC {
public:
	Merc(const NPCType* d, float x, float y, float z, float heading);
	virtual ~Merc();

	//abstract virtual function implementations requird by base abstract class
	bool Death(Mob* killer_mob, int64 damage, int32 spell_id, EQ::skills::SkillType attack_skill, KilledByTypes killed_by = KilledByTypes::Killed_NPC, bool is_buff_tic = false) override;
	void Damage(Mob* from, int64 damage, int32 spell_id, EQ::skills::SkillType attack_skill, bool avoidable = true, int8 buffslot = -1, bool iBuffTic = false, eSpecialAttacks special = eSpecialAttacks::None) override;
	bool Attack(Mob* other, int Hand = EQ::invslot::slotPrimary, bool FromRiposte = false, bool IsStrikethrough = false,
	bool IsFromSpell = false, ExtraAttackOptions *opts = nullptr) override;
	bool HasRaid() override { return false; }
	bool HasGroup() override { return (GetGroup() ? true : false); }
	Raid* GetRaid() override { return 0; }
	Group* GetGroup() override { return entity_list.GetGroupByMob(this); }

	// Mob AI Virtual Override Methods
	virtual void AI_Start(int32 iMoveDelay = 0);
	void AI_Stop() override;
	void AI_Process() override;

	//virtual bool AICastSpell(Mob* tar, int8 iChance, uint32 iSpellTypes);
	virtual bool AICastSpell(int8 iChance, uint32 iSpellTypes);
	bool AIDoSpellCast(int32 spellid, Mob* tar, int32 mana_cost, uint32* oDontDoAgainBefore = 0) override;
	bool AI_EngagedCastCheck() override;
	//virtual bool AI_PursueCastCheck();
	bool AI_IdleCastCheck() override;

	bool Process() override;

	// Static Merc Group Methods
	static bool AddMercToGroup(Merc* merc, Group* group);
	static bool RemoveMercFromGroup(Merc* merc, Group* group);
	void ProcessClientZoneChange(Client* mercOwner);
	void MercSay(MercChatter kind, const std::string& message, bool to_group = false);
	Corpse* GetGroupMemberCorpse();

	// Merc Spell Casting Methods
	int8 GetChanceToCastBySpellType(uint32 spellType);
	void SetSpellRecastTimer(uint16 timer_id, int32 spellid, uint32 recast_delay);
	void SetDisciplineRecastTimer(uint16 timer_id, int32 spellid, uint32 recast_delay);
	void SetSpellTimeCanCast(int32 spellid, uint32 recast_delay);
	static int32 GetSpellRecastTimer(Merc *caster, uint16 timer_id);
	static bool CheckSpellRecastTimers(Merc *caster, const MercSpell& merc_spell);
	static int32 GetDisciplineRecastTimer(Merc *caster, uint16 timer_id);
	static bool CheckDisciplineRecastTimers(Merc *caster, int32 spell_id, uint16 timer_id);
	static int32 GetDisciplineRemainingTime(Merc *caster, uint16 timer_id);
	static std::list<MercSpell> GetMercSpellsForSpellEffect(Merc* caster, int spellEffect);
	static std::list<MercSpell> GetMercSpellsForSpellEffectAndTargetType(Merc* caster, int spellEffect, SpellTargetType targetType);
	static std::list<MercSpell> GetMercSpellsBySpellType(Merc* caster, uint32 spellType);
	static MercSpell GetFirstMercSpellBySpellType(Merc* caster, uint32 spellType);
	static MercSpell GetFirstMercSpellForSingleTargetHeal(Merc* caster);
	static MercSpell GetMercSpellBySpellID(Merc* caster, int32 spellid);
	static MercSpell GetBestMercSpellForVeryFastHeal(Merc* caster);
	static MercSpell GetBestMercSpellForFastHeal(Merc* caster);
	static MercSpell GetBestMercSpellForHealOverTime(Merc* caster);
	static MercSpell GetBestMercSpellForPercentageHeal(Merc* caster);
	static MercSpell GetBestMercSpellForRegularSingleTargetHeal(Merc* caster);
	static MercSpell GetBestMercSpellForGroupHealOverTime(Merc* caster);
	static MercSpell GetBestMercSpellForGroupCompleteHeal(Merc* caster);
	static MercSpell GetBestMercSpellForGroupHeal(Merc* caster);
	static MercSpell GetBestMercSpellForAETaunt(Merc* caster);
	static MercSpell GetBestMercSpellForTaunt(Merc* caster);
	static MercSpell GetBestMercSpellForHate(Merc* caster);
	static MercSpell GetBestMercSpellForDelayedHeal(Merc* caster);
	static MercSpell GetBestMercSpellForCure(Merc* caster, Mob* target);
	static MercSpell GetBestMercSpellForStun(Merc* caster);
	static MercSpell GetBestMercSpellForAENuke(Merc* caster, Mob* target);
	static MercSpell GetBestMercSpellForTargetedAENuke(Merc* caster, Mob* target);
	static MercSpell GetBestMercSpellForPBAENuke(Merc* caster, Mob* target);
	static MercSpell GetBestMercSpellForAERainNuke(Merc* caster, Mob* target);
	static MercSpell GetBestMercSpellForNuke(Merc* caster);
	static MercSpell GetBestMercSpellForNukeByTargetResists(Merc* caster, Mob* target);
	static bool CheckAENuke(Merc* caster, Mob* tar, int32 spell_id, uint8 &numTargets);
	static bool GetNeedsCured(Mob *tar);
	bool HasOrMayGetAggro();
	bool ShouldHoldForHate();
	bool IsHoldingBackOffense();
	bool IsPassiveHealer();
	bool EngagesOnOwnerAttack();
	bool IsAssistStance();
	bool ShouldEngageOwnerTarget(Mob* owner);
	Mob* GetCalledTarget();
	const MercOwnerSettings& GetOwnerSettings();
	float GetFollowDistance();
	void CallAssist(Mob* target);
	bool CanRestNearOwner();
	bool IsTargetedByHostile();
	bool StepBackFromTarget();
	void PickTarget();
	static uint8 GetDefaultStance(uint32 merc_template_id);
	bool UseDiscipline(int32 spell_id, int32 target);

	bool IsMerc() const override { return true; }
	bool IsOfClientBotMerc() const override { return true; }

	void FillSpawnStruct(NewSpawn_Struct* ns, Mob* ForWho) override;
	static Merc* LoadMercenary(Client *c, MercTemplate* merc_template, uint32 merchant_id, bool updateFromDB = false);
	void UpdateMercInfo(Client *c);
	void UpdateMercStats(Client *c, bool setmax = false);
	void UpdateMercAppearance();
	void UpdateEquipmentLight() override;
	void AddItem(uint8 slot, uint32 item_id);
	static const char *GetRandomName();
	bool Spawn(Client *owner);
	void SendOwnerLinkToZone(bool spawned);
	void SendStatTable(bool full);
	bool Suspend();
	bool Unsuspend(bool setMaxStats);
	bool MercJoinClientGroup();
	void Zone();
	void Depop(bool start_spawn_timer = false) override;
	bool Save() override;
	bool GetDepop() { return p_depop; }

	bool IsDead() { return GetHP() < 0;};
	bool IsMedding() { return _medding; };
	bool IsSuspended() { return _suspended; };

	void Signal(int signal_id);
	void SendPayload(int payload_id, std::string payload_value);

	static uint32 CalcPurchaseCost( uint32 templateID , uint8 level, uint8 currency_type = 0);
	static uint32 CalcUpkeepCost( uint32 templateID , uint8 level, uint8 currency_type = 0);

	// "GET" Class Methods
	Mob* GetOwner() override;
	Client* GetMercenaryOwner();
	Mob* GetOwnerOrSelf() override;
	uint32 GetMercenaryID() { return _MercID; }
	uint32 GetMercenaryCharacterID( ) { return owner_char_id; }
	uint32 GetMercenaryTemplateID() { return _MercTemplateID; }
	uint32 GetMercenaryType() { return _MercType; }
	uint32 GetMercenarySubType() { return _MercSubType; }
	uint8 GetProficiencyID() { return _ProficiencyID; }
	uint8 GetTierID() { return _TierID; }
	uint32 GetCostFormula() { return _CostFormula; }
	uint32 GetMercNameType() { return _NameType; }
	uint8 GetStance() { return _currentStance; }
	int GetHatedCount() { return _hatedCount; }

	inline const uint8 GetClientVersion() const { return _OwnerClientVersion; }

	void SetTarget(Mob* mob) override;
	bool HasSkill(EQ::skills::SkillType skill_id) const;
	bool CanHaveSkill(EQ::skills::SkillType skill_id) const;
	uint16 MaxSkill(EQ::skills::SkillType skillid, uint16 class_, uint16 level) const;
	inline uint16 MaxSkill(EQ::skills::SkillType skillid) const { return MaxSkill(skillid, GetClass(), GetLevel()); }
	void DoClassAttacks(Mob *target) override;
	void CheckHateList();
	bool CheckTaunt();
	bool CheckAETaunt();
	bool CheckConfidence();
	bool TryHide();

	// stat functions
	virtual void ScaleStats(int scalepercent, bool setmax = false);
	void CalcBonuses() override;
	void AddMercEquipmentBonuses(StatBonuses *b);
	void RecalculateEquipmentBonuses();
	int64 GetEndurance() const {return cur_end;} //This gets our current endurance
	inline uint8 GetEndurancePercent() { return (uint8)((float)cur_end / (float)max_end * 100.0f); }
	inline int32 GetATK() const override { return ATK; }
	inline int32 GetATKBonus() const override { return itembonuses.ATK + spellbonuses.ATK; }
	int32 GetRawACNoShield(int &shield_ac) const;

	inline int32 GetSTR() const override { return STR; }
	inline int32 GetSTA() const override { return STA; }
	inline int32 GetDEX() const override { return DEX; }
	inline int32 GetAGI() const override { return AGI; }
	inline int32 GetINT() const override { return INT; }
	inline int32 GetWIS() const override { return WIS; }
	inline int32 GetCHA() const override { return CHA; }
	inline int32 GetMR() const override { return MR; }
	inline int32 GetFR() const override { return FR; }
	inline int32 GetDR() const override { return DR; }
	inline int32 GetPR() const override { return PR; }
	inline int32 GetCR() const override { return CR; }
	inline int32 GetCorrup() const override { return Corrup; }

	inline int32 GetHeroicSTR() const override { return itembonuses.HeroicSTR; }
	inline int32 GetHeroicSTA() const override { return itembonuses.HeroicSTA; }
	inline int32 GetHeroicDEX() const override { return itembonuses.HeroicDEX; }
	inline int32 GetHeroicAGI() const override { return itembonuses.HeroicAGI; }
	inline int32 GetHeroicINT() const override { return itembonuses.HeroicINT; }
	inline int32 GetHeroicWIS() const override { return itembonuses.HeroicWIS; }
	inline int32 GetHeroicCHA() const override { return itembonuses.HeroicCHA; }
	inline int32 GetHeroicMR() const override { return itembonuses.HeroicMR; }
	inline int32 GetHeroicFR() const override { return itembonuses.HeroicFR; }
	inline int32 GetHeroicDR() const override { return itembonuses.HeroicDR; }
	inline int32 GetHeroicPR() const override { return itembonuses.HeroicPR; }
	inline int32 GetHeroicCR() const override { return itembonuses.HeroicCR; }
	inline virtual int32 GetHeroicCorrup() const { return itembonuses.HeroicCorrup; }
	// Mod2
	inline virtual int32 GetShielding() const { return itembonuses.MeleeMitigation; }
	inline virtual int32 GetSpellShield() const { return itembonuses.SpellShield; }
	inline virtual int32 GetDoTShield() const { return itembonuses.DoTShielding; }
	inline virtual int32 GetStunResist() const { return itembonuses.StunResist; }
	inline virtual int32 GetStrikeThrough() const { return itembonuses.StrikeThrough; }
	inline virtual int32 GetAvoidance() const { return itembonuses.AvoidMeleeChance; }
	inline virtual int32 GetAccuracy() const { return itembonuses.HitChance; }
	inline virtual int32 GetCombatEffects() const { return itembonuses.ProcChance; }
	inline virtual int32 GetDS() const { return itembonuses.DamageShield; }
	// Mod3
	inline int32 GetHealAmt() const override { return itembonuses.HealAmt; }
	inline int32 GetSpellDmg() const override { return itembonuses.SpellDmg; }
	inline virtual int32 GetClair() const { return itembonuses.Clairvoyance; }
	inline virtual int32 GetDSMit() const { return itembonuses.DSMitigation; }

	inline virtual int32 GetSingMod() const { return itembonuses.singingMod; }
	inline virtual int32 GetBrassMod() const { return itembonuses.brassMod; }
	inline virtual int32 GetPercMod() const { return itembonuses.percussionMod; }
	inline virtual int32 GetStringMod() const { return itembonuses.stringedMod; }
	inline virtual int32 GetWindMod() const { return itembonuses.windMod; }

	inline int32 GetDelayDeath() const override { return aabonuses.DelayDeath + spellbonuses.DelayDeath + itembonuses.DelayDeath; }

	// "SET" Class Methods
	void SetMercData (uint32 templateID );
	void SetMercID( uint32 mercID ) { _MercID = mercID; }
	void SetMercCharacterID( uint32 ownerID ) { owner_char_id = ownerID; }
	void SetMercTemplateID( uint32 templateID ) { _MercTemplateID = templateID; }
	void SetMercType( uint32 type ) { _MercType = type; }
	void SetMercSubType( uint32 subtype ) { _MercSubType = subtype; }
	void SetProficiencyID( uint8 proficiency_id ) { _ProficiencyID = proficiency_id; }
	void SetTierID( uint8 tier_id ) { _TierID = tier_id; }
	void SetCostFormula( uint8 costformula ) { _CostFormula = costformula; }
	void SetMercNameType( uint8 nametype ) { _NameType = nametype; }
	void SetClientVersion(uint8 clientVersion) { _OwnerClientVersion = clientVersion; }
	void SetSuspended(bool suspended) { _suspended = suspended; }
	void SetStance(uint8 stance_id) { _currentStance = stance_id; }
	void SetHatedCount( int count ) { _hatedCount = count; }

	void Sit();
	void Stand();
	bool IsSitting() const override;
	bool IsStanding();

	// Merc-specific functions
	bool IsMercCaster() { return (GetClass() == HEALER || GetClass() == CASTERDPS); }
	bool IsMercCasterCombatRange(Mob *target);
	virtual float GetMaxMeleeRangeToTarget(Mob* target);
	virtual void MercMeditate(bool isSitting);
	bool FindTarget();
	bool LoadMercenarySpells();

protected:
	int64 GetFocusEffect(focusType type, int32 spell_id, bool from_buff_tic = false);

	std::vector<MercSpell> merc_spells;
	std::map<uint32,MercTimer> timers;

	Timer evade_timer; // can be moved to pTimers at some point

	uint16 skills[EQ::skills::HIGHEST_SKILL + 1];
	uint32 equipment[EQ::invslot::EQUIPMENT_COUNT]; //this is an array of item IDs
	uint32 d_melee_texture1; //this is an item Material value
	uint32 d_melee_texture2; //this is an item Material value (offhand)
	uint8 prim_melee_type; //Sets the Primary Weapon attack message and animation
	uint8 sec_melee_type; //Sets the Secondary Weapon attack message and animation

private:

	std::array<int32, MercenaryStat::Count> BuildStatTable();

	int32 CalcAC();
	int32 CalcATK();
	//int CalcHaste();

	int32 CalcSTR();
	int32 CalcSTA();
	int32 CalcDEX();
	int32 CalcAGI();
	int32 CalcINT();
	int32 CalcWIS();
	int32 CalcCHA();

	int32 CalcMR();
	int32 CalcFR();
	int32 CalcDR();
	int32 CalcPR();
	int32 CalcCR();
	int32 CalcCorrup();
	int64 CalcMaxHP();
	int64 CalcBaseHP();
	int64 CalcHPRegen();
	int64 CalcHPRegenCap();
	int64 CalcMaxMana();
	int64 CalcBaseMana();
	int64 CalcManaRegen();
	int64 CalcManaRegenValue(bool sitting) const;
	int64 CalcBaseManaRegen();
	int64 CalcManaRegenCap();
	void CalcMaxEndurance(); //This calculates the maximum endurance we can have
	int64 CalcBaseEndurance(); //Calculates Base End
	int64 GetMaxEndurance() const {return max_end;} //This gets our endurance from the last CalcMaxEndurance() call
	int64 CalcEnduranceRegen(); //Calculates endurance regen used in DoEnduranceRegen()
	int64 CalcEnduranceRegenCap();
	void SetEndurance(int32 newEnd); //This sets the current endurance to the new value
	void DoEnduranceUpkeep(); //does the endurance upkeep
	void CalcRestState();

	int GroupLeadershipAAHealthEnhancement();
	int GroupLeadershipAAManaEnhancement();
	int GroupLeadershipAAHealthRegeneration();
	int GroupLeadershipAAOffenseEnhancement();

	float GetDefaultSize();

	bool CheckStance(int16 stance);
	void AddNPCsHatingGroup();
	bool IsValidAssistTarget(Mob* target);
	bool IsMercBeyondLeash(Mob* owner);
	bool IsTargetBeyondLeash(Mob* owner, Mob* target);
	void EndCombatIfHateListEmpty();
	bool IsInCasterPosition(Mob* target);
	Mob* GetCasterAnchor(Mob* target);
	Mob* GetAssistTarget();
	Mob* GetTankTarget();
	std::unordered_map<uint16, Mob*>& GetNearbyMobs(float distance);
	const std::vector<MercSpell>& GetMercSpells() const { return merc_spells; }

	// Private "base stats" Members
	int32 base_mana;
	int32 base_end;
	int32 _baseAC;
	uint32 _baseSTR;
	uint32 _baseSTA;
	uint32 _baseDEX;
	uint32 _baseAGI;
	uint32 _baseINT;
	uint32 _baseWIS;
	uint32 _baseCHA;
	uint32 _baseATK;
	uint32 _baseRace; // Necessary to preserve the race otherwise mercs get their race updated in the db when they get an illusion.
	uint8 _baseGender; // Merc gender. Necessary to preserve the original value otherwise it can be changed by illusions.
	uint32 _baseMR;
	uint32 _baseCR;
	uint32 _baseDR;
	uint32 _baseFR;
	uint32 _basePR;
	uint32 _baseCorrup;
	uint32 RestRegenHP;
	uint32 RestRegenMana;
	uint32 RestRegenEndurance;

	uint32 _MercID;
	uint32 _MercTemplateID;
	uint32 _MercType;
	uint32 _MercSubType;
	uint8 _ProficiencyID;
	uint8 _TierID;
	uint8 _CostFormula;
	uint8 _NameType;
	uint8 _OwnerClientVersion;
	uint8 _currentStance;
	std::array<int32, MercenaryStat::Count> m_last_stat_table{};

	EQ::InventoryProfile m_inv;
	int64 max_end;
	int64 cur_end;
	bool _medding;
	bool _suspended;
	bool p_depop;
	bool m_merc_depopped = false; // Depop runs once: death, owner disconnect and ~Client can all reach it
	bool _check_confidence;
	bool _lost_confidence;
	int _hatedCount;
	std::array<float, MAX_GROUP_MEMBERS> last_heal_check_hpr{};
	uint8 nuke_cast_count;
	float melee_range_factor;
	uint16 melee_range_target_id;
	uint32 owner_char_id;
	Timer endupkeep_timer;
	Timer rest_timer;
	Timer confidence_timer;
	Timer check_target_timer;
	Timer pick_target_timer;
	Timer engaged_buff_timer;
	uint32 retarget_allowed_time;
	uint32 follow_range_since;
	std::array<uint32, static_cast<size_t>(MercChatter::Count)> chatter_next_time{};
	uint32 step_back_allowed_time = 0;
	uint32 step_back_until = 0;
	uint16 called_target_id = 0;
	uint32 called_until = 0;
	uint32 called_heal_until = 0;
	glm::vec4 follow_owner_position;
};
