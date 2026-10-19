# Ascendant Server — Custom Systems Reference

> This document describes custom systems unique to the Ascendant EQEmu server.
> These are NOT part of stock EQEmu.

---

## Server Architecture

- **Server 1:** 51.81.106.189 — World, Login, UCS, QueryServ, SharedMemory, zones
- **AkkStack Docker** — All services in Docker containers
- **Container path:** `/home/eqemu/server/` inside container = `/opt/akk-stack/server/` on host
- **Quest scripts must use container paths** for file I/O (e.g., `/home/eqemu/server/logs/`)
- **Spire** web admin tool for management

### Key Directories
- `quests/global/` — Global scripts (global_npc.pl, global_player.pl)
- `quests/plugins/` — Perl plugin library (auto-loaded by all zones)
- `quests/lua_modules/` — Lua module library
- `quests/{zoneshortname}/` — Zone-specific scripts
- `quests/{zoneshortname}/encounters/` — Encounter scripts

---

## Encounter Scaling System

**File:** `quests/plugins/ascendant_encounter_scaling.pl`

Dynamically scales NPC stats based on nearby player count using a logarithmic curve. Solo players face weaker mobs; groups face stronger.

### Stat Categories Scaled
- **HP** — Health pool multiplier
- **Melee** — min_hit, max_hit multiplier
- **ATK** — Attack multiplier
- **Resists** — Flat resist delta (negative = easier)

### NPC Tiers
- **Raid** — `raid_target = 1` in npc_types
- **Named** — `rare_spawn = 1` in npc_types
- **Trash** — Everything else

### Current Values (with +15% increase, melee/ATK can exceed 1.00)

**Raid:**
| Stat | Solo floor | Solo ceil | Group base | Group cap |
|------|-----------|-----------|------------|-----------|
| HP | 0.31 | 0.60 | 0.63 | 0.81 |
| Melee | 0.77 | 1.00 | 1.00 | 1.20 |
| ATK | 0.66 | — | 0.91 | 1.15 |
| Resists | -101 | -32 | -23 | -4 |

**Named:**
| Stat | Solo floor | Solo ceil | Group base | Group cap |
|------|-----------|-----------|------------|-----------|
| HP | 0.37 | 0.61 | 0.68 | 0.81 |
| Melee | 0.81 | 1.00 | 1.00 | 1.15 |
| ATK | 0.75 | — | 0.99 | 1.10 |
| Resists | -82 | -17 | -14 | -3 |

**Trash:**
| Stat | Solo floor | Solo ceil | Group base | Group cap |
|------|-----------|-----------|------------|-----------|
| HP | 0.51 | 0.77 | 0.80 | 0.81 |
| Melee | 0.96 | 1.00 | 1.00 | 1.10 |
| ATK | 0.91 | — | 1.00 | 1.05 |
| Resists | -50 | -11 | -8 | -2 |

### Key Notes
- Multipliers > 1.00 allowed for melee/ATK (buffs mobs above DB baseline in large groups)
- HP capped at 1.00 (no buff above DB stats)
- Scaling uses log curve ramping with group size
- Applied via `ModifyNPCStat()` in EVENT_COMBAT/EVENT_SPAWN hooks

---

## Fellowship Bonus System

**Plugin:** `quests/plugins/ascendant_fellowship.pl`
**Hook:** `quests/global/global_player.pl` (EVENT_GROUP_CHANGE, EVENT_ENTERZONE, 30s timer)
**NPC hooks:** `quests/global/global_npc.pl` (EVENT_COMBAT, EVENT_DEATH)

Rewards players who group with unique real players (anti-multibox detection via IP cross-reference).

### Tiers

| Tier | Unique Players | Buff Spell | XP Spell |
|------|---------------|------------|----------|
| Bronze | 2 | 29433 (Ascendant Fellowship I) | 14629 (+5% XP) |
| Silver | 3 | 29434 (Ascendant Fellowship II) | 13088 (+10% XP) |
| Gold | 4+ | 29435 (Ascendant Fellowship III) | 13089 (+25% XP) |

### Buff Stats by Tier

| Stat | Bronze | Silver | Gold |
|------|--------|--------|------|
| Melee crit % | +2 | +4 | +7 |
| Spell crit % | +2 | +4 | +7 |
| Cast time reduction | -2% | -4% | -6% |
| Mana cost reduction | -2% | -3% | -5% |
| Pet power | +1 | +2 | +3 |

