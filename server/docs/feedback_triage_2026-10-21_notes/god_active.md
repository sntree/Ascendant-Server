# #god-bug-reports — ACTIVE threads triage (2026-10-21)

Read-only pass. Nothing posted to Discord, no DB writes, no files edited besides this one.

---

### Vengeful Spirits AA (1561993751570944031, created 2026-10-20, last 2026-10-20)
- Summary: Druid can't see/buy Vengeful Spirits; the rogue (cross-class) can. `aainfo 20637` on the druid shows a different (level 70) requirement.
- Category: bug (config / DB data)
- Status: still happening. No staff reply.
- Evidence checked (confidence high):
  - `aa_custom_mapping`: universal 20637 -> original 637 (classes 9776 = SHD/DRU/SHM/NEC/ENC).
  - Universal chain 33017/33018/33019 = level_req 65 (lowered by `server/sql/2026_09_26_god_gate_release.sql`). Native chain 13017/13018/13019 = level_req **70**, expansion 17.
  - `server/quests/plugins/ascendant_insight_trainer.pl:336-351` (browse) and `:452-454` (aainfo) use the **original** rank chain for native-class players (`_has_native_access`). Next rank level 70 > 65, so it's hidden in browse, shows "Level Required 70" in aainfo and is blocked at `:804`. The regular AA window shows the native ability with level 70 too.
  - Systemic: all 40 HoT (exp 17) trainer abilities have native first ranks at 70. The release lowered only the 52 universal ranks. A query pairing universal rank id N with native rank N-20000 finds exactly **52 native ranks, all exp 17, all level 70**. So every class is locked out of its *own* new HoT AAs while other classes can buy them.
  - Not the guildlobby "missing next rank" fix. That fix is in this plugin (`_count_ranks`/`_get_nth_rank_id`, lines 1031-1071). File mtime 2026-10-13 12:11 CDT, before the 2026-10-18 23:30 restart, so it is already live.
- Recommended action: fix (DB, live at the next normal restart, since AA data loads at zone boot; no hotfix):
  ```sql
  CREATE TABLE asc_patch_god_hot_native_aa_backup AS
    SELECT n.id, n.level_req FROM aa_ranks n JOIN aa_ranks u ON u.id = n.id + 20000
    WHERE u.id BETWEEN 20000 AND 39999 AND u.expansion = 17 AND u.level_req = 65
      AND n.expansion = 17 AND n.level_req = 70;            -- expect 52 rows
  UPDATE aa_ranks n JOIN asc_patch_god_hot_native_aa_backup b ON b.id = n.id SET n.level_req = 65;  -- expect 52
  -- rollback:
  UPDATE aa_ranks n JOIN asc_patch_god_hot_native_aa_backup b ON b.id = n.id SET n.level_req = b.level_req;
  DROP TABLE asc_patch_god_hot_native_aa_backup;
  ```
  Vengeful Spirits rank 4 (13396, level 71, exp 18) stays hidden. That's intended until the next unlock. This change is a subset of the open owner follow-up ("AAs up to 71 should be accessible", memory aa-unlock-schedule) and can ship on its own.
- Solo/policy flags: none.
- Suggested reply: "Yep, found it. Your own class's new AAs are stuck at a level 70 requirement, other classes got the right one. Fix goes in with the next update."

