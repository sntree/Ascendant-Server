# #god-bug-reports: ARCHIVED threads (13), triage 2026-10-21

All checks were read-only: quest files under /opt/akk-stack/server/quests, DB SELECTs, code in /opt/akk-stack/code (HEAD 5452bb91f, live since the 2026-10-18 restart).
Short version: every staff fix claimed in these threads is in the current files, DB or code. One related solo gate is still live (Sentinel of the Altar, see the extra finding at the end).

---

### Sone worker (1560467442583740436, created 2026-10-16, last activity unknown)
- Summary: the thread is empty. 0 messages, and the starter post is gone (get_message on the thread id returns 404). Probably deleted by the poster.
- Category: none (no content)
- Status: nothing to check.
- Evidence checked: discord_read_channel since 60d and since 2026-10-01 both return no messages. confidence high
- Recommended action: none (leave archived; deleting it is optional)
- Suggested reply: none

### Instance required (1560282081785815153, created 2026-10-15, last activity 2026-10-15 17:01)
- Summary: Truk had an Ikkinz raid-trial instance (Glorified) but could not re-enter after dying. Tublik would not issue a new one ("already have it").
- Category: bug (entrance usability), now fixed
- Status: resolved in-thread. Straps re-marked the compass and promised bigger entrance triggers after the restart, and Truk confirmed it works. Verified that the fix is live:
  - `kodtaz/#Zoner4.pl`, `#Zoner5.pl`, `#Zoner6.pl` (mtime Oct 15 16:48): proximity is now ±25 (was ±10), with a 10-second grace window after zoning in (`kt_zonein`, set in `kodtaz/player.lua`).
  - `kodtaz/Tublik_Narwethar.pl:87-101`: the raid requirement is gone (`if (1)`). It joins a party member's existing instance (`PARTY_IKKY_INSTANCE`), and when you already hold the instance it says so and re-marks the compass.
  - Live since the 10-18 restart. Truk is now ikky=10, ikkyraid=1, ikkyredo=1.
- Evidence checked: the files above, quest_globals for Truk. confidence high
- Recommended action: none
- Solo/policy: fixed for Righteous/Glorified/Transcendent. The 4th raid trial (Sentinel of the Altar) still requires a raid, see the extra finding.
- Suggested reply: none

### Kod Taz summoners ring event in dz appears bugged (1559986348662718596, created 2026-10-14, last activity 2026-10-14 20:30)
- Summary: in a raid DZ, Pixtt Grand Summoner healed to full every tick, so the golem waves never ended.
- Category: bug, already fixed
- Status: Straps fixed it in-thread ("OOC regen was faster") and closed the thread. Verified: `kodtaz/encounters/summoners.lua:121-124` `Grand_Spawn` calls `e.self:SetOOCRegen(0)`, with an Ascendant comment (mtime Oct 14 13:07).
- Evidence checked: summoners.lua lines 116-128. Grand Summoner rows checked: npc 293212, raid_target=1, so it depops in a normal-mode DZ by design, but the reporter was in a raid DZ. confidence high
- Recommended action: none
- Suggested reply: none

### Uqua - gas chamber traps requiring 6 players. (1560024256790659132, created 2026-10-14, last activity 2026-10-14 20:36)
- Summary: the gas chamber key slabs needed 6 players standing in the chamber.
- Category: bug / solo policy, already fixed
- Status: Straps said it was fixed and reloaded. Verified: `uqua/player.lua:3` `local player_minimum = 1; -- Ascendant: solo-friendly (live required 6 players...)`, used at lines 73 and 147 (mtime Oct 14 15:31).
- Evidence checked: uqua/player.lua lines 3, 60-80 and 147. confidence high
- Recommended action: none
- Solo/policy: resolved.
- Suggested reply: none

### Kod'Taz Progression Guide #5 Pit of the Lost (1559972170673692677, created 2026-10-14, last activity 2026-10-14 16:59)
- Summary: the Ageless Relic Protector died without splitting, so the Protector's Relic of Energy (60161) never dropped. The reporter was in a non-raid (normal) DZ.
- Category: bug, already fixed
- Status: Cause: the split forms 293222 and 293223 are raid_target=1, and global_npc.pl depops raid targets in normal-mode DZs. Fixed: `global/global_npc.pl:122-136` adds 293222 and 293223 to `%raid_depop_exceptions` with the comment "Ageless Relic Protector split forms ... Kod'Taz Pit of the Lost step". The split script `kodtaz/Ageless_Relic_Protector.pl` is correct. This step does not set a flag (Tublik 60161 hand-in gives exp only, Tublik_Narwethar.pl:248-251). Point has since moved on (ikky=10).
- Evidence checked: npc_types rows 293221-293225, global_npc.pl lines 114-136 (mtime Oct 18 23:36, so live since the restart), Tublik lines 248-251 and 263-268. confidence high
- Recommended action: none. Optional reply so Point knows a normal DZ works now.
- Suggested reply: "this should work in a normal dz now too, the split mobs were getting removed. just needed the restart"