### Bonus Loot
- **Bonus shard** (item 9600): Bronze 1/60, Silver 1/40, Gold 1/30
- **Bonus named loot pool**: Bronze 3%, Silver 7%, Gold 12%
  - Uses `plugin::get_merged_pool($npc_lvl, $zoneid)` for expansion-aware items

### Mob Strength Scaling
- Bronze: +5%, Silver: +10%, Gold: +15% (HP, min_hit, max_hit, AC)
- Applied in EVENT_COMBAT after encounter scaling
- Snapshots pre-fellowship stats, restores on disengage
- Gated: `!$npc->IsPet() && $npc->GetLevel() > 1`

### Unique Player Detection
- Uses `account_ip` table for historical IP cross-reference
- Union-find algorithm groups accounts sharing IPs
- Result cached via data bucket (60s TTL)

### Plugin Functions
- `Fellowship_ApplyBuff($client)` — Evaluate and apply/fade fellowship + XP spells
- `Fellowship_FadeAll($client)` — Fade all fellowship + XP spells
- `Fellowship_GetCurrentTier($client)` — Returns 0-3 tier from entity variable
- `Fellowship_ScaleMob($npc, $tier)` — Apply mob strength multiplier
- `Fellowship_RestoreMob($npc)` — Restore mob to pre-fellowship stats
- `Fellowship_BonusLoot($npc, $client, $tier)` — Roll bonus shard + named loot

---

## Pet Bag System

**Plugin:** `quests/plugins/ascendant_pet_bag_system.pl`

Allows pet classes to equip items on their pets via a special bag.

### Bag Items
- **Original:** item 93861 (Ascendant Pet Bag, 6-slot) — all pet classes
- **Caster:** item 2828 (Ascendant Casters Pet Bag, 10-slot) — Enc/Mag/Wiz/Nec only (classes bitmask 15360)

### Class Priority in FindPetBag()
- Enc(11)/Mag(13)/Wiz(14)/Nec(10): Searches for caster bag (2828) first, falls back to original (93861)
- All other pet classes: Original bag (93861) only

### How Items Apply to Pets
- `AddItem()` applies: HP, STR, STA, AGI, DEX, INT, WIS, CHA, all resists
- `AddItem()` does NOT apply: AC, ATK, Min/Max Hit, Attack Delay — these use `ModifyNPCStat()`
- Weapon procs via `AddMeleeProc()` — hard cap of 2 procs per pet

### Spell Focus
- Accumulates `spelldmg` and `healamt` from all bag items via `GetItemStat()`
- Applied via `$npc->SetSpellFocusDMG($spelldmg_bonus * $FOCUS_DMG_MULT)`
- `$FOCUS_DMG_MULT = 3`, `$FOCUS_HEAL_MULT = 3` (tunable)
- SpellFocusDMG is hardcoded to 0 in npc.cpp — only settable via `SetSpellFocusDMG()`

### DPS Formula
- **dmg_bonus** = `($level >= 28) ? 1 + int(($level - 28) / 3) : 0`
- **Weapon DPS** = `(damage * 2.0 + dmg_bonus) / (item_delay / haste_factor)`
- **Weapon max_hit** = `int(damage * 3.0 + dmg_bonus)`
- **haste_factor** = `1 + (GetHaste() / 100)`

### Data Key Scoping
- All keys scoped per `${char_id}_${pet_eid}` (character ID + pet entity ID)
- `petbag_base:${char_id}_${pet_eid}` — Baseline stats snapshot (first equip only)
- `petbag_active_delay:${char_id}_${pet_eid}` — Active delay (raw)
- All keys use 14400s TTL (4 hours)

### Re-equip Behavior
- `ClearItemList()` wipes loot display but does NOT revert stat bonuses
- Baseline stats restored via `ModifyNPCStat()` before re-adding items
- Baseline only stored once (first equip per pet entity)

### Proc Safety
- Procs applied ONCE per pet lifetime via `SetEntityVariable('petbag_procs_applied', 1)`
- Charm pets: procs enabled but may persist after charm break (no `RemoveMeleeProc` exists)
- Hard cap: 2 procs via counter