### Not sure a pile of bones spawned at the completion of Ikky 3 trial (1561787218186739752, created 2026-10-19, last 2026-10-19)
- Summary: Samp (WAR 65) finished Sanctuary of the Transcendent (Ikkinz raid #3) but found no pile of bones / Artifact of Transcendence.
- Category: user error (probable) + progression tip
- Status: no staff reply. Another player offered help.
- Evidence checked (confidence med):
  - `ikkinz/#Vrex_Xalkak_Nixki.lua:50-54`: on Nixki death it spawns 4x `a_pile_of_bones` 294596 in the arena (519,-247 / 521,-156 / 556,-266 / 618,-232). The bones are 11-hp NPCs you kill and loot. Loottable 90431 drops Artifact of Transcendence 60172 at 100%.
  - It works for others: 60172 is held by Arily, Daki, Trills, Bjorn, Apresia and more. Arily/Daki/Trills got lockout5 at H67 (Nixki kill) on 2026-10-21.
  - Samp has no 60172, has no current `ikkylockout5` (can run again now), and has `ikkyraid=1` while holding Artifact of Glorification 60171 that was **never handed to Tublik**. `kodtaz/Tublik_Narwethar.pl:276-294` needs ikkyraid==2 before 60172 advances the flag, so Samp must turn in 60171 first. Truk is in the same state (ikkyraid=1, holds 60170+60171).
  - Raid #3 class mechanics: sentinels pick a class present in the zone (`294598.lua pick_class`), so it's fine solo. The Marauder (`Vrex_Xalkak-s_Marauder.pl:43-63`) rotates its archetype every 75s, so a solo player has to wait for their archetype's window.
- Recommended action: reply. Optional owner review: the pet killing-blow case (`$class` / `e.other:GetClass()` of a pet fails the check) can trap pet classes.
- Solo/policy flags: kill-class checks may fail when a pet lands the killing blow (pet-class solo concern). Not verified in game.
- Suggested reply: "When Nixki dies, 4 piles of bones pop up around the room where you fought him. You have to kill one to loot the artifact. Your lockout's clear, so you can run it again. Also hand Tublik the Glorification artifact first or the last one won't count."

### BIC #4 did not reward the Green Hued Gemstone (1561526450434351155, created 2026-10-18, last 2026-10-18)
- Summary: The Green Hued Gemstone combine said success / recipe learned but gave nothing.
- Category: already fixed
- Status: fixed, not answered in-thread. `global/global_player.pl:97-130` (EVENT_COMBINE_SUCCESS for recipes 10334/10346/10903) called an undefined `plugin::ClassType`, so the script died and gave no gemstone and no tools back. `plugins/class_type.pl` was added 2026-10-19 22:01 CDT, after the last restart. It loads in zones booted after that or after a quest reload.
- Evidence checked (confidence high): `plugins/class_type.pl` exists. Melladee's `char_recipe_list` now shows the whole chain made (10335 Muddy, 10341, 10346, 10899, 10903, 10906, 10925, 10945, 13364, 13370 Black Gemstone of Pain), so she got past it. Other 10334 makers (Meatboy, Shrine, Carmen) hold their gemstone + tools.
  - Side note: recipes 10334 and 10903 have Gemcutter Tools 67704 with no successcount (the script re-summons the tools, so this is fine now). Before the fix, failed runs also ate the tools.
- Recommended action: reply + close. No restore needed for Melladee (she progressed).
- Solo/policy flags: none.
- Suggested reply: "That one's fixed, and it looks like you got through the rest of the chain. Closing this out, thanks."

### Rikabi the Riddler - not taking Right Hand (1561030958290051153, created 2026-10-17, last 2026-10-17)
- Summary: Rikabi said "I have no need for this" and handed Right Hand back, but the door unlocked. The reporter self-resolved.
- Category: working as intended
- Status: resolved by the reporter.
- Evidence checked (confidence high): `tipt/#Rikabi_the_Riddler.lua:186-200`: the door unlocks on any riddle hand-in, and the correct item is returned with that exact line.
- Recommended action: close.
- Solo/policy flags: none.
- Suggested reply: "Yep, that's normal. He hands it back and the door opens. Closing."

### Braided Horsehair Handbag (Ascendant) (1560665871830229052, created 2026-10-16, last 2026-10-16)
- Summary: Base bag is 14 slots, Enhanced drops to 12, Ascendant ends at 16. The reporter expected 20.
- Category: bug (item tier generation)
- Status: still the case. No staff reply. Players note it hits every 14+ bag.
- Evidence checked (confidence high): `items` 17219 = 14 slots, 317219 = 12, 517219 = 14, 717219 = 16. Across all bags (base id < 200000 with +300000/+500000/+700000 tiers), tier slots = min(base,10) + 2/4/6. **400 base bags with >10 slots (11..40)** get fewer slots at Enhanced (a 40-slot bag becomes 12). RoF2/ToB invbag SLOT_COUNT is 200 (`code/common/patches/rof2_limits.h:225`), so larger bags are supported.
- Recommended action: owner decision on the formula (suggest base+2/+4/+6, or at least GREATEST(tier, base)). Item DB change, so it goes in at a normal restart:
  ```sql
  CREATE TABLE asc_patch_bag_tier_slots_backup AS
    SELECT t.id, t.bagslots FROM items b JOIN items t ON t.id IN (b.id+300000, b.id+500000, b.id+700000)
    WHERE b.id < 200000 AND b.bagslots > 10;                 -- expect ~1200
  UPDATE items t JOIN items b ON t.id = b.id + 300000 SET t.bagslots = b.bagslots + 2 WHERE b.id < 200000 AND b.bagslots > 10;
  UPDATE items t JOIN items b ON t.id = b.id + 500000 SET t.bagslots = b.bagslots + 4 WHERE b.id < 200000 AND b.bagslots > 10;
  UPDATE items t JOIN items b ON t.id = b.id + 700000 SET t.bagslots = b.bagslots + 6 WHERE b.id < 200000 AND b.bagslots > 10;
  -- rollback: UPDATE items t JOIN asc_patch_bag_tier_slots_backup k ON k.id = t.id SET t.bagslots = k.bagslots; DROP TABLE asc_patch_bag_tier_slots_backup;
  ```
  Check RoF2 client display for 30+ slot bags before going above 40.
- Solo/policy flags: none.
- Suggested reply: "Good catch. The upgrades are capping big bags lower than they should. Looking at a fix for an upcoming update."

### Sanctuary of the Transcendent (1560620428006006924, created 2026-10-16, last 2026-10-16)
- Summary: Truk is told "You have recently completed a raid" when requesting the Ikkinz raid #3 instance.
- Category: working as intended (lockout), with a solo-friendliness concern
- Status: lockout has since expired. Truk has no `ikkylockout5` now and can request again. No staff reply.
- Evidence checked (confidence med): `kodtaz/Tublik_Narwethar.pl:144-165` blocks on `ikkylockout5`. `ikkinz/lockout_ikkinz.lua:41-48`: lockout5 is set H33 when **Guardian of Transcendence** dies (signal 5, `Guardian_of_Transcendence.pl` death) and H67 on Nixki. A failed attempt after the Guardian kill locks you out for 33h. Truk: ikky=10, ikkyraid=1, holds 60170+60171 (Glorification not yet turned in to Tublik).
- Recommended action: reply. Owner consideration: move the H33 lockout off the Guardian death so a failed solo attempt can be retried (lockout only on Nixki).
- Solo/policy flags: a 33h lockout from a mid-event kill punishes failed solo attempts.
- Suggested reply: "That was the lockout from your earlier run. It's cleared now so you can grab a new one. Also turn the Glorification artifact in to Tublik if you haven't, you'll need that before the last one counts."

### Zone out of chambers of destruction (1560400111987855421, created 2026-10-15, last 2026-10-16)
- Summary: The Ikkinz trial exit dropped players onto the re-entry trigger (zone loop), and a trio got split into two instances.
- Category: already fixed
- Status: Straps patched it 2026-10-15 and joined the instances. Truk thanked him.
- Evidence checked (confidence high): `kodtaz/#Zoner4.pl`, `#Zoner5.pl`, `#Zoner6.pl` (mtime 2026-10-15 16:48 CDT) have a 10s zone-in ignore via `kodtaz/player.lua` `kt_zonein`. `Tublik_Narwethar.pl:370-383 PARTY_IKKY_INSTANCE` joins a groupmate's instance. `#Zoner7.pl` (raid #4) was not enlarged (±10) and has no guard, so it shouldn't overlap. Not tested in game.
- Recommended action: close.
- Solo/policy flags: none.
- Suggested reply: "This should be all sorted now. Closing, thanks for the reports."

### Smith Rondo event in Ferubi (1560385379276951582, created 2026-10-15, last 2026-10-16)
- Summary: No Pxet Elite spawned after a full Ferubi clear in a raid DZ. Players say it works in OW.
- Category: bug (design tuning, instance)
- Status: still the case. No staff reply.
- Evidence checked (confidence high): `ferubi/Event_Spawner.lua:27/36/45/54`: each elite spawns at the **100th** kill from specific spawngroups (287779+287826 blade, 287782 spear, 287783 hammer, 287784 brawler), with per-instance counters. Spawn point counts: 66 / 46 / 43 / 24. One DZ clear can never reach 100. `data_buckets` shows DZ counters stalled at exactly those numbers (inst131: blade 66, spear 46, brawler 25). OW counters are shared and persist (blade 47, brawler 76). The instance buckets have no expiry and will carry over when an instance id is reused.
- Recommended action: fix (quest, reload-safe, applied at owner's normal reload/restart). In `Event_Spawner.lua`, use a lower instance threshold, e.g. `local need = (eq.get_zone_instance_id() == 0) and 100 or 20` and replace `== 100` with `>= need` on lines 27/36/45/54. Also pass an expiry on instance keys (`eq.set_data(key, val, "6h")` when instance_id ~= 0). Rollback = restore the file.
- Solo/policy flags: 100 kills x4 camps is a heavy solo grind. The DZ path is impossible as-is.
- Suggested reply: "Yeah, they only pop after a lot of kills and a DZ clear doesn't get there right now. Do that part in the open world for now. Looking at making it work in DZs too."

### Missing warrior AA (1560377132113334432, created 2026-10-15, last 2026-10-15)
- Summary: Truk (WAR) sees none of the new GoD/HoT warrior AAs. Others confirm you can't buy your own class's new AAs, but other classes can.
- Category: bug, same root cause as Vengeful Spirits (dupe-ish)
- Status: still happening. Straps replied "ahh ok" and no fix since.
- Evidence checked (confidence high): warrior HoT trainer abilities Howl of the Warlord 744, Warlord's Fury 912, Warlord's Resurgence 911: native first ranks 13169/10546/10545 at level_req 70, universal copies at 65. See the Vengeful Spirits entry for the trainer code path.
- Recommended action: fix with the same 52-row SQL as Vengeful Spirits. Reply once it's scheduled.
- Solo/policy flags: none.
- Suggested reply: "Found it. Your own class's new AAs got left at a level 70 requirement. Fix goes in with the next update."

### BiC Quest #1 - Qinimi (1559649271421341800, created 2026-10-13, last 2026-10-16)
- Summary: Stone of Entry needs to be on the cursor (answered). Then the execution event double-fired and broke in a raid DZ.
- Category: already fixed
- Status: Straps removed the duplicate script on 10-15. A player confirmed it worked on 10-16.
- Evidence checked (confidence high): `qinimi/#Trigger_Qinimi_1.pl.disabled-20261014` is disabled, and no other duplicate .pl/.lua pairs remain in qinimi.
- Recommended action: close.
- Solo/policy flags: none.
- Suggested reply: "Glad it's working now. Closing this one."

### Unable to buy Gelid Rending Rank 2 (1559614054446927925, created 2026-10-13, last 2026-10-13)
- Summary: Error buying Gelid Rending rank 2. Straps: "some ranks end at HoT, need to explore."
- Category: already fixed (trainer). Data note below.
- Status: the trainer was fixed 2 minutes after the report (plugin mtime 2026-10-13 12:11 CDT = 17:11 UTC) and has been live since at least the 10-18 restart. Not confirmed in-thread.
- Evidence checked (confidence high): `aa_ranks` 12846 / 32846 (rank 1) point `next_id` to 12847 / 32847, which **don't exist**. `plugins/ascendant_insight_trainer.pl:1031-1071` now stops at missing rows (the comment cites Gelid Rending). Core `zone/aa.cpp:1907` already sets next_id -1 when the row is missing. 13 HoT trainer abilities are single-rank for the same reason (Blessing of Ro, Calculated Insanity, Celestial Rapidity, Companion's Sacrifice, Concussive Intuition, Crippling Aurora, Dreary Deeds, Gelid Rending, Glyph Spray, Howl of the Warlord, Nature's Blessing, Nature's Fury, Spirit of the Bear). Optional cleanup: set next_id=-1 on those 26 rows. It's cosmetic only.
- Recommended action: reply + close.
- Solo/policy flags: none.
- Suggested reply: "Gelid Rending only has the one rank on our server right now, so rank 1 is max. The trainer won't offer rank 2 anymore. Closing."

---

## Top issues in this channel
1. **Own-class HoT AAs locked at level 70** (Vengeful Spirits + Missing warrior AA, and it hits every class). 52 native ranks need level_req 65. One SQL, live at the next restart. Highest impact: every level-65 player is missing their class's new AAs.
2. **Pxet Elites can't spawn in a DZ** (Smith Rondo / BiC #4). The 100-kill threshold is higher than the camp spawn counts per clear, and it's a heavy solo grind in OW. Quest-only fix.
3. **Bag tier upgrades shrink bags over 10 slots** (400 bases, Enhanced drops to 12). Item DB fix, owner picks the formula.
4. **Ikkinz raid #3: 33h lockout on a mid-event kill**, plus players not turning in Glorification before Transcendence (Samp, Truk). Reply with tips. Consider moving the lockout to the final kill only.
5. Closeable: BiC #4 gemstone (fixed), Chambers zone-loop (fixed), Qinimi BiC #1 (fixed), Rikabi (WAI), Gelid Rending (trainer fixed).
