# Discord feedback triage and fix plan (2026-10-21)

**Scope:** all 5 forum channels, every thread created or active since about 2026-09-21. That is 60 threads, with all replies read:
- #tob-client-feedback: 8
- #god-bug-reports: 24
- #bug-reports: 7
- #aa-bug-reports: 1
- #suggestions-and-feedback: 10 (older threads with new replies included)

**What was done:** read-only. Nothing was posted to Discord, written to the DB, edited or reloaded. Every SQL below is a proposal with a rollback.

**Deployed code:** `ascendant/2026-10-19` (HEAD 5452bb91f, live since the 10-18 23:30 restart).

**Upstream code work:** C++ items for the ToB/upstream agent are in [tob_agent_brief_2026-10-21.md](tob_agent_brief_2026-10-21.md). It is a ready-to-paste prompt.

**Detail:** per-thread notes (evidence, every suggested reply) are in `feedback_triage_2026-10-21_notes/`, one file per channel.

---

## Morning checklist (do in this order)

1. Approve or adjust the **Tier A** DB and quest fixes (A1-A5). They all ride the next normal restart; nothing gets hotfixed.
2. Paste the ToB agent brief into the ToB/upstream session (Tier B). B1 and B2 are one-liners and could ship in the same build as A.
3. Make the owner calls in **Tier C**. Each one is a yes/no or pick-a-number.
4. Approve the Discord replies and closes in **Tier D**. I'll post them only after your OK.

---

## Tier A: true bugs, server data or quest fixes (no C++), ranked by player impact

### A1. Own-class House of Thule AAs locked at level 70: affects every level-65 player
- **Threads:** GoD "Vengeful Spirits AA" and "Missing warrior AA". Players see that other classes can buy their class's new AAs, but they can't.
- **Cause:** `2026_09_26_god_gate_release.sql` lowered only the 52 *universal/cross-class* ranks (33xxx) to level 65. The 52 matching *native* ranks (13xxx, expansion 17) are still level 70. Examples:
  - 13017 is 70 while 33017 is 65.
  - 13169 is 70 while 33169 is 65.
  
  The insight trainer uses the native chain for your own class (`plugins/ascendant_insight_trainer.pl:336-351, 452-454, 804`), and the AA window shows the native level too.
- **Not** the "missing next rank" trainer fix. That code (`_count_ranks`/`_get_nth_rank_id`, lines 1031-1071) has a file mtime of 10-13 and has been live since the 10-18 restart. If you meant a newer fix, I didn't find one on disk.
- **Fix** (AA data loads at zone boot, so it goes live at the restart):
  ```sql
  CREATE TABLE asc_patch_god_hot_native_aa_backup AS
    SELECT n.id, n.level_req FROM aa_ranks n JOIN aa_ranks u ON u.id = n.id + 20000
    WHERE u.id BETWEEN 20000 AND 39999 AND u.expansion = 17 AND u.level_req = 65
      AND n.expansion = 17 AND n.level_req = 70;                       -- expect 52
  UPDATE aa_ranks n JOIN asc_patch_god_hot_native_aa_backup b ON b.id = n.id SET n.level_req = 65;  -- expect 52
  -- rollback
  UPDATE aa_ranks n JOIN asc_patch_god_hot_native_aa_backup b ON b.id = n.id SET n.level_req = b.level_req;
  DROP TABLE asc_patch_god_hot_native_aa_backup;
  ```
  Vengeful Spirits rank 4 (13396, level 71, expansion 18) stays hidden, which is intended. Drop the backup table once it's confirmed in game (cleanup-test-remnants rule).

### A2. AA swarm pets permanently rooted: hurts solo pet classes
- **Thread:** #aa-bug-reports "State Of Swarm Pets". The player's list matches the data exactly.
- **Cause:** `zone/mob.cpp:441` `permarooted = (runspeed > 0) ? false : true;`. 96 AA swarm-pet npc_types have `runspeed = 0`. They include:
  - Call of Xuzl ranks 6+
  - Doppelganger 3-13
  - Pack Hunt, Attack of the Warders, Rise of Bones
  - Swarm of Decay 5-13 (ranks 2-4 already move)
  - Army of the Dead 2-18
  - Phantasmal Opponent
  - Nature's Guardian 7-14
  - Chattering Bones 2-4
  - Spirits of Nature, Projections, Lyrical Prankster, Illusory Ally
  
  Wards, utility summons and Bane Spirit are excluded on purpose.