### ShowPetStats
- Uses `quest::popup()` — NOT DiaWind
- Shows: base stats, equipped bonuses, weapon DPS, procs, spell focus

---

## Philanthropist NPC (Ben Affactor)

**NPC script:** `quests/guildlobby/Ben_Affactor.pl` (also `quests/cshome/`)
**Plugin:** `quests/plugins/ascendant_philanthropist.pl`
**Pickup hook:** `quests/global/global_player.pl` (EVENT_ENTERZONE + 120s timer)
**SQL schema:** `quests/guildlobby/philanthropist_tables.sql`

### How It Works
High-level players donate platinum; it pools and periodically distributes to low-level eligible players.

### Config (Plugin)
- **Pool threshold:** 10,000pp to trigger distribution
- **Distribution timer:** Random 45min-3hrs
- **Daily cap per recipient:** 5,000pp
- **Recipient criteria:** Level < 51, time_played < 900 minutes (15 hours)
- **Online detection:** last_login > (UNIX_TIMESTAMP() - 600)
- **Excluded zones:** 151 (bazaar), 344 (guildlobby)

### Config (NPC Script)
- **Min donation:** 100pp, **Max:** 50,000pp
- **Confirm threshold:** 10,000pp
- **Donor cooldown:** 10s (account-wide, anti-spam)
- **Donor min level:** 10

### Donation Methods
1. **Saylinks:** 100/500/1K/5K or `give X` for custom
2. **Direct platinum trade:** In trade window
3. **Item hand-in:** Parceled to nearest eligible, platinum via trade window

### Architecture
- Instance 0 only runs distribution timer (EVENT_SPAWN checks `$instanceid == 0`)
- All shard instances handle donations
- Pickup runs per-player from global_player.pl (120s timer, any zone)

### GM Menu (4 options, `$client->GetGM()` gated)
- **Show Eligible** — List online recipients
- **Force Distribute** — Run distribution ignoring 10K threshold
- **Pool Status** — Pool total, donors, pending grants, lifetime stats
- **Clear Pool** — Delete all undistributed deposits

### World broadcast color: 14 (green)

---

## Gambling NPC (Harley Wynn)

**NPC scripts:** `quests/guildlobby/Harley_Wynn.pl` and `quests/cshome/Harley_Wynn.pl` (kept in sync)
**Plugin:** `quests/plugins/ascendant_gambling.pl`
**AA spell:** `quests/global/spells/27086.pl`

### Lucky Coins
- **Item ID:** 1378
- **Cost:** 1,500pp each, buy 1/10/50 via saylinks
- **Max per trade:** 10 coins
- **AA spell 27086:** Deducts 3 AA points and summons 1 Lucky Coin

### Odds

| Tier | Chance | ~Rolls |
|------|--------|--------|
| Legendary | 0.25% | ~400 |
| Jackpot | 1% | ~100 |
| Exceptional | 4% | ~25 |
| Rare | ~20% | ~5 |
| Uncommon | ~25% | ~4 |
| Common | ~50% | ~2 |

### Implementation Notes
- `quest::varlink()` must be called in NPC script context (not plugin) — causes "item 0" if called in plugin
- `quest::getitemstat($item_id, "loreflag")` used for lore check
- `DoGamble` returns `($tier_name, $item_id)` — messaging done in NPC script
- All NPC text uses `plugin::Whisper()`
- `EVENT_ITEM` uses `quest::handin` while loop pattern

---

## Buff Bag System

**Plugin:** `quests/plugins/ascendant_buff_bag_system.pl`
**EVENT_CAST handler:** `quests/global/global_player.pl` (lines 585-598)

### Items
- **Satchel:** item 17672 (6-slot container)
- **Wand:** item 17673 (clickeffect spell 17782)
- **Scrolls:** items 17665-17671

### Scroll Effects
| Item ID | Buff |
|---------|------|
| 17665 | Aegolism |
| 17666 | Chloroplast |
| 17667 | Focus |
| 17668 | Clarity |
| 17669 | Damage Shield |
| 17670 | DMF (Dead Man Floating) |
| 17671 | Strength of Nature |

### Buff Application
- `$client->ApplySpell($spell_id, 3600, 50, 0, 0)` — 60min duration at level 50

---

## Crystallize Essence (Forge) System

