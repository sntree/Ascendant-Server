# Triage: #bug-reports + #aa-bug-reports (items since 2026-09-21)

Scope check: discord_list_threads for both channels showed no other threads created or active since 09-21 beyond the ones listed below. "Current Known Bugs" (1498419918255685753) has **no messages since 09-21**, and "Coldain Ring Quest - Ring 9" (1550296127314722938) has **no replies since 09-21** (thread archived, nothing pending).

All times UTC unless marked "server" (server time = UTC-5).

---

## #bug-reports

### Unfired Medium Bowl not stackable (1562316274015408198, created 2026-10-21, last 2026-10-21 04:31)
- Summary: Player says Unfired Medium Bowl (16929) should stack to 100, citing Allakhazam/EQTraders. Ali (Senior Guide) replied that the stack-size increase was a Feb '08 (SoF-era) change and pointed them to suggestions.
- Category: working as intended (era data) / suggestion
- Status: answered by a guide in-thread. Not a regression.
- Evidence checked: `items` 16929 Unfired Medium Bowl stacksize=1, along with every other Unfired bowl (16925, 16930, 16933, 3653) and ~100 other Unfired pottery items (containers, pie tin, muffin tin, idols, etc.) all stacksize=1. 224 other Unfired items do stack. This is consistent stock PEQ data, not a one-off bad row. Confidence: high.
- Recommended action: none (or close). If the owner wants QoL stacking, it would be a batch change to all 1-stack unfired pottery, not just this one item, applied at a normal restart. That's an owner call.
- Solo/policy flags: none
- Suggested reply: none (Ali already covered it)

### Crash on load (1561843064698441769, created 2026-10-19, last 2026-10-21 04:23)
- Summary: ToB client user went linkdead buying bags with items on cursor, and then the char could not log in. First Mortician (10-19), then Menanu (10-21). Both were fixed by hand by Straps.
- Category: bug (known ToB issue)
- Status: **Yes, this is the known ToB cursor-bag issue.** Straps' 10-21 fix note says it exactly: "starter Backpack was stuck on the cursor with its contents saved one slot off". Both chars were fixed by hand. The root cause is still open in code ("I'll have to deal with that sooner vs. later").
- Evidence checked:
  - Fix verified: Menanu (130278) now has Backpack* in bank slot 2000 with 8 items at 6210-6217 (the bank-bag range for slot 2000). Nothing is on the cursor (slot 33) or in the cursor-bag range (6010-6209). world_145745.log shows Menanu logged in and zoning guildlobby -> poknowledge at 10-21 00:53-00:54 server (05:53 UTC), after the 04:23 UTC fix. Mortician (130277) has nothing on cursor. Confidence: high.
  - No crash dumps: /opt/akk-stack/server/logs/crashes is empty and get_crash_logs returns none. This is a client-side crash on load, not a server crash.
  - At-risk scan: inventory slot 33 holding a bag, chars logged in since 10-01. One hit: **Nivmizzit (130276, lvl 25 rogue, last login 10-20 15:25 server)** has an empty Backpack (17005) on cursor, with no contents in the cursor-bag range and nothing orphaned in the slot-25 bag range. An empty bag is lower risk, but it's the same pattern (char ids are consecutive with Mortician/Menanu, and the account is likely the same player's). Watch for a third report. Older cursor-range rows (Mark of Ascendance at 6011+ for ~11 chars from March-August) are the old cursor queue, not this bug.
- Recommended action: needs a code change in the C++ server (ToB agent): when a ToB client buys or moves a bag while the cursor holds a bag or item, the cursor-bag contents are saved at sub-slot +1 (off by one in the ToB->server bag sub-index translation for the cursor bag, likely in the ToB patch slot conversion for CURSOR_BAG_BEGIN=6010). That leaves the client with an invalid inventory on next load. A defensive login-side fix also helps: on load, if a cursor bag's children don't line up, move the bag to the first free bank or general slot. Until the code fix lands, keep doing the hand fixes.
- Solo/policy flags: none
- Suggested reply: none (Straps already replied "Try logging in", and the player is back in). If they post again: "glad you're back in. the bag/cursor thing on the TOB client is on my list, try not to buy bags with stuff on your cursor until it's patched"