### Ikkinz Group Trial Chamber of Twin Struggles - loot bug? (1559680740097003570, created 2026-10-13, last activity 2026-10-14 03:25)
- Summary: when one member kills "a pile of bones", the rest of the group gets no loot rights until the corpse unlocks.
- Category: bug, already fixed (C++)
- Status: Straps said it would be fixed with the next reboot. Verified in the deployed code: `code/zone/attack.cpp:2632-2641`. When `GetHateDamageTop` is null and the NPC has AggroImmunity, the kill is credited to the killer's ultimate owner, so the group/raid AllowPlayerLoot path (lines 2955-3010) runs. The piles 294139 and 294140 have special_abilities `14,1^24,1` (24 = aggro immune), which matches the cause.
- Evidence checked: npc_types 294139/294140, attack.cpp. The fix is in the committed HEAD that is deployed. confidence high
- Recommended action: none
- Suggested reply: none

### Lodestar (1559753978248560704, created 2026-10-14, last activity 2026-10-14 03:29)
- Summary: the Ascendant 2HB Lodestar had only one usable augment slot. The player asked for two.
- Category: suggestion, applied
- Status: Straps said it would be updated with the next reboot. Verified: items 752122 "Lodestar (Ascendant)" has augslot types 8 / 20 / 4. The base, Enhanced and Exalted versions keep 8 / 20. So the Ascendant version now has two non-ornament slots.
- Evidence checked: items table, rows matching '%Lodestar%'. confidence high (that the DB row has the slot). Not checked in client.
- Recommended action: none
- Suggested reply: none

### Taskmistress Krisz (1559704391940964434, created 2026-10-13, last activity 2026-10-14 04:30)
- Summary: the Riwwi Pixtts said "assemble at least three groups", so the Taskmistress would not spawn solo.
- Category: bug / solo policy, already fixed
- Status: Straps fixed it and the player thanked him. Verified: `riwwi/#Pixtt_Kekken.lua:1,9` and `riwwi/#Pixtt_Uxnikk.lua:1,9` set `TM_minimum_players = 1` with `math.max(GetRaidMemberCountInZone(),1)`, so a solo player now passes.
- Evidence checked: both files. confidence high
- Recommended action: none. Cosmetic only: the fail message on line 15 of both files still says "three groups", but nobody can hit it now.
- Suggested reply: none

### Progression stopped (1559800677876768869, created 2026-10-14, last activity 2026-10-14 05:43)
- Summary: Truk finished the summoning circle and handed in the relic/rune, got the blue text, but Tublik still talked about Crumbled Sanctuary.
- Category: working as intended
- Status: never answered in-thread. The circle/Protector relic hand-in (60161) gives exp only and sets no flag (Tublik_Narwethar.pl:248-251). Crumbled Sanctuary was his real next step at ikky=7 (hail text, line 9). He has since progressed to ikky=10, ikkyraid=1.
- Evidence checked: Tublik_Narwethar.pl lines 8-10 and 248-251, quest_globals for Truk. confidence high
- Recommended action: none (leave archived). No reply is needed now, he is well past it.
- Suggested reply: none

### Tacvi instance request requires 18 players in raid group (1559955872392679454, created 2026-10-14, last activity 2026-10-14 15:56)
- Summary: the Tacvi DZ request needed 18 players.
- Category: bug / solo policy, already fixed
- Status: Straps fixed and reloaded. Verified: `txevu/Hamari_Nedu.lua:3` and `txevu/player.lua:3` set `min_players=1, max_players=54` (mtime Oct 14 10:55).
- Evidence checked: grep for min_players across GoD zones. No GoD zone has min_players above 1. confidence high
- Recommended action: none
- Solo/policy: resolved.
- Suggested reply: none

### Barindu Sewer Quest: pool of Sludge. (1559795092783628328, created 2026-10-14, last activity 2026-10-14 05:27)
- Summary: map piece 2 was missing, and the player then found their rogue had pickpocketed it.
- Category: user error
- Status: the player resolved it themselves.
- Evidence checked: thread only. confidence high
- Recommended action: none
- Suggested reply: none