### Forge Spells
| Spell ID | Class |
|----------|-------|
| 26716 | Cleric |
| 26717 | Druid |
| 26718 | Shaman |
| 26719 | Enchanter |
| 26720 | Magician |
| 26721 | Necromancer |
| 26722 | Ranger |

### AA Ranks: 40000-40006

### Mechanics
- **XP cost:** 65% of current level band (`GetEXPForLevel(level+1) - GetEXPForLevel(level)`)
- **Cooldown:** 6 hours via `quest::set_data()` with auto-expiry
- **Min level:** 50

---

## LDON Unlock Event

### Zone Gating (3 layers)
1. `zone.content_flags = 'ldon'` on all expansion=6 zones — gates spawns/merchants/objects
2. `zone.min_status = 250` — blocks non-GM zone entry
3. `zone.expansion = 6` — blocks entry if server expansion < 6
4. `content_flags.enabled = 0` for `ldon` flag

### Relic Drop
- **Item:** 9544 (Lost Dungeon Relic)
- **Drop rate:** 1/2000 from any mob
- **Condition:** Only drops while `ldon` flag is disabled
- **Hook:** `global_npc.pl` EVENT_SPAWN

### NPC: A Dust Covered Wayfarer
- **NPC ID:** 26051 (Troll Warrior, level 70)
- **Script:** `quests/guildlobby/A_Dust_Covered_Wayfarer.pl` (also `cshome`)
- **Dialogue:** All `plugin::Whisper()`, pre/post-unlock modes
- **Counter:** `quest::get_data("ldon_relic_count")` / `quest::set_data()`
- **Milestones:** World emote (orange, color 15) every 50 relics

### At 250 Relics
1. `quest::set_content_flag("ldon", 1)` — enables spawns/merchants
2. `UPDATE zone SET min_status = 0, expansion = 2 WHERE expansion = 6` — opens zone entry
3. World emote: "The earth trembles as ancient seals shatter..."

### Manual Go-Live SQL
```sql
UPDATE zone SET min_status = 0, expansion = 2 WHERE expansion = 6;
UPDATE content_flags SET enabled = 1 WHERE flag_name = 'ldon';
```
Then `#reloadcontentflags` and `#reloadworld`

### Key: content_flags on zone table does NOT gate zone entry
Only gates spawn2/objects/merchantlist. Zone entry is gated by `min_status` and `expansion`.

---

## Legacy of Ykesha Release (2026-09)

Manual gate release (no community event). LoY = `zone.expansion = 5`; gated
only by `Expansion:CurrentExpansion` (4 -> 5). `nedaria` additionally had a
custom `min_status = 20`.

### Zones
| ID | Zone | Notes |
|----|------|-------|
| 182 | nedaria | Hub. Treated as **classic** for loot/scaling (like bazaar/jaggedpine) |
| 224 | gunthak | L35-50 nameds |
| 225 | dulak | Captain Varns, Quigli (nameds — not raid per Ali) |
| 226 | torgiran | Event bosses Overseer Wrank / Overlord Ngrub as rares |
| 227 | nadox | Luggald Broodmother + #Innoruuk (raid, scripted events); Spiritseeker Nadox, Bloodspear (nameds) |
| 228 | hatesfury | #Captain_Krasnok (raid, script-spawned); ship nameds as rares |
| 277 | chardokb | #Korucust (raid) |
| 278 | soldungc | #Culthor_the_Gatekeeper (named — not raid per Ali) |