### Ecliptic Reinforcement 4 (1561249387966111744, created 2026-10-18, last 2026-10-18 17:22)
- Summary: Mobs (and occasionally players) got the "Ecliptic Reinforcement 4" buff from the 65535 no-spell sentinel, which made casters nuke for 4k+ (a cleric reported a 7k Reckoning). Straps explained it, scripted a stopgap, and said the real fix needed a reboot.
- Category: bug, already fixed (data side); code hardening pending
- Status: **No reports after the 10-18 23:30 server restart.** The last message in the thread is 10-18 17:22 UTC (12:22 server), about 11 hours before the restart (10-19 04:30 UTC). Nothing has come in since.
- Evidence checked: `spells_new` id 65535 is gone (verified). npc_spells attack/range/defensive_proc and npc_spells_entries have no references to 65534/65535. zone_state_spawns has no buffs JSON containing 65535, so no saved-state buffs will reapply it. No "Ecliptic Reinforcement" in the current zone logs (short retention). Side note: **65534 now exists (Ecliptic Reinforcement 3)** and max spell id is 74875. If any code path treats 0xFFFE/65534 as a sentinel, or truncates ids above 65535 to uint16, the same class of bug can reappear. Mention this to the ToB agent doing the int32 fix. Quest plugins already special-case `!= 65535` (ascendant_pet_bag_system.pl:175/608/719), which is fine. Confidence: high.
- Recommended action: none in Discord (or close). Make sure the pending int32 spell-id code fix also audits uint16 spell-id fields and any 0xFFFF/0xFFFE sentinel compares now that spell ids go past 65535.
- Solo/policy flags: none
- Suggested reply: "this one's fixed since the last reboot, let me know if you see that buff show up again"

### PoFire fast despawn (1554269388599861298, created 2026-09-28, last 2026-09-29 23:55)
- Summary: (1) The Council of Fire / Fennin spawn stage depopped within about 4 minutes while the player was alt-tabbed. (2) In a raid DZ, they died fighting the 4 hasted named before Fennin's castle, those named depopped, and the event became unfinishable. No staff reply.
- Category: bug (unconfirmed cause) / possibly working as intended for (1)
- Status: no staff answer. Reporter has not followed up. Logs from 9-28 are long gone (short retention).
- Evidence checked:
  - `pofire/#Fennin_Ro_the_Tyrant_of_Fire.pl` (trigger 217076): phase NPCs only depop when the `event_timeout` timer fires (7200s) and signals them. The phase named scripts (Azobian/Javonn/Reaxnous/Hebabbilys/Council: `pofire/*_the_*.pl`, Chancellor_*.pl, Warlord_Prolazz.pl, Omni_Magus_Crato.pl) are `sub EVENT_SIGNAL { quest::depop(); }`, so they depop on **any** signal from anything. That is fragile.
  - `pofire/Fennin_Ro_the_Tyrant_of_Fire.pl`: a 3600s despawn timer, already hardened 06-20 against the enc_rescan timer. Not a 4-minute despawn.
  - `global/global_npc.pl:121-136`: in a **normal-mode** DZ, any `raid_target` NPC is depopped on spawn. Fennin (217054) is raid_target=1, so in a normal DZ the whole event runs and then Fennin vanishes the instant he spawns. If report (1) was in a normal DZ, that explains it, and the experience is very confusing for the player. The phase named (217077-217084) are not raid targets.
  - Ruled out: zone shutdown (pofire shutdowndelay=3600000, DZ uses version 0), the encounter-scaling plugins (no depop), and spawn_conditions (onchange=2 repop, no signal action). No pofire spawn_events.
  - Fennin is being killed normally by many players (data_buckets pop_fenninro_* ~20+ entries), so the event isn't broadly broken.
  - Confidence: low on root cause for (2). Medium that (1) is the normal-DZ raid-target depop.