### Trial #1: Singular Might (1559731088991977502, created 2026-10-14, last activity 2026-10-14 03:28)
- Summary: the first Ikkinz group trial required a group of 2.
- Category: bug / solo policy, already fixed
- Status: Straps removed the check. Verified: `kodtaz/Gazak_Klelkek.lua:64` has the comment "solo-friendly, no group required" with no group check left, and `min_players=1`. `Maroley_Nazuey.lua` and `Kenra_Kalekkio.lua` (trials 2 and 3) also have min_players=1 and no group check. The stale header comment at Gazak lines 8-9 ("real minimum is 2") is harmless.
- Evidence checked: those three files. confidence high
- Recommended action: none
- Suggested reply: none

### Cragbeast Queen doesnt appear to spawn in DZ. (1559633788835991642, created 2026-10-13, last activity 2026-10-14 02:24)
- Summary: CBQ never spawned in the DZ, even through the 12-2am game-time window.
- Category: config (game clock backlog after idle), resolved
- Status: Straps said the zone game-clock was catching up, and another player (onlywiseman) confirmed she popped afterward. Instances force the spawn condition: `natimbi/player.pl:16-20` `EVENT_ENTERZONE` sets `spawn_condition(natimbi, instanceid, 1, 1)` (mtime Oct 13 09:55, before the report). The spawn_events SpawnCBQ (1am) / DespawnCBQ (3am) exist, and the condition onchange=3 means repop-if-ready, never depop. So in a DZ she is effectively not clock-locked once someone zones in.
- Evidence checked: player.pl, #Cragbeast_Queen.lua, spawn_events 64/65, spawn_conditions natimbi 1. confidence med (did not test in game). Minor edge case: `#Cragbeast_Queen.lua` event_combat turns the condition off after 2am when she leaves combat. That has no visible effect while she is up, but after a wipe and depop she would need a re-zone to come back.
- Recommended action: none
- Suggested reply: none

---

## Extra finding (outside these 13, but related to the solo policy)
- **`kodtaz/Sentinel_of_the_Altar.pl:5-27`** (Ikkinz raid trial #4, Chambers of Destruction, ikky=14): still requires being in a raid (`if($raid){ ... } else { "You are not in a raid!" }`), and the instance goes through `AssignRaidToInstance` only. A solo player without a raid partner cannot start it. This is the same pattern Tublik had before the fix (Tublik_Narwethar.pl:87). The file has not been touched (mtime May 4). It probably relates to the active thread "Zone out of chambers of destruction" (1560400111987855421).
  - Smallest fix: copy Tublik's pattern. Remove the `if($raid)` / else branch (keep the `<= 54` check only when a raid exists). Then assign with raid, else group, else `quest::AssignToInstance($Instance)`, the same logic as Tublik's `ASSIGN_IKKY_INSTANCE`. Rollback: restore the file from a backup copy made first. It is a quest file, so it takes effect when kodtaz reloads or at the next restart (no reload per policy).
- Cosmetic: the "three groups" fail text in riwwi/#Pixtt_Kekken.lua:15 and #Pixtt_Uxnikk.lua:15.
- Non-GoD content still has min_players above 1, but those zones are not live yet: oasis (6), draniksscar / natimbi #Kemik_Arulon / dranik (3), provinggrounds / nedaria (2). They need the solo pass before OoW/DoD. natimbi/#Kemik_Arulon.lua:3 (Murkglider Hive, min 3) sits in a live GoD zone, but it is DoN/DoD content. Check whether it can be reached now.

## Top issues in this channel (player impact first)
1. Sentinel of the Altar (Ikkinz raid trial 4) still needs a raid, so it blocks solo players at ikky=14. Not fixed. Likely ties into the active "Zone out of chambers of destruction" thread.
2. The 6-player (Uqua), 18-player (Tacvi), 3-group (Taskmistress) and group-of-2 (Singular Might) gates are all removed and verified in the files.
3. Pit of the Lost split and pile-of-bones loot lock are both fixed (global_npc exception, C++ aggro-immune killer credit) and live since the 10-18 restart.
4. Ikkinz raid instance re-entry is fixed (bigger triggers, re-mark compass, join a party member's instance).
5. The rest is resolved or user error: summoners regen, Lodestar aug slot, CBQ clock, Barindu pickpocket, Tublik dialogue, empty "Sone worker" thread.