- **Fix:**
  ```sql
  UPDATE npc_types SET runspeed = 1.25 WHERE runspeed = 0 AND id IN
  (675,684,954,955,956,957,958,959,960,961,962,963,
   1318,1319,1320,1321,1322,1323,1324,1325,1326,1327,1328,1329,1330,1331,1332,1333,1334,
   1335,1336,1337,1338,1339,1340,1341,1342,1343,1344,1345,1346,1347,
   1348,1349,1350,1351,1352,1353,1354,1355,1356,1357,1358,1359,1360,1361,1362,1363,1364,
   1365,1366,1367,1368,1369,1370,1371,1372,1373,1374,1375,1376,1377,1378,1379,1380,
   1381,1382,1383,1384,1385,1386,1387,1388,1389,
   1408,1409,1410,1411,1412,1413,1414,1415,1416,1417,1418,1419);
  -- rollback: same list, SET runspeed = 0
  ```

### A3. Ikkinz raid trial 4 (Sentinel of the Altar) still requires a raid: solo blocker at ikky=14
- **Found** while verifying the archived GoD threads. It probably sits behind the "Zone out of chambers of destruction" chatter.
- **Cause:** `server/quests/kodtaz/Sentinel_of_the_Altar.pl:6-26`. It checks `if($raid)` and otherwise says "You are not in a raid!", then calls `AssignRaidToInstance` only.
- **Fix:** copy Tublik's pattern (`kodtaz/Tublik_Narwethar.pl:357-367` `ASSIGN_IKKY_INSTANCE`):
  - Drop the `if($raid)` wrapper and the else branch.
  - Keep the 54-member check only when a raid exists.
  - Replace `quest::AssignRaidToInstance($Instance)` with raid → `AssignRaidToInstance`, group → `AssignGroupToInstance`, else → `AssignToInstance`.
  
  Back up the file first (`backup/…`). It takes effect on the next quest load.

### A4. Pxet Elites can never spawn in a DZ (Smith Rondo / Ferubi, BiC #4)
- **Cause:** `server/quests/ferubi/Event_Spawner.lua:27/36/45/54`. Each elite needs the **100th** kill from its camp. A full clear has only 66 / 46 / 43 / 24 spawn points. DZ counters in data_buckets are stuck at exactly those numbers. The instance keys have no expiry, so they leak into reused instance ids.
- **Fix:** `local need = (eq.get_zone_instance_id() == 0) and 100 or 20`, then use `>= need` instead of `== 100`. Set the data with an expiry (`"6h"`) when inside an instance. 100 kills x4 camps is also a heavy solo grind in OW, so consider lowering the OW threshold too (your call).

### A5. Coral Hilted Shortsword bard swarm pet mezzes and fears its own target
- **Thread:** suggestions "Coral Hilted Shortsword". It's really a bug.
- **Cause:** npc 654 `SwarmPetBard` uses the shared "Default Bard List" (npc_spells 11). Its mez/fear/memblur songs are typed as nukes. Nine NPCs use list 11 (verified), so don't edit the list.
- **Fix (simplest):** `UPDATE npc_types SET npc_spells_id=0 WHERE id=654;` Rollback: `SET npc_spells_id=11`.
- **Better (~30 min):** a new list with only DoTs and buffs.

---

## Tier B: true bugs needing C++ (send to the ToB/upstream agent, then build + normal restart)

Full prompt with file:line: [tob_agent_brief_2026-10-21.md](tob_agent_brief_2026-10-21.md). Ranked:

| # | Issue | Thread(s) | Size |
|---|---|---|---|
| B1 | **TGB is off for every ToB player.** Group buffs only hit your own group. ToB never sends OP_TGB, so `tgb` stays false. | TGB Broken? | 1 line |
| B2 | **In-bag auto-combine broken on ToB** (mixing bowl etc.). The decode drops the container id. Depot is a red herring. | Tradeskill from Depot | 1 line + 1 log check |
| B3 | **ToB item-loss paths.** OP_MoveMultipleItems has no decode (bebags / bulk moves destroy items). Shared bank phantom slots don't resync. Server advertises unlimited shared bank slots. | bebags PSA, Extra shared bank space | medium |
| B4 | **ToB cursor-bag login crash.** Buying bags with a bag on the cursor saves contents one slot off, and the char can't log in. Hand-fixed for Mortician and Menanu. **Nivmizzit (130276)** has an empty Backpack on cursor right now (low risk, same pattern). | Crash on load | medium |
| B5 | **Vendor buy, all clients.** Money is taken *before* the room check and not refunded when bags are full. Several failure paths send no reply, which leaves the ToB quantity window stuck. | Minor Adv Loot Bug (2nd half) | small |
| B6 | **Advanced Loot "give" to an out-of-zone member** leaves the row stuck until it rolls. | Minor Adv Loot Bug | small |
| B7 | **Spell-id hardening.** The pending int32 fix should also audit uint16 spell-id fields and 0xFFFE compares. 65534 is now a real spell (Ecliptic Reinforcement 3). | Ecliptic Reinforcement 4 (fixed by data) | audit |

B1 and B2 are tiny and safe. If the ToB agent turns them around quickly, they can ship with Tier A.

---

## Tier C: owner decisions (design or policy)

| # | Decision | Recommendation |
|---|---|---|
| C1 | **Bag tier upgrades shrink bags over 10 slots.** Tier slots are min(base,10)+2/4/6, so the 14-slot handbag drops to 12 at Enhanced, and 400 base bags are affected. | Use base+2/+4/+6 (SQL with backup in `feedback_triage_2026-10-21_notes/god_active.md`). Check that RoF2 displays 30+ slot bags before going above 40. |
| C2 | **Heals under the 3010 ms cast cap.** Karana's Renewal and Complete Heal beat the GoD heals (Sylvan Infusion, Holy Light, Pious Light) by about 2x at the same mana and time. | Decide system-wide. Options: buff the new heals in spells_new, or give long percent heals their own cap in `spells.cpp`. Don't do it one spell at a time. |
| C3 | **GoD quest reward variants roll weak mod2s.** The Ascendant class helm 768855 has heroic 1 and no heal/spell dmg, while raid drops have 5/5/10-14. | Worth doing for solo players (quest gear is the reachable tier). About half a day of DB work. |
| C4 | **Ikkinz raid #3: a 33h lockout is set on the Guardian kill**, mid-event, so a failed solo attempt is locked out. | Move the lockout to the Nixki kill only (`ikkinz/lockout_ikkinz.lua:41-48`). |
| C5 | **Taelosian Morphemic Staff (66617, +700 Yunjo/Nihil when equipped) on the Marks vendor** (Exarch_Valeth 344046). The faction checks in Riwwi count it. | Yes, it's a quick win. Pick the cost (5 marks suggested). DESCRIBE merchantlist before inserting. |
| C6 | **Liminal in PoTranq** (and maybe the Dulak docks). | Yes. It needs one in-game /loc, then 3 rows (spawngroup/entry/spawn2). |
| C7 | **Early spell scribing.** The client blocks it, but the server has no level check, so an NPC hand-in could scribe early. | Optional QoL. About 1-2h of quest work plus a RoF2+ToB display test. |
| C8 | **PoFire Fennin event.** A normal DZ deletes Fennin the moment he spawns (raid_target depop), and the phase named depop on *any* signal. | Ask the reporter for details first. A cheap hardening: depop the trigger in normal DZs with a message. |
| C9 | **Other min-player gates still live** outside GoD: Natimbi `#Kemik_Arulon.lua` (min 3), plus oasis, draniksscar, dranik, provinggrounds, nedaria expeditions (2-6). | Set them to 1 per solo-friendly policy. Check whether each is reachable in the current era first. |
| C10 | **Unfired pottery stack size** (all 1-stack unfired items). Era-correct as is. | No change unless you want QoL. |
| C11 | **Tome icons / MOA bank boxes / Progression Ideas.** Already answered by Ali; boxes are waiting on B3. | No action. |

---

## Tier D: wrong, bad, or already-handled reports (reply/close only)