- Recommended action: ask the reporter for character name, normal vs raid DZ, and approximate date/time. Code-side options, all owner decisions and none urgent:
  (a) in normal DZ mode, depop the Fennin **trigger** (217076) or show a message, rather than letting the event play out and silently deleting Fennin (global_npc.pl:121-136 runs on Fennin's spawn; adding 217076 to the normal-DZ depop, or messaging there, is a small edit);
  (b) make the phase named only depop on the specific timeout signal (e.g. `return unless $signal == 0;` or use a dedicated id like 217999) so a stray signal can't wipe progress;
  (c) for (2), check whether death or empty instance resets anything once a fresh repro has logs.
- Solo/policy flags: **Solo-friendly concern.** A solo player who dies mid-event in a raid DZ loses the whole event with no respawns (raid DZs block respawns, per global_npc.pl `_get_dz_mode` / EVENT_DEATH_COMPLETE). If the depop on death is real, it's a progression blocker for solo players.
- Suggested reply: "sorry this one slipped. was this in a normal or raid DZ, and what's the char name? normal DZs strip the raid bosses so Fennin won't stay up there. if it was raid i'd like to dig into it, a rough date/time helps"

### lost exp/level (1556142069922140281, created 2026-10-04, last 2026-10-10 00:01)
- Summary: Necro Necrophilly kept "losing levels". Straps checked logs: the only loss was 10/3 21:34, the same second as creating an Ascendant Scroll: Dead Man Floating (the AA scroll creation costs XP). Player accepted and asked to close. Ali closed it.
- Category: user error / working as intended
- Status: answered by staff and closed (archived). Nothing after 10-10.
- Evidence checked: thread only (staff already pulled the logs and posted the explanation). Confidence: high.
- Recommended action: none
- Solo/policy flags: none
- Suggested reply: none

### Current Known Bugs (1498419918255685753, staff list, created 2026-04-27)
- Summary: no messages since 2026-09-21.
- Category: n/a
- Status: stale. The list does not mention the current live issues (ToB cursor-bag login crash, swarm pets rooted).
- Recommended action: optional. Straps could add the ToB bag/cursor note there.
- Suggested reply: none

### Coldain Ring Quest - Ring 9 (Collect Berradin's Head) (1550296127314722938, created 2026-09-18, archived)
- Summary: no replies since 2026-09-21. The thread is archived.
- Category: n/a (nothing new)
- Recommended action: none
- Suggested reply: none

---

## #aa-bug-reports

### State Of Swarm Pets (1561732248506601563, created 2026-10-19, no replies)
- Summary: Player lists AA swarm pets as "permarooted" vs "mobile": Doppelganger, Rise of Bones, Attack Warders, Pack Hunt, Phantasmal Opponent, Swarm of Decay rooted, and Call of Xuzl rooted from mid ranks onward. Chattering Bones, Nature's Guardian, Song of Stone, Host of the Elements, Spirit Call, Companion of Necessity, and Celestial Hammer are mobile.
- Category: bug (data)
- Status: no staff reply. **Confirmed, with an exact root cause.**
- Evidence checked: code `zone/mob.cpp:441` `permarooted = (runspeed > 0) ? false : true;`. The pet npc_types behind AA spells (spells_new.teleport_zone -> npc_types.name) split exactly along the player's list:
  - runspeed 1.25 (mobile): SumSword 641 / SumFireSword 685 (Call of Xuzl ranks 1-5), SKSkeletonSwarm 649, SumBearSpirit 857, bardSwarm 650, mageSwarm*, SpiritWolf/SumDireWolf*, SumCelestialSpirit*, CompanionOfNecessity* (2.0), SwarmPetDG1/2, SwarmPetSwarmofDecay2-4 (831-833).
  - **runspeed 0 (rooted)**: SumFireSword2-6 (1376-1380, Call of Xuzl higher ranks; the player's "from around level 3 onward"), SwarmPetDG3-13 (684, 954-963), packhunt1-7 (1358-1364), bstSwarm1-7 (1337-1343, Attack of the Warders), riseofbones1-10 (1348-1357), PhantasmalOpponent 1418, SwarmPetSoF1 675 + SwarmPetSwarmofDecay5-13 (1381-1389), and also some the player didn't list: **Army of the Dead animateDead2-18 (1318-1334)**, Song of Stone upgrades (1335-1336), Chattering Bones 2-4 (1365-1367), Nature's Guardian 7-14 (1368-1375), Spirits of Nature (1344-1347), Projection of Piety/Doom/Fury (1408-1413), Lyrical Prankster (1414-1417), Illusory Ally 1419.
  - Swarm of Decay is mixed across ranks (831-833 mobile, 675/1381+ rooted), so the rooting is not intentional design. Confidence: high.
- Recommended action: fix (DB, apply at the next normal restart; npc_types load at zone boot, so never hotfix). Excludes stationary-by-design pets: WARD* wards, ServantRo*, Summon* utility (banker/resupply/eye), SumBaneSpirit.
  ```sql
  -- fix
  UPDATE npc_types SET runspeed = 1.25 WHERE runspeed = 0 AND id IN
  (675,684,954,955,956,957,958,959,960,961,962,963,
   1318,1319,1320,1321,1322,1323,1324,1325,1326,1327,1328,1329,1330,1331,1332,1333,1334,
   1335,1336,1337,1338,1339,1340,1341,1342,1343,1344,1345,1346,1347,
   1348,1349,1350,1351,1352,1353,1354,1355,1356,1357,1358,1359,1360,1361,1362,1363,1364,
   1365,1366,1367,1368,1369,1370,1371,1372,1373,1374,1375,1376,1377,1378,1379,1380,
   1381,1382,1383,1384,1385,1386,1387,1388,1389,
   1408,1409,1410,1411,1412,1413,1414,1415,1416,1417,1418,1419);
  -- rollback
  UPDATE npc_types SET runspeed = 0 WHERE id IN (<same list>);
  ```
  The owner may decide to keep Swarm of Decay (675, 1381-1389) stationary if they're meant to be archers. Live's are mobile, and ranks 2-4 here already move.
- Solo/policy flags: **Solo-friendly impact.** Swarm pets are a big part of solo DPS for pet classes. A rooted pet only hits if the mob stands in its spot, which hurts solo players most (no tank to hold mobs in place).
- Suggested reply: "nice list, thanks. found it, a bunch of the higher rank swarm pets got set up as stationary by mistake. fix goes in with the next update"

---

## Top issues in this channel (player impact first)
1. **ToB cursor-bag login crash** (Crash on load). Characters are locked out until a GM hand-fixes them. 2 chars so far, and 1 more char (Nivmizzit) has a bag on cursor now. Needs a C++ fix in the ToB slot translation.
2. **AA swarm pets permarooted** (State of Swarm Pets). 96 pet npc_types have runspeed=0, across about 15 AA lines incl. Army of the Dead. One-line DB fix at the next restart. Hurts solo pet classes.
3. **PoFire Fennin event depop** (PoFire fast despawn). Unanswered for 3 weeks. Likely the normal-DZ raid-target strip for (1). (2) is unconfirmed and could block solo progression. Needs reporter details and a repro.
4. **Ecliptic Reinforcement 65535 sentinel**: fixed in data, no reports since the 10-18 restart. Make sure the int32 code fix also covers 65534 now being a real spell.
5. Unfired bowl stack size: era-correct, no action. Lost exp: user error, closed.
