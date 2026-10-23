# Test agent instruction: A1 + A2 data fixes (2026-10-21)

Paste everything below the line into the test agent session.

---

You're working on the Ascendant EQEmu server repo (branch `ascendant`, last release tag `ascendant/2026-10-19`). Two player-reported data bugs need to ship as **custom DB migrations**, so they're in git and apply on test now and on prod with the next release.

**Where migrations go:**
- Add entries to `common/database/database_update_manifest_custom.h`, after version 13. Use the same `ManifestEntry` style as 12/13: `.check` / `.condition` / `.sql`, `.content_schema_update = true` for content tables.
- Bump `CUSTOM_BINARY_DATABASE_VERSION` in `common/version.h` (13 → 15).

**Both migrations must be idempotent and safe to re-run.** Prod already has A1 applied by hand.

## Migration 14: A1, own-class House of Thule AAs need level 70 (record only on prod)

**Bug:** `server/sql/2026_09_26_god_gate_release.sql` lowered `level_req` 70 → 65 on the 52 universal (cross-class) ranks of the 40 insight-trainer AAs (ids 2xxxx/3xxxx). Their native twins at `id - 20000` stayed at 70. As a result:
- Classes can't buy their own new HoT AAs in the trainer or the AA window.
- Other classes can buy them cross-class.

**Prod state:** already fixed on 2026-10-21 via `server/sql/2026_10_21_god_hot_native_aa_level65.sql`. That left backup table `asc_patch_god_hot_native_aa_20261021`. Expansion-17 counts on prod are now 104 ranks at 65 and 1,103 at 70.

**Migration:**
- description `2026_10_21_god_hot_native_aa_level65`
- `.check` = `SELECT n.id FROM aa_ranks n JOIN aa_ranks u ON u.id = n.id + 20000 WHERE u.id BETWEEN 20000 AND 39999 AND u.expansion = 17 AND u.level_req = 65 AND n.expansion = 17 AND n.level_req = 70 LIMIT 1`
- `.condition = "not_empty"` (on prod it's empty, so it's skipped)
- `.sql`:
  ```sql
  UPDATE aa_ranks n JOIN aa_ranks u ON u.id = n.id + 20000
  SET n.level_req = 65
  WHERE u.id BETWEEN 20000 AND 39999 AND u.expansion = 17 AND u.level_req = 65
    AND n.expansion = 17 AND n.level_req = 70;
  ```
- **Expect** 52 rows on a DB that has the 09-26 release applied. If test doesn't have it, the check is empty and it does nothing; that's fine.

## Migration 15: A2, AA swarm pets are permanently rooted

**Bug:** Discord #aa-bug-reports "State Of Swarm Pets".
- `zone/mob.cpp:441` (`permarooted = (runspeed > 0) ? false : true;`, stock since 2013) roots any NPC with `runspeed = 0`.
- 96 AA swarm-pet `npc_types` rows have `runspeed = 0`; the working low-rank pets have 1.25. Affected:
  - Call of Xuzl rank 6+ (SumFireSword2-6)
  - Doppelganger 3-13
  - Pack Hunt, Attack of the Warders (bstSwarm), Rise of Bones
  - Swarm of Decay 5-13 (ranks 2-4 already move)
  - Army of the Dead 2-18
  - Phantasmal Opponent
  - Nature's Guardian 7-14, Chattering Bones 2-4
  - Spirits of Nature, Projections, Lyrical Prankster, Illusory Ally
- Wards, utility summons (banker, resupply, eye) and Bane Spirit are stationary by design and are excluded.

**Migration:**
- description `2026_10_21_aa_swarm_pets_runspeed`
- `.check` = `SELECT id FROM npc_types WHERE runspeed = 0 AND id IN (<list>) LIMIT 1`
- `.condition = "not_empty"`
- `.sql`:
  ```sql
  UPDATE npc_types SET runspeed = 1.25 WHERE runspeed = 0 AND id IN
  (675,684,954,955,956,957,958,959,960,961,962,963,
   1318,1319,1320,1321,1322,1323,1324,1325,1326,1327,1328,1329,1330,1331,1332,1333,1334,
   1335,1336,1337,1338,1339,1340,1341,1342,1343,1344,1345,1346,1347,
   1348,1349,1350,1351,1352,1353,1354,1355,1356,1357,1358,1359,1360,1361,1362,1363,1364,
   1365,1366,1367,1368,1369,1370,1371,1372,1373,1374,1375,1376,1377,1378,1379,1380,
   1381,1382,1383,1384,1385,1386,1387,1388,1389,
   1408,1409,1410,1411,1412,1413,1414,1415,1416,1417,1418,1419);
  ```

**Before committing:**
- Run `SELECT id, name, runspeed FROM npc_types WHERE id IN (<list>)` on test. Confirm all 96 are AA swarm pets and none is a ward or utility summon.
- List any id that isn't a swarm pet in your report and drop it from the list.
- Confirm the pet names against `spells_new.teleport_zone` for the AA spells (SE_TemporaryPets).

**Rollback** (document it in the commit message; don't ship it as a migration):
```sql
UPDATE npc_types SET runspeed = 0 WHERE id IN (<same list>);
```

## Test on the test server
1. Build, then start the test server so migrations 14 and 15 run. Check `db_version.custom_version = 15` and the expected row counts in the world log.
2. **A2:** on a test char, cast a high-rank swarm AA (Army of the Dead, Call of Xuzl rank 6+, Pack Hunt or Rise of Bones) on a mob that moves. The pets should chase and melee it. Repeat for Swarm of Decay 5+.
3. **A1:** a level-65 warrior sees and can buy Howl of the Warlord at the insight trainer and in the AA window.
4. **Regression:** a ward or utility summon still stays put.

## Deliverable
- One commit on `ascendant`: both migrations plus the version bump. Message lists the bugs, the expected row counts and the rollback SQL.
- Report back with:
  - the commit hash
  - the row counts seen on test
  - the in-game results
  - any ids removed from the A2 list

**Prod rollout:** prod picks this up with the next release build and reboot. Migration 14 is a no-op there; migration 15 applies the pet fix.
