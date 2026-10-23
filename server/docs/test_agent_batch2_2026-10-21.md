# Test agent instruction: batch 2 (2026-10-21)

Paste everything below the line into the test agent session.

---

Five fixes for Ascendant, all on test first. Target: tonight's update if possible; B1 and B2 come first.

**Conventions:**
- Code goes on `ascendant`, rebased on `origin/ascendant`.
- DB changes are custom migrations in `common/database/database_update_manifest_custom.h`. Fellowships took 14 and A1/A2/A5 take the next ones, so use the next free number and bump `CUSTOM_BINARY_DATABASE_VERSION`.
- Before editing a quest file, back it up as `<file>.bak-20261021`.
- Full analysis for B1 and B2 is in `server/docs/tob_agent_brief_2026-10-21.md`.

## B1. ToB: TGB is always off (one line)

- **Problem:** ToB never sends OP_TGB (`patch_TOB.conf:101` = 0x0000), so `Client::tgb` stays false. Group spells then ignore the target (`zone/spells.cpp:2173`, `:2516`), and ToB players can't buff another player's group.
- **Fix:** `zone/client.h:934`:
  ```cpp
  bool TGB() const { return tgb || ClientVersion() >= EQ::versions::ClientVersion::TOB; }
  ```
- **Test:** on ToB, target a player in another group and cast a group buff; that group gets it. Targeting an NPC still buffs your own group. RoF2 `/tgb` behavior is unchanged.

## B2. ToB: auto-combine from a bag container fails silently (one line)

- **Problem:** `common/patches/tob.cpp:10078` sets `emu->combine.some_id = eq->some_id`. The decoder's comment says the container's item id is in `eq->recipe_id` (+4), but that value is only logged. `GetTradeRecipe` (`zone/tradeskills.cpp:1569-1585`) then can't match bag recipes (e.g. 17901 Mixing Bowl), and the combine fails with no message at `tradeskills.cpp:611-615`.
- **Fix:**
  ```cpp
  emu->combine.some_id = eq->recipe_id ? eq->recipe_id : eq->some_id;
  ```
- **Confirm first:** do one bag combine with Netcode logging on. The raw line at `tob.cpp:10075` should show the bowl id in `recipe_id`.
- **Test:** a recipe-window combine from a mixing bowl in a bag works, including with the "Personal Depot" box checked. World containers still work.

## C9. Remove the group-size gates left on expeditions (quest, solo policy)

- **Change:** set `min_players=1` (keep `max_players`) in:
  - `oasis/a_wayward_kiraikuei.lua:3` (6)
  - `draniksscar/Grand_Historian_Gersh.lua:5,12,19` (3)
  - `natimbi/#Kemik_Arulon.lua:3` (3)
  - `nedaria/#Redis_Linrok_the_Suffering.lua:2` (2)
  - `dranik/Grand_Historian_Rygua.lua:20,41` (3)
  - `provinggrounds/Projection_of_{Realms,Arcana,Power,Body,Mind,Tactics}.lua` (2)
- **Also:** grep the same files for any other "need N players" checks and fix those too. Report anything in a zone that's reachable in the current era (expansion 7).
- **Test:** request the Natimbi Kemik Arulon DZ solo; you get the expedition.

## C5. Taelosian Morphemic Staff on the Marks vendor (DB migration)

- **Why:** item 66617 gives +700 Yunjo Slave Resistance and +700 Nihil when equipped. The Riwwi BiC faction checks count it. It has no source today.
- **Migration:**
  - `.check` = `SELECT item FROM merchantlist WHERE merchantid=344046 AND item=66617`
  - `.condition = "empty"`
  - `.sql`:
    ```sql
    INSERT INTO merchantlist (merchantid, slot, item, faction_required, level_required, alt_currency_cost, min_expansion, max_expansion)
    VALUES (344046, 13, 66617, -1100, 0, 5, -1, -1);
    ```
    DESCRIBE `merchantlist` first and match the existing rows' defaults (they're slots 1-12 with `faction_required` -1100).
- **Rollback:** `DELETE FROM merchantlist WHERE merchantid=344046 AND item=66617;`
- **Test:** Exarch Valeth (guildlobby) sells it for 5 Marks.

## C1. Bag tier upgrades shrink bags over 10 slots (DB migration)

- **Problem:** the Enhanced, Exalted and Ascendant variants (base id +300000/+500000/+700000) have `bagslots = min(base,10) + 2/4/6`. 400 base bags with more than 10 slots (up to 60) get *smaller* when upgraded; a 14-slot Braided Horsehair Handbag 17219 is only 12 at Enhanced.
- **New rule (owner approved):** tier slots = base + 2 / + 4 / + 6.
- **Migration:**
  ```sql
  UPDATE items t JOIN items b ON t.id = b.id + 300000 SET t.bagslots = b.bagslots + 2 WHERE b.id < 200000 AND b.bagslots > 10;
  UPDATE items t JOIN items b ON t.id = b.id + 500000 SET t.bagslots = b.bagslots + 4 WHERE b.id < 200000 AND b.bagslots > 10;
  UPDATE items t JOIN items b ON t.id = b.id + 700000 SET t.bagslots = b.bagslots + 6 WHERE b.id < 200000 AND b.bagslots > 10;
  ```
  - Use a `.check` that finds any tier row below its new value.
  - Expect about 1,200 rows (400 × 3).
- **Rollback:** before the update, snapshot `id, bagslots` of those rows to a backup table in the migration, and keep the restore SQL in the commit message.
- **Client check first:** on test, open a 40+ slot bag on **both RoF2 and ToB** (server bag `SLOT_COUNT` is 200). If either client shows or holds fewer slots than the DB says, cap the new values at what the client supports and report the cap.
- **Test:**
  - Handbag 17219's tiers are 16/18/20.
  - Items inside an upgraded bag stay put after a relog on both clients.

## Deliverables

1. Commit hash(es) on `origin/ascendant` for B1, B2, C5 and C1, with migration numbers.
2. The B2 Netcode log line showing the bag id in `recipe_id`.
3. Unified diffs and backup file names for the C9 quest files, plus any extra player-count checks you found.
4. The C1 client slot check result (RoF2 and ToB), any cap applied, and the row counts.
5. A short in-game test note per item.
6. One line per item on whether it makes tonight's update.
