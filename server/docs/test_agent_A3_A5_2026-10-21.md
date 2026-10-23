# Test agent instruction: A3–A5 (2026-10-21)

Paste everything below the line into the test agent session.

---

Three player-facing fixes for Ascendant, all on test first:
- **A3:** a quest edit.
- **A4:** a quest edit.
- **A5:** a custom DB migration.

**Owner policy:** Ascendant is solo-friendly. One character of any class must be able to progress, so group-size or raid-size gates on trials, events and DZ requests are bugs.

**Quest files** live in `server/quests` (not in git). Back up every file you touch as `<file>.bak-20261021` next to it. Report a unified diff for each file so the owner can roll it to prod.

**DB changes** go in `common/database/database_update_manifest_custom.h` as the next version after the A1/A2 migrations (14/15), so **16**. Bump `CUSTOM_BINARY_DATABASE_VERSION` in `common/version.h` to match. Also confirm the A1/A2 commit is pushed to `origin/ascendant`; as of this writing it wasn't there.

## A3. Ikkinz raid trial 4 (Sentinel of the Altar) still requires a raid: solo blocker at ikky=14

**File:** `kodtaz/Sentinel_of_the_Altar.pl` (hand-in of Icon of the Altar 60173).

Today, lines 6-26 require `$client->GetRaid()`:
- No raid → "You are not in a raid!" and no instance.
- With a raid → 54-member cap, `ikkylockout6` check, `quest::CreateInstance("ikkinz", 6, 21600)`, then `quest::AssignRaidToInstance`.

Trials 1-3 were already made solo-friendly in `kodtaz/Tublik_Narwethar.pl`. Copy its pattern:
- `sub ASSIGN_IKKY_INSTANCE` (around :357-367): raid → `AssignRaidToInstance`, group → `AssignGroupToInstance`, else `AssignToInstance`.
- Also check how Tublik reuses a party member's existing instance (`PARTY_IKKY_INSTANCE`, around :370-383, used at :93/:122/:151). Do the same here, so a group whose member already has instance 6 joins it rather than making a second one.

**Change:**
- Drop the `if($raid) … else "You are not in a raid!"` wrapper.
- Keep the 54-player cap, but only when a raid exists.
- Keep the `ikkylockout6` check, the `GetInstanceID("ikkinz",6)` "already in an instance" check, `MarkCompassLoc` and the item return as they are.
- Replace `AssignRaidToInstance` with the raid/group/solo assign.

**Also check:** make sure nothing *inside* the trial 4 instance (`ikkinz/` scripts for version 6, e.g. Keeper of the Altar 294620 and the altar triggers) checks raid membership or a minimum player count. List anything you find.

**Test:**
1. A solo char at `ikky = 14` holding 60173 hands it in, gets the instance, zones in, and can reach Keeper of the Altar.
2. A 2-char group: both are assigned.
3. A raid still works.
4. A char without 60173 / `ikky != 14` gets nothing.

## A4. Pxet Elites can never spawn in a DZ (Ferubi, Smith Rondo / Breakdown in Communication #4)

**File:** `ferubi/Event_Spawner.lua` (npc 284110). Discord: "Smith Rondo event in Ferubi".

**How it works now:**
- Camp mobs signal 284110 on `event_death_complete` according to their spawngroup (e.g. `ferubi/an_aneuk_seer.lua`):
  - signal 1 = blademaster (287779/287826)
  - signal 2 = spearmaster (287782)
  - signal 3 = hammermaster (287783)
  - signal 4 = brawler (287784/38637)
- Each counter spawns its elite on **exactly the 100th** kill (`== 100`, lines 27/36/45/54), then resets after 3s.
- Counters are per instance via `inst_key()`.

**The bug:**
- One full DZ clear has only about 66 / 46 / 43 / 24 spawn points for those camps, and DZ mobs don't respawn. So the elites can never spawn in a DZ.
- data_buckets on prod show DZ counters stuck at exactly those numbers (e.g. inst131: blade 66, spear 46, brawler 25).
- Instance keys (`ferubi-*-count-inst<id>`) also have no expiry, so they leak into a later DZ that reuses the instance id.

**Change:**
1. Use a per-instance threshold:
   ```lua
   local function need() return (eq.get_zone_instance_id() == 0) and 100 or 20 end
   ```
   Then change the four `== 100` checks to `>= need()`.
   - 20 is under every camp's spawn count, including brawler at 24.
   - Run your own count of spawn points per camp (spawngroups above × spawn2 rows in ferubi version 0). If any camp has fewer than 20, lower the DZ threshold to fit and report it.
2. Expire the instance keys. When `eq.get_zone_instance_id() ~= 0`, call `eq.set_data(key, value, <expiry>)`.
   - Use the instance's remaining time if the Lua API exposes it (check `get_quest_api_doc` / lua_general for an instance-timer getter).
   - Otherwise use `"24h"`.
   - Leave the open-world keys without an expiry (unchanged).
3. Don't change the open-world threshold (100) unless the owner says so. Note it as an owner call in your report: 100 kills × 4 camps is a heavy solo grind.

**Test:**
- In a Ferubi DZ (normal and raid mode), with debug lines temporarily un-commented or a GM watching data_buckets, kill 20 blademaster-camp mobs. Pxet_Elite_Blademaster 284039 spawns once, and the counter resets.
- Repeat for one other camp.
- Open-world behavior is unchanged.

## A5. Coral Hilted Shortsword bard swarm pet mezzes and fears its own target

Discord: #suggestions-and-feedback "Coral Hilted Shortsword". It's really a bug.

**The data:**
- Item 24594 (and 724594) procs spell 3639 Call of Rizlona, which summons npc 654 `SwarmPetBard` (L60).
- That NPC uses `npc_spells_id = 11` "Default Bard List". At L60 the list includes mez (868, 1100, 1197, 1753), fear (1756), memblur (1451), charm (750) and others, mostly typed as nukes. So the pet casts them on its own target: mezzing it ends the fight, and the mob regens.
- List 11 is shared by 9 npc_types (verified), so **don't edit list 11**.
- Only spell 3639 points at SwarmPetBard.

**Migration 16** (description `2026_10_21_bard_swarm_pet_no_mez`):
- `.check` = `SELECT id FROM npc_types WHERE id = 654 AND npc_spells_id = 11`
- `.condition = "not_empty"`
- `.sql` = `UPDATE npc_types SET npc_spells_id = 0 WHERE id = 654;`
- Rollback (commit message only): `UPDATE npc_types SET npc_spells_id = 11 WHERE id = 654;`

This makes it a melee-only pet. It's short-lived, so losing its songs is fine. The owner can ask later for a dedicated list with only DoTs and buffs.

**Test:** equip the shortsword on test and fight until it procs. The pet melees the target and never mezzes, fears or charms it.

## Deliverable

Report back with:
- the A3 and A4 unified diffs plus backup file names
- the commit hash for migration 16 + version bump (and the pushed A1/A2 hash)
- your spawn-point counts for A4
- anything found inside the Ikkinz trial 4 instance (A3)
- the in-game test results

**Prod rollout:** with the next release build and reboot. The owner copies the A3/A4 quest files to prod; the migration runs on boot.