### Quest-side integration (done)
- `plugins/ascendant_encounter_scaling_loy.pl` — resist-only (PoP style), kill switch `$LOY_SCALING_ENABLED`
- `global/global_npc.pl` — `LoyScaling_OnEngage/OnDisengage/Rescan` dispatch, `IsLoyScalingZone` exclusion (line ~149), `'loy'` added to raid level-block loot gate (line ~337)
- `plugins/ascendant_loot_utils.pl` — `'loy'` appended to `active_expansions()` (after `pop`, so it uses the DB rare_spawn loottable roll), dispatch branches added
- `plugins/ascendant_loot_pools.pl` — `loy_zone_ids`, `loy_rare_pools` (empty, DB roll), `loy_level_blocks` (1-44 / 45-53 / 54-59 / 60+), `loy_raid_pools` (block 1 <=65: Nadox + Hate's Fury drops; block 2 66+: Korucust/Varns/Culthor), `loy_raid_level_blocks`
- `plugins/ascendant_expedition_config.pl` — all 8 zones; raid tier on nadox, hatesfury, chardokb (rev 3, 2026-09-25)

### DB scripts (`server/sql/`, apply in order, each has a rollback)

**Status 2026-09-25:** steps 1, 2 and 3 APPLIED (flags: 160 rare / 5 raid; 8 Tyrael spawns; rule 4->5 + nedaria min_status 0 at 21:36, live on the following reboot). GMs with `#gm on` bypass the expansion check and can test LoY zones before release. Table backup: `backup/loy_prep_20260925/`.
1. `2026_09_04_loy_named_flags.sql` (rev 3) — raid_target = Ali's list only (Korucust, Krasnok, Broodmother x2, Innoruuk); Varns, Quigli, Spiritseeker Nadox x2, Bloodspear, Culthor are `rare_spawn`. 156 listed `rare_spawn` in three data-driven tiers (A: shared-slot chance<=50 in multi-entry spawngroup, B: static 100% named with own loottable, C: script-spawned event nameds — mirrors PoP's 156/132/123 split) + 4 demoted raid_targets = 160 rare, 5 raid, un-flags trigger NPCs 228118 `#spell_target` and 227330 `_` (would otherwise be depopped in Normal expeditions and break Hate's Fury / Nadox events). 80%-slot `#` NPCs (the *common* entry) and quest NPCs deliberately excluded; opt-in block at the bottom.
2. `2026_09_04_loy_planeshifter_tyrael_spawns.sql` — Tyrael (npc 186202) at each zone's safe point
3. `2026_09_04_loy_gate_release.sql` — rule 4 -> 5, nedaria `min_status = 0`; then `#reload content_flags global` (reloads rules + expansion in world and all zones), `#reload merchants global`, `#reload zone_data global`, and `#repop` in cazicthule (285 LoY-era spawns turn on there)

Already applied (2026-09-25): `2026_09_25_loy_raid_ascendant_boost.sql` — T3 (+700000) of the 46 `loy_raid_pools` items raised to PoP-by-boss-level (block 1 -> PoP1, block 2 -> PoP2 + Luclin block-4 heroic pass). Items live in shared memory: takes effect at the next normal restart (never `#hotfix`).

### Adding an expansion — checklist (reuse for GoD)
1. New `plugins/ascendant_encounter_scaling_<exp>.pl` (copy pop module, rename `Pop`->`<Exp>`, `POP_`->`<EXP>_`, `_pop_`->`_<exp>_`, replace zone map)
2. `global_npc.pl`: add `<Exp>Scaling_*` to EVENT_COMBAT (engage + disengage), EVENT_TIMER rescan, and the `Is<Exp>ScalingZone` exclusion in EVENT_SPAWN
3. `ascendant_loot_utils.pl`: `active_expansions()`, `_expansion_{pool,blocks,zone,raid_pool,raid_blocks}_dispatch`
4. `ascendant_loot_pools.pl`: `<exp>_zone_ids`, `<exp>_rare_pools`, `<exp>_level_blocks`, `<exp>_raid_pools`, `<exp>_raid_level_blocks`; check the zone isn't already claimed by an earlier expansion's zone_ids (first match wins)
5. `global_npc.pl` line ~337: add `<exp>` to the raid level-block loot gate
6. `ascendant_expedition_config.pl`: zone entries + `has_raid_tier`
7. SQL: `rare_spawn`/`raid_target` flags (PEQ ships few), un-flag `raid_target` on trigger NPCs, Tyrael spawns, rule bump
8. `ascendant_insight_trainer.pl` `$CURRENT_MAX_EXPANSION` if the expansion adds AAs

---

## GM Command Audit Logging

**Hook:** `EVENT_GM_COMMAND` in `quests/global/global_player.pl`

### Exported Variable
- `$message` — Full command string

### Storage
- **DB table:** `gm_audit_log` (id, timestamp, account_id, account_status, char_name, zone, command)
- **Flat file:** `/home/eqemu/server/logs/gm_audit.log` (container path)
  - Host path: `/opt/akk-stack/server/logs/gm_audit.log`
- Uses `plugin::LoadMysql()` for DB inserts
- Logs all accounts with status > 0

### Important Notes
- `EVENT_COMMAND` does NOT work for built-in # commands
- `EVENT_SAY` does NOT fire for built-in # commands (handled in C++ before quest system)
- `EVENT_GM_COMMAND` is the only reliable hook for all built-in commands

---

## Custom Plugin Functions

### ascendant_encounter_scaling.pl
- `ScaleEncounter($npc, $player_count)` — Apply encounter scaling

### ascendant_fellowship.pl
- `Fellowship_ApplyBuff($client)` — Evaluate and apply fellowship buff
- `Fellowship_FadeAll($client)` — Remove all fellowship buffs
- `Fellowship_GetCurrentTier($client)` — Get current tier (0-3)
- `Fellowship_ScaleMob($npc, $tier)` — Scale mob for fellowship
- `Fellowship_RestoreMob($npc)` — Restore mob stats
- `Fellowship_BonusLoot($npc, $client, $tier)` — Roll bonus loot

### ascendant_pet_bag_system.pl
- `FindPetBag($client)` — Find pet bag in inventory (returns bag slot or 0)
- `EquipPetFromBag($client, $npc, $bag_slot)` — Equip pet from bag contents
- `ShowPetStats($client, $npc)` — Display pet stats popup

### ascendant_philanthropist.pl
- `Philanthropist_Donate($client, $npc, $amount)` — Process donation
- `Philanthropist_Distribute()` — Run distribution cycle
- `Philanthropist_Pickup($client)` — Check and deliver pending grants

### ascendant_gambling.pl
- `DoGamble($client, $npc)` — Roll gambling result, returns ($tier_name, $item_id)
- `GetGamblePool($level, $zone)` — Get level/zone-appropriate item pool

### ascendant_buff_bag_system.pl
- `ProcessBuffBag($client, $spell_id)` — Process buff bag wand click

---

## Global Script Hook Summary

### global_npc.pl (EVENT_SPAWN)
- LDON Relic drop (1/2000, gated by content flag)
- Encounter scaling initialization

### global_npc.pl (EVENT_COMBAT)
- Encounter scaling application
- Fellowship mob strength scaling

### global_npc.pl (EVENT_DEATH)
- Fellowship bonus loot rolls
- Bonus shard drops

### global_player.pl (EVENT_ENTERZONE)
- Fellowship buff recheck
- Philanthropist pickup timer start

### global_player.pl (EVENT_GROUP_CHANGE)
- Fellowship buff evaluation

### global_player.pl (EVENT_CAST)
- Buff bag wand processing (spell 17782)
- Crystallize Essence processing (spells 26716-26722)

### global_player.pl (EVENT_GM_COMMAND)
- GM audit logging

### global_player.pl (EVENT_TIMER)
- Fellowship 30s recheck timer
- Philanthropist 120s pickup timer

---

## Item ID Quick Reference

| ID | Item | System |
|----|------|--------|
| 93861 | Ascendant Pet Bag (6-slot) | Pet Bag |
| 2828 | Ascendant Casters Pet Bag (10-slot) | Pet Bag |
| 1378 | Lucky Coin | Gambling |
| 9544 | Lost Dungeon Relic | LDON Unlock |
| 9600 | Bonus Shard | Fellowship |
| 17672 | Buff Satchel | Buff Bag |
| 17673 | Buff Wand | Buff Bag |
| 17665-17671 | Buff Scrolls | Buff Bag |

## Spell ID Quick Reference

| ID | Spell | System |
|----|-------|--------|
| 17782 | Buff Wand Click | Buff Bag |
| 26716-26722 | Crystallize Essence | Forge |
| 27086 | Lucky Coin AA | Gambling |
| 29433 | Ascendant Fellowship I | Fellowship |
| 29434 | Ascendant Fellowship II | Fellowship |
| 29435 | Ascendant Fellowship III | Fellowship |
| 14629 | Learner's Effect (+5% XP) | Fellowship |
| 13088 | Potion of Adventure I (+10% XP) | Fellowship |
| 13089 | Potion of Adventure II (+25% XP) | Fellowship |

## AA ID Quick Reference

| ID | AA | System |
|----|-----|--------|
| 40000-40006 | Crystallize Essence (per class) | Forge |