| Thread | Verdict |
|---|---|
| ToB: Buy All AA | Server loop is fine (buys until the first rank you can't buy). Probably a level, points or prereq limit. **Needs player info.** |
| ToB: Tradeskill overflow message | Working as intended. The ToB client calls the cursor queue "overflow"; nothing is lost. |
| ToB: E3Next | Community sorted it (Rekka's ToB build). Nothing to do server-side. |
| ToB: Extra shared bank space | Fixed by Straps 10-17. Remaining gaps are in B3. |
| ToB: bebags PSA | Items returned by Straps. Root cause in B3. |
| GoD: pile of bones Ikky 3 | User error: the bones are 4 small NPCs you kill. Samp/Truk also never turned the Glorification artifact in to Tublik. |
| GoD: Sanctuary of the Transcendent | Lockout, now expired (see C4). |
| GoD: BiC #4 gemstone | Fixed (missing `class_type` plugin added 10-19). Player finished the chain. |
| GoD: Rikabi | Working as intended. |
| GoD: Gelid Rending rank 2 | Rank 2 doesn't exist in the data. Trainer fixed 10-13. |
| GoD: Qinimi BiC #1, Chambers zone loop | Fixed by Straps, verified in files. |
| GoD archived (13) | All fixed and verified, or user error/WAI. No replies needed. |
| Bugs: Ecliptic Reinforcement 4 | Fixed by the 10-18 restart. No reports since. |
| Bugs: Unfired Medium Bowl | Era data. Ali answered. |
| Bugs: lost exp/level | User error. Closed by staff. |
| Suggestions: Krasnok's Reserve | Done (90%), verified. Close. |

---

## Suggested Discord replies (casual, short; post only after your OK)

| Thread | Reply |
|---|---|
| TGB Broken? | "Yep, confirmed. The new client doesn't tell the server TGB is on, so group buffs only hit your own group right now. Fix is lined up for the next update." |
| Tradeskill from Depot | "Thanks, reproduced it. It's bag containers, not the depot. Fix is in the works. Ovens and forges work fine meanwhile." |
| Minor Adv Loot Bug | "Both of these are on the list. Should be sorted next update." |
| Buy All AA | "Which AA was it, and on which character? Buy All stops at the first rank you can't buy yet (level or points), so that might be it. If not, we'll dig in." |
| Tradeskill combine messages | "That's just the new client's way of saying it went to your cursor queue. Nothing's lost." |
| bebags PSA | "Thanks for the heads up. Until the next update, skip bebags or any bulk-move script on the new client." |
| Vengeful Spirits AA | "Yep, found it. Your own class's new AAs got left at a level 70 requirement. Fix goes in with the next update." |
| Missing warrior AA | "Found it. Your own class's new AAs got left at a level 70 requirement. Fix goes in with the next update." |
| State Of Swarm Pets | "Nice list, thanks. A bunch of the higher rank swarm pets got set up as stationary by mistake. Fix goes in with the next update." |
| Coral Hilted Shortsword | "Yeah, that pet shouldn't be mezzing what you're fighting. Fix goes in with the next update." |
| Smith Rondo event | "Yeah, a DZ clear doesn't get enough kills for them to pop right now. Do that part in the open world for now. Looking at making it work in DZs too." |
| Braided Horsehair Handbag | "Good catch. The upgrades are capping big bags lower than they should. Looking at a fix." |
| pile of bones Ikky 3 | "When Nixki dies, 4 piles of bones pop up around the room. Kill one to loot the artifact. Your lockout's clear so you can run it again. Also hand Tublik the Glorification artifact first or the last one won't count." |
| Sanctuary of the Transcendent | "That was the lockout from your earlier run. It's cleared now. Turn the Glorification artifact in to Tublik first if you haven't." |
| BIC #4 gemstone | "That one's fixed, and looks like you got through the rest of the chain. Closing this out, thanks." (+ close) |
| Rikabi | "Yep, that's normal. He hands it back and the door opens. Closing." (+ close) |
| Gelid Rending Rank 2 | "Gelid Rending only has the one rank on our server right now, so rank 1 is max. Closing." (+ close) |
| Qinimi BiC #1 | "Glad it's working now. Closing this one." (+ close) |
| Zone out of chambers | "This should be all sorted now. Closing, thanks for the reports." (+ close) |
| PoFire fast despawn | "Sorry this one slipped. Was this a normal or raid DZ, and what char? Normal DZs strip raid bosses so Fennin won't stay up there. If it was raid I'd like to dig in. A rough date/time helps." |
| Ecliptic Reinforcement 4 | "This one's fixed since the last reboot. Let me know if that buff shows up again." |
| Krasnok's Private Reserve | (close only) |
| Taelosian Morphemic Staff | (after C5 is approved) "Going to toss it on the marks vendor with an upcoming update." |
| Liminal in PoTranq | (after C6) "Good call, will get one added in Tranq with an upcoming update." |
| Sylvan Infusion | "Yeah, the cast time cap makes the big heals way better than the GoD ones. Looking at it across all the healers, not just druid." |
| Quest Items Buffed | "Fair point, the quest stuff comes out weaker on the extra stats. Will look at bumping the GoD quest rewards." |
| Allow early learning spells | "The client blocks scribing above your level on its own, but we might be able to do it through an NPC. Will take a look." |

---

## Restart bundle (once A and C are approved)

- **DB:** A1, A2, A5 and any approved C1/C5/C6, each with its backup table or rollback.
- **Quests:** A3, A4, and C4/C9 if approved. Back up into `backup/` first.
- **C++:** whatever the ToB agent returns (B1 and B2 at minimum). After the build, copy `code/utils/patches/patch_TOB.conf` to `server/assets/patches/` (AFTER-BUILD.md).
- **After the restart:** in game, check own-class HoT AA visibility on a level-65 char, a swarm pet following, TGB on a ToB client, and a mixing-bowl auto-combine. Then post the replies, then drop the backup tables after a few days.

---

## Status log (2026-10-21, later)

- **A1:** applied on prod (`server/sql/2026_10_21_god_hot_native_aa_level65.sql`; backup table `asc_patch_god_hot_native_aa_20261021`). Live at the next reboot. Threads closed.
- **A2-A5:** done by the test agent (`test_agent_A1_A2_2026-10-21.md`, `test_agent_A3_A5_2026-10-21.md`). Threads closed (swarm pets, Smith Rondo, Coral Hilted Shortsword).
  - Pxet open-world threshold is now 30 (owner).
  - **Watch:** Fellowships already took custom migration 14 on origin, so these migrations must be 15+ before the release build.
- **Inquisition (126445):** login crash from bags saved inside bags after ToB extra shared bank moves. Hand-fixed on prod (`2026_10_21_fix_inquisition_nested_bags.sql`, rollback alongside). Thread closed. Tomes inside the nested bags may be lost (no snapshots).
- **ToB bag/shared bank fixes (B3; B4 if included):** patching in tonight's update per owner. Closed "bebags PSA" and "Crash on load" with "patched with tonight's update" notes. If the patch didn't include the cursor-bag save fix (B4), expect "Crash on load" to come back.
- **A6, Riwwi Polished Silvery Key:** the cooldown bucket is global across all Riwwi zones. Prompt: `test_agent_A6_riwwi_key_2026-10-21.md`. Thread kept open until it's in test.
- **Exarch Valeth rez tokens:** working as intended (10-13 gambling change). Staff answered; closed.
- **Discord rule (owner):** reply only when closing. Already solved = thanks + close. Player data = fix locally + close. Code = test-agent prompt, no reply until in test.
- **Batch 2 + A6** (test agent; queued for tonight's release on branch `ascendant-batch2`, migrations 21/22, custom version 22):
  - B1 TGB, B2 bag combine (Mixing Bowl is 17906, not 17901)
  - C5 Morphemic Staff (5 Marks, sells back for 0)
  - C1 bag tiers = base +2/+4/+6 (1,200 rows, backup `items_bagslots_bak_20261021`)
  - C9 solo gates, plus Tacvi 18 and the Vxed paladin epic 6
  - A6 Riwwi key: per instance, 45 min
  
  Closed: TGB Broken?, Tradeskill from Depot, Taelosian Morphemic Staff, Braided Horsehair Handbag, Polished Silvery Key.
- **Still open:** Minor Adv Loot Bug (B5 vendor no-reply/money taken with full bags, B6 loot give out-of-zone), B7 spell-id audit, PoFire (needs info). Decisions: heals cap, quest item stats, HoT 66-71 AAs, Ikkinz #3 lockout, Liminal, early scribing.
