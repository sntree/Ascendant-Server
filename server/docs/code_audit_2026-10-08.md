# Ascendant code audit — 2026-10-08

Read-only assessment of `/opt/akk-stack/code` (customized EQEmu, C++20) performed under the rules in `modernization_agent_brief.md`. No source, quest, config, rule, or database content was changed; no builds, tests, binaries, or service actions were run; git was only read with `GIT_OPTIONAL_LOCKS=0`. Database observations are tiny indexed/aggregate SELECTs made through the read-only EQEmu MCP on 2026-10-08 and are labelled "stored DB" below. Nothing here was verified in-game or in staging.

Baseline facts (confirmed):

- Checkout HEAD is upstream `024abf74a` (2026-01-23, "Fix inventory overflow bug overwriting equipment slot 0"); the preceding commits are stock EQEmu (`brainiac`, `Knightly`, etc.). All Ascendant customization is **uncommitted**: 69 modified files (+3079/−251 lines) plus 3 untracked files (`zone/gm_commands/adventure.cpp`, two `account_login_server_links` repository headers). Modified-file mtimes span 2026-02-25 to 2026-09-25.
- Production binaries are symlinks into this tree (`/opt/akk-stack/server/bin/zone -> /home/eqemu/code/build/bin/zone`). Any build here is a deploy. `CMAKE_BUILD_TYPE=RelWithDebInfo` with `-Wno-everything` (PERF-01 in the plan still stands).
- `/opt/akk-stack/code_updates` contains only SQL/quest packages (LDoN chest fix, tome AA mana, pet illusion, etc.). None of them touch C++ and none mention "idle". They are not relevant to the idle question.
- No crash logs are present (`get_crash_logs` returned none).

---

## (a) Executive summary — top items by player impact / risk

| # | ID | One-line | Confidence | Needs rebuild+restart? |
|---|----|----------|------------|------------------------|
| 1 | IDLE-03 | Zone-state save/restore is re-persisting **quest-spawned event NPCs forever**. `pojustice` currently has **4,512 saved dynamic NPCs** (3,306 `#a_flickering_spirit`, 1,102 `#A_Scourge_of_Honor`, 26 `#Event_Lashing_Control`), all restored on every boot with no script/timer context. This is the most concrete "idle/unload logic caused bugs" evidence found, and it hits GoD-style scripted encounters (`thedeep`, `poeartha`, `poair`, `potactics`, `necropolis` are already accumulating). | Confirmed source + stored DB | Fix: rebuild. Mitigation: rule change / GM `#repop` clears state (not run by me). |
| 2 | SEC-01 | Login-server account linking links a never-seen LS login to an existing local account **by name only**; the promised IP validation (`CheckAccountIPMatch`) is never called. Three login servers are configured (public eqemulator.net legacy, eqemulator.dev, local). Account takeover by registering a matching name on another LS. | Confirmed source | Rule can be disabled now (`World:EnableLoginserverAccountLinking=false`, world reads rules at boot — so effective after a world restart); proper fix is code. |
| 3 | IDLE-04 | `Character:IdleShrinkSize=1` + `Character:DisableEXPWhenIdle=true` are **custom** additions enabled in production. Restoring size uses the race default (ignores shrink buffs, pet DB sizes, charmed pets, pet swapped while idle), pet size 1 is **persisted** through `m_petinfo.size` on camp/zone, and EXP is silently withheld from quest hand-ins / kills while flagged idle (idle only clears on movement). | Confirmed source; stored rules | Rules can be zeroed without restart for new zones (rules load per zone boot); code fix needs rebuild. |
| 4 | IDLE-02 | Upstream `idle_when_empty` is on for **all 618 zone rows** (60 s) and 568 zones keep running for **1 hour** after emptying (`zone.shutdowndelay=3600000`), so empty zones sit in a frozen state where NPC buffs/DoTs, OOC regen, timed depops, pathing and AI do not tick; combined with the custom leash change (no `RestoreHealth`) and HP being saved to zone state, a damaged boss can persist at low HP across an empty period and even across a zone reboot. | Confirmed source; stored DB | Rule/column changes only (no rebuild); see recommendation. |
| 5 | BUG-01 | Pre-connect crash recovery (`Zone:PreConnectCrashRecoveryZones=all`, `ZoneID=0`) moves any player who drops between "zone contents sent" and "connect finished" to their **bind point** and sets `zone_instance=0`. A LD during zone-in to a raid/expedition instance ejects the player. | Confirmed source; stored rules | Rule change only to narrow scope. |
| 6 | SEC-02 | `OP_ShopPlayerBuy`: `mpo->price = single_price * mp->quantity` is 32-bit unsigned and `quantity` is only capped for stackables (to `StackSize`); the `price < 0` guard can never fire on a `uint32`. Wrap-around under-charging is possible for high-priced stackables and any non-stackable (`quantity` uncapped). Upstream-shared, but Ascendant has custom high-price vendors. | Confirmed source; exploitability depends on vendor prices (unverified) | Rebuild. |
| 7 | BUG-02 | Pet-bag characters (`item_id IN (93861, 2828)`) never save or load `character_pet_inventory`, so anything handed to a pet outside zone 344 is lost on zone/camp unless the Perl pet-bag plugin re-equips it. | Likely issue (plugin not fully read) | Rebuild or plugin verification. |
| 8 | PERF-05 | `SavePetInfo`/`LoadPetInfo` each run an extra `inventory` scan per character save/load; `HasLoreConflictInPendingParcels`/`RecipientHasLoreItem` use `item_id IN (SELECT id FROM items WHERE loregroup=…)` subselects (likely unindexed) on every parcel send / out-of-bazaar purchase. | Confirmed source; cost unmeasured | Rebuild for code; index is a DB change. |
| 9 | BUG-03 | Custom leash reset drops `RestoreHealth()` (relies on `NPC:OOCRegen`, which is 10 in ruleset 1 but 0 in ruleset 10 and unset elsewhere) — kite-to-leash chipping; with zone idle this HP persists. | Likely issue | Rebuild (or rule for OOCRegen in every ruleset). |
| 10 | COMPAT-01 / TEST-01 / PERF-01 | Still open from the plan: unbounded `rof2.cpp:5337` string decode; test gate ignores result; `-Wno-everything` RelWithDebInfo. | As in plan | As in plan |

Positive note: a number of the custom changes are genuine hardening (atomic bazaar claim, stale-listing checks, parcel/bazaar lore checks, refund on parcel save failure, guild-tribute `TRIBUTE_NONE` map-insert crash fix, NaN spawn-position guard, merchant visibility checks in `OP_ShopPlayerBuy`, barter kill-switch, `OP_AdventureMerchantPurchase` missing `return`). These should be preserved (BASE-01).

---

## (b) Idle code — history, what it does, bugs, verdict

### History (what is upstream vs. Ascendant)

| Piece | Origin | Evidence |
|---|---|---|
| Zone "idle when empty" (skip `Mob::Process` for NPCs when `numclients==0` after `seconds_before_idle`) | **Upstream EQEmu**, commit `742b437f2` "[Zones] Convert IDLE_WHEN_EMPTY to a Zone Column (#3891)" (older `#define IDLE_WHEN_EMPTY` before that) | `git log -S'IsIdleWhenEmpty' -- zone/entity.cpp`; `git diff HEAD -- zone/entity.cpp` shows only two unrelated group-leader null checks, so the idle block is byte-identical to upstream |
| Client auto-idle / auto-AFK (`CheckAutoIdleAFK`, `m_is_idle`, skipped movement packets) | **Upstream EQEmu**, commit `567d46c3d` "[Performance] Auto Idle / AFK (#4903)" | `git log -S'CheckAutoIdleAFK'` |
| Zone state save/restore on shutdown (`zone_save_state.cpp`, `Zone:StateSavingOnShutdown`) | **Upstream EQEmu** (#4875, #4889; file unmodified locally) | `git diff HEAD -- zone/zone_save_state.cpp` is empty |
| `Character:IdleShrinkSize`, `Character:DisableEXPWhenIdle`, `EVENT_IDLE_ON/OFF`, 5-second un-idle grace | **Ascendant custom** (uncommitted) | `/opt/akk-stack/code/common/ruletypes.h:242-243`, `zone/client_packet.cpp:17600-17645`, `zone/exp.cpp:501-503`, `zone/event_codes.h:150-151`, `zone/embparser.cpp:213-214`, `zone/lua_parser.cpp:194-195`, `zone/lua_general.cpp:6998-6999` |
| Fast zone shutdown (`Zone:AutoShutdownDelay=5000`) and per-zone `shutdowndelay` | **Stored DB configuration**, not code | `rule_values` ruleset 1; `zone.shutdowndelay` distribution |

So the "someone added idle logic" story is partly a misattribution: the mechanisms that freeze or unload zones are upstream Akkadius performance features that this server **enables and tunes aggressively**, and the Ascendant-specific additions sit on top of the client-side idle flag. The bug fallout comes from the combination.

### What it does (source-confirmed)

1. **Zone idle** — `/opt/akk-stack/code/zone/entity.cpp:461-527` (`EntityList::MobProcess`). When `zone->IsIdleWhenEmpty()` and `numclients==0`, a static settle timer of `GetSecondsBeforeIdle()` runs; after it fires, every NPC whose `Spawn2` is not `path_when_zone_idle` and while `!zone->quest_idle_override` gets **no `Process()` call at all** — only `GetDepop()` is honored (line 524). `DoorProcess` is also skipped (`entity.cpp:398-402`). Trader/buyer tables are cleared at idle (`entity.cpp:493`). Spawn2 respawn timers (`zone.cpp:1546-1557`), corpses, objects, encounters, quest timers, instance timers and weather keep running. Defaults come from the `zone` table (`zone.cpp:1011-1012`, fallback `true/60` at `zone.cpp:992-993`).
   - Stored DB: all 618 `zone` rows have `idle_when_empty=1, seconds_before_idle=60`; only 231 of 165,923 `spawn2` rows have `path_when_zone_idle=1`; `quest_idle_override` is not used by any quest (grep of `/opt/akk-stack/server/quests`).
2. **Zone shutdown** — `zone.cpp:1600-1605` returns false from `Zone::Process` when the autoshutdown timer fires with 0 clients (non-static zones; only `guildlobby` is static per `eqemu_config.json` `staticZones`). `StartShutdownTimer` (`zone.cpp:1847-1874`) uses the `zone.shutdowndelay` column when it differs from `Zone:AutoShutdownDelay`.
   - Stored DB: `Zone:AutoShutdownDelay=5000` (upstream default 60000). `zone.shutdowndelay`: **568 rows = 3,600,000 ms (1 h)**, 47 rows = 5,000 ms (all DoN/DoDH zones: corathus*, drachnidhive*, dreadspire, *korlach*, illsalin*, gukc, mirc, rujj, shadowspine, take), `tacvi` = 5 h, `airplane` = 12 h, `kithicor` = 24 h. So most zones: idle after 60 s, unload after 1 h.
3. **Zone state save/restore** — `zone.cpp:871-872` saves on zone destruction when `Zone:StateSavingOnShutdown` (stored: true; `StateSaveBuffs=true`, `StateSaveEntityVariables=true`, `StateSaveClearDays=7`). `zone_save_state.cpp:699-819` writes every Spawn2 (with the live NPC's HP/mana/loot/buffs/grid/position), every **non-spawngroup NPC that is not a pet/swarm/aura/bot/merc/trap/id<100/id==500** (lines 738-762), and NPC corpses. `spawn2.cpp:540` restores it on boot; `zone_save_state.cpp:600-631` re-creates the dynamic NPCs with `new NPC(...)` and `entity_list.AddNPC`, then `LoadNPCState` (287-337) restores HP (`SetHP(s.hp)` if >0), buffs, loot and position. `attack.cpp:2578-2582` refuses to let a resumed NPC die for 10 s after resume (`npc.cpp:615-617`). `ClearZoneState` runs only on `Zone::Repop` (`zone.cpp:1971-1973`) and on invalid state.
   - Stored DB (2026-10-08): `zone_state_spawns` = 65,070 rows over 182 zones, 15 corpses, **5,404 dynamic NPC rows**, oldest 2026-09-20.
4. **Client idle** — `client_packet.cpp:17542-17667`. Idle after `SecondsBeforeIdle{Combat,NonCombat}Zone` (stored 1800 / 480 s) without a position change; `ResetAFKTimer()` (`client.h:495-501`) also refreshes `m_last_moved` and is called on cast (`client_packet.cpp:4611, 4839`), item move (`11083`), manual AFK toggle (`15081`), and every tick while `auto_attack` is on (`client_process.cpp:532-534`). While idle, `MobMovementManager` skips sending NPC/PC position updates to that client (`mob_movement_manager.cpp:843, 887`); un-idle triggers `SyncWorldPositionsToClient` (`client_packet.cpp:17669`). Idle is cleared **only** by `has_moved` (line 17627), not by actions.
   - Ascendant additions on idle-on (17608-17617): `ChangeSize(IdleShrinkSize, unrestricted=true)` on self and `GetPet()`; EXP suspension message. On idle-off (17635-17645): `ChangeSize(GetDefaultRaceSize(), true)` on self and current pet; `EVENT_IDLE_ON/OFF` fire. `exp.cpp:501-503` returns early from `Client::AddEXP` when `DisableEXPWhenIdle && IsIdle()`.
   - Stored rules (ruleset 1): `EnableAutoAFK=true`, `IdleShrinkSize=1`, `DisableEXPWhenIdle=true`, `AutoIdleFilterPackets=true`, `SecondsBeforeAFKCombatZone=1800`, `SecondsBeforeAFKNonCombatZone=900`.

### Bug list

**IDLE-01 — Upstream client idle: EXP withheld and shrink applied to stationary players (custom additions).** Confirmed source.
- `exp.cpp:501-503`: every `AddEXP` call (kills, quest `exp()`/task rewards, AA EXP, resurrection EXP return) is silently dropped while `m_is_idle`. Only movement clears idle; casting/attacking refresh the timer *before* idle but do not un-idle afterwards (`client_packet.cpp:17627` requires `has_moved`). Scenario: a player who stands still ≥30 min in a combat zone (camped puller/healer pre-event, box character, someone waiting on a raid) becomes idle, then receives a quest turn-in or a kill without moving — no EXP, only the earlier yellow "suspended" line. Non-combat zones (8 min) include PoK/Nexus hand-in hubs.
- `client_packet.cpp:17608-17614` shrinks to size **1** (unrestricted). `mob.cpp:4514-4525` clamps to 1.0. Size affects `CombatRange` reach and targeting/visual; players report "tiny" characters/pets in hubs.
- Restore uses `GetDefaultRaceSize()` (`mob.cpp:8015-8020`, race/gender default): a player under a shrink buff (or a custom size from illusion/quest `SetSize`) returns to full default size while the buff still shows; a pet with a DB/pet-power size (`pets.cpp:144, 291`) is set to the generic race height; a **charmed** NPC used as pet is resized permanently; if the pet changed while idle (re-summon does not require movement) the new pet gets the race default.
- **Persistence leak**: `client.cpp:1067` saves `m_petinfo.size = pet->GetSize()` and `pets.cpp:291` restores it. A player who camps or zones while idle (camping needs no movement; `/gate`/evac/summon do not go through `has_moved`) stores pet size 1 and the pet comes back size 1 after the pet is re-created, with no idle-off ever running for it.
- Smallest fix: set `Character:IdleShrinkSize=0` and `Character:DisableEXPWhenIdle=false` (rules; effective per zone boot — zones booted after the change pick it up; the hour-long dynamic zones and `guildlobby` keep old values until they cycle). Code fix (rebuild): remember pre-idle size per entity and restore that; make `AddEXP` idle check apply only to kill EXP from `ExpSource::Kill` with a visible message, or drop it.
- Test: staging, stand still past the thresholds, hand in a quest item, verify EXP; shrink-buffed player + mage pet idle/un-idle; camp while idle, log in, check pet size.
- Impact: player-facing, pre-GoD relevant (long raid events with stationary roles).

**IDLE-02 — Upstream zone idle freezes NPC simulation for up to an hour per empty zone.** Confirmed source; no incident established.
- With `entity.cpp:461-527`, after 60 s empty the NPCs stop ticking: `NPC::Process` (`npc.cpp` ~lines 50-130) owns `BuffProcess` (buff/DoT durations), OOC/HP/mana regen, `swarm_timer` depops, stun/enrage/assist timers, grid/roambox movement and `AI_Process`. Visible effects when someone enters: mobs resume mid-path, boss debuffs/buffs resume with the same tics remaining, an NPC that leashed at 20% HP is still at 20% until OOC regen catches up, charm/fear states persist, temporary (swarm) pets with no owner never depop until the zone unloads, door open/close timers stall.
- With `ClientLinkdeadMS=60000` and `FastLinkdeadCleanupZones=344`, a raid wipe where everyone releases leaves the zone empty, idle after 60 s, and the boss's **HP and buffs are then written to `zone_state_spawns`** if the zone unloads within the hour (`SaveNPCState` at `zone_save_state.cpp:687`), and restored by `LoadNPCState` (`:294-296`). So a boss at 5% can survive an empty period *and* a reboot at 5%, then regenerate only from the moment the first player enters (10%/tic with `NPC:OOCRegen=10`).
- `NPC:OOCRegen` is 10 in ruleset 1 ("default", 217 zones) and **0 in ruleset 10** (`EQEmu_Default`, no zones) and **unset for rulesets 2 (`pop+`, 359 zones), 4 (`GoD`, 2 zones), 5 (`raidzone`, 7 zones), 20** — rulesets inherit from ruleset 1 only via `RuleManager` fallback semantics, which were not verified here. If GoD/raid rulesets resolve OOCRegen to 0, there is no regen at all after a leash (see BUG-03).
- Smallest fix: (a) exclude raid/event zones from idle/unload with `zone.idle_when_empty=0` for GoD raid and trial zones, or (b) set `Zone:StateSaveBuffs=false` and accept HP restoration, or (c) rebuild with a patch that calls `RestoreHealth()`/`BuffFadeAll()` for engaged-but-empty NPCs on entering idle (upstream behaviour before the leash change already did this on leash). Option (a) is a DB column change applied per zone boot; no rebuild.
- Test: staging; damage a boss, evac all, wait >60 s, re-enter; also wait > shutdown delay and re-enter.

**IDLE-03 — Zone-state restore resurrects quest-spawned event NPCs without their scripts and they accumulate.** Confirmed source + stored DB.
- `zone_save_state.cpp:738-762` persists every NPC with `GetSpawnGroupId()==0` except pets/swarm/aura/bot/merc/trap/id<100/id==500. NPCs created by `eq.spawn2`/`quest::spawn2` in encounter scripts fall through this filter. `:600-631` re-creates them at boot via `new NPC` with `SetResumedFromZoneSuspend(true)`, **without** the Lua encounter timers (`quest_manager.ClearAllTimers()` runs on boot) and without the controller's wave state.
- `pojustice`: `/opt/akk-stack/server/quests/pojustice/#Event_Lashing_Control.lua:88-115` spawns `201469/201467/201463…` per trial and depops them only from the controller's timers (`:44-58, 121-127`). After a zone unload mid-trial, the restored spirits/scourges have no controller timer; the next trial spawns a fresh set; the saved set grows. Stored DB today: 3,306 `#a_flickering_spirit` + 1,102 `#A_Scourge_of_Honor` + 26 controllers + 27 punishers + … = **4,512 dynamic NPCs in pojustice**, all re-created on each boot (each one costs `LoadNPCTypesData` + full `NPC::Process` per tick once a player is present). Smaller accumulations: `thedeep` 216 (`A_core_burrower`, `A_stone_carver`…), `soltemple` 111 `an_incarnate`, `poeartha` 101, `necropolis` 76, `poair` 61 (`Avatar` adds), `eastwastes` 52, `potactics` 42, `akheva` 39.
- Effects: zone-in lag and CPU in those zones, encounters that check `IsMobSpawnedByNpcTypeID` (`#Event_Lashing_Control.lua:52`) think a wave is still alive and stall, trash that was supposed to despawn stays, exp/loot farming of restored event adds (they are killable after the 10 s window), and 65k-row `zone_state_spawns` churn on every shutdown (`DeleteWhere` + `InsertMany` of thousands of rows at `zone_save_state.cpp:797-816`).
- Smallest fix (code, rebuild): in `SaveZoneState` skip NPCs whose name starts with `#` or that carry an entity variable/flag set by `quest::spawn2` (e.g. add `ignore_npcs |= n.second->IsQuestSpawned()` using a new flag set in `lua_general.cpp:1343-1496`/`questmgr` spawn paths), and/or skip NPCs with an active depop/swarm timer. Alternative without code: `zone.idle_when_empty` is irrelevant here; set `Zone:StateSavingOnShutdown=false` (loses the intended respawn/loot persistence) or per-zone `shutdowndelay` long enough that encounters finish. Operational cleanup: a GM `#repop` in affected zones clears the rows (`zone.cpp:1971-1973`) — recommend, not performed. Note from memory: no restarts/hotfixes on the live server; `#repop` is a zone action the owner must decide on.
- Test: staging `pojustice`, start trial, let zone empty and unload, re-enter, count `#a_flickering_spirit`.

**IDLE-04 — Custom idle hooks are applied before the stored thresholds are sane for raids.** Covered under IDLE-01; listed separately because the fix is pure rules. Also note `EVENT_IDLE_ON/OFF` are exported to Perl/Lua but no quest uses them (`grep -rl idle_on /opt/akk-stack/server/quests` — none), so they are inert.

**IDLE-05 — Minor/cleanup.** `MobProcess` uses function-static `old_client_count`/`mob_settle_timer` (upstream); fine for one zone per process but prevents unit testing. `SyncWorldPositionsToClient(bool ignore_idle)` flips `m_is_idle` false then false again (`client_packet.cpp:17669-17712`) — the `reset_idle` branch is a no-op; harmless.

### Verdict per piece

| Piece | Recommendation |
|---|---|
| Upstream client auto-idle (packet filtering) | **Keep.** It is the actual load saver and is low risk. |
| `Character:IdleShrinkSize=1` | **Revert via rule (set 0)** now; delete the code or fix size memory later. Cosmetic benefit, several correctness bugs, persistence leak into pet size. |
| `Character:DisableEXPWhenIdle=true` | **Revert via rule (set false)** before GoD; if an anti-AFK-leech is wanted, re-implement for kill EXP only, grouped-with-killer only, and with un-idle on any action. |
| `EVENT_IDLE_ON/OFF`, 5 s grace | **Keep** (inert / harmless). |
| Upstream zone idle-when-empty (60 s) | **Keep for normal zones; disable (`idle_when_empty=0`) for GoD raid/trial and event zones**, or extend `seconds_before_idle` there. No rebuild. |
| `Zone:AutoShutdownDelay=5000` + 1 h `shutdowndelay` | **Keep** but understand that it is what makes IDLE-02/03 bite. Consider 5–10 min for trial/instance zones so scripted encounters complete or clean up before unload. |
| Zone state save (`StateSavingOnShutdown=true`) | **Fix** (filter quest-spawned NPCs; do not persist HP/buffs for engaged-with-nobody NPCs) **before** GoD if possible; otherwise **disable `StateSaveBuffs`** and plan a `#repop` cleanup of `pojustice` and other accumulators at the next maintenance. |

---

## (c) Findings

Format: ID — title. Behavior/why it matters. Evidence (absolute path:line). Confidence. Smallest fix. Test. Production impact (rebuild? restart? rule/DB?). Next action.

### Exploits / security

**SEC-01 — Login-server account linking by bare name, IP check never applied.**
- Behavior: when a login from any configured login server has an `lsaccount_id` with no local account, world looks up a local account by `LOWER(name)` only and, if found, links and logs in as that account with its status (GM level included). The rule text promises "IP validation from account_ip history" but `Database::CheckAccountIPMatch` has no callers.
- Evidence: `/opt/akk-stack/code/world/cliententry.cpp:336-370` (`GetAccountIDByName(m_login_account_name, &found_status)` → `m_account_id = found_id; m_admin = found_status`), `:49-66` (link table lookup on CLE creation), `/opt/akk-stack/code/world/login_server.cpp:772-774, 781-784`, `/opt/akk-stack/code/common/database.cpp:17-42` (name-only overload, no loginserver filter), `:51-62` (`CheckAccountIPMatch`, unused: `grep -rn CheckAccountIPMatch` finds only the definition), `/opt/akk-stack/code/common/ruletypes.h:362` (rule text). Config: `eqemu_config.json` `world.loginserver1/2/3` = `login.eqemulator.net` (legacy), `worldserver.eqemulator.dev`, `127.0.0.1`. Stored rule: `World:EnableLoginserverAccountLinking=true`.
- Confidence: confirmed source logic; exploit requires registering the victim's account name on a login server where it is free (plausible across three servers) — not tested.
- Smallest fix: require `CheckAccountIPMatch(found_id, ip)` **and** same `ls_id` family before linking, or require an explicit one-time link token; log the link as a player event. Interim: set `World:EnableLoginserverAccountLinking=false` (world reads rules at boot; effective after the next world restart — do not restart for this alone unless the owner judges the exposure warrants it) and audit `account_login_server_links` for links created from a different `ls_id` than the account's original.
- Test: staging with two login servers, same account name, different LS ids.
- Impact: world-only change; no zone rebuild needed for the rule; code fix needs world rebuild/restart.
- Next: owner decision on disabling now vs. at next maintenance; run the read-only audit query on `account_login_server_links`.

**SEC-02 — Merchant purchase price wrap (`uint32 price = single_price * quantity`).**
- Evidence: `/opt/akk-stack/code/zone/client_packet.cpp:14470-14472` (`mpo->price = single_price * mp->quantity`), `:14474-14480` (`if (mpo->price < 0)` on a `uint32`, `common/eq_packet_structs.h:2235`), quantity cap only for `item->Stackable` (`:14440-14442`); non-stackables are charged `price*quantity` and receive one item (overcharge), stackables with `StackSize*price ≥ 2^32` can be under-charged. `TakeMoneyFromPP(uint64)` receives the already-wrapped value.
- Confidence: confirmed arithmetic; exploitability depends on whether any vendor sells an item with `Price*SellRate*StackSize ≥ 4.29e9` copper (not checked; `items` was not scanned).
- Smallest fix: compute in `uint64`, clamp `quantity` to 1 for non-stackables, reject when `price > MAX_TRANSACTION_VALUE`-style limit. Upstream may already have this; compare before patching.
- Test: unit test on the arithmetic; staging buy of a 1000-stack at a high price.
- Impact: zone rebuild + rolling restart. Next: read-only `SELECT MAX(price*stacksize) FROM items WHERE stacksize>1` (bounded) to confirm reachability.

**SEC-03 — Kick class gate removed (`can_use_kick` commented out).**
- Evidence: `/opt/akk-stack/code/zone/special_attacks.cpp:589-593`. Any class can send `OP_CombatAbility` kick; damage is skill-based so low, but `CheckIncreaseSkill(SkillKick…)` is attempted and the `DoubleSpecialAttack` extra-kick loop (`:607-630`) applies. Probably intentional (custom), but it widens the client-packet surface. Confidence: confirmed source; intent unverified. Fix: re-enable the gate or gate on `GetSkill(SkillKick) > 0 || MaxSkill(...) > 0`. Impact: rebuild. Next: owner confirms intent.

**SEC-04 — Bazaar reconcile paths mutate DB inventory/currency directly for a trader not in memory.**
- Evidence: `/opt/akk-stack/code/world/zoneserver.cpp:1706-1766` (deletes `inventory` rows and credits `character_currency` when the trader CLE is not found), `/opt/akk-stack/code/zone/worldserver.cpp:3805-3863` (deletes rows when the online trader has no in-memory match). The buyer-side checks (`trading.cpp:2924-2971`, atomic claim `trader_repository.h:116-128`) are good. Risk: if the trader is online but briefly absent from the CLE (zoning crash recovery, CLE stale sweep), the world path credits currency in DB that the trader's next `Save()` overwrites (money loss) while the inventory row deletion is not re-applied in memory until relog (item appears present but is gone → confusing, not a dupe). Confidence: likely edge case, not reproduced. Smallest fix: in the world branch, refuse when `FindCLEByCharacterID` exists in any state, and queue a parcel refund instead of touching currency. Impact: world rebuild. Next: review player_event_logs for `ServerOP_BazaarPurchase ... not found in world` occurrences (read-only).

**SEC-05 — Handlers without an early size check (sampled list).** `Handle_OP_Camp, DisarmTraps, FeignDeath, GuildCreate, Mend, QueryUCSServerStatus, SenseTraps, TradeAcceptClick, Trader` lack a size check in their first 30 lines (`awk` scan of `/opt/akk-stack/code/zone/client_packet.cpp`). Most do not read `pBuffer`; `Handle_OP_Trader` dispatches on action with per-case checks. Confidence: sampled only. Next: no action; COMPAT-01 (rof2.cpp:5337) remains the real bounded-decoding item.

**SEC-06 — GM/permission checks.** New `#adventure` command is registered at `AccountStatus::GMMgmt` (`/opt/akk-stack/code/zone/command.cpp:84`); world handlers (`zoneserver.cpp:1338-1347`) trust the zone-sent requester name (consistent with upstream inter-server trust). `tellsoff = gm_hide_me && RuleB(Command, HideMeCommandDisablesTells)` (`client_packet.cpp:1497`) — fine. No new permission bypasses found in the diff.

### Bugs / correctness

**BUG-01 — Pre-connect crash recovery relocates players to bind and clears instance.**
- Evidence: `/opt/akk-stack/code/zone/client_process.cpp:369-470`; `should_recover_pre_connect_crash` is true for any zone (`Zone:PreConnectCrashRecoveryZones=all`), `PreConnectCrashRecoveryZoneID=0` so the configured-zone branch is skipped and `try_recover_to(GetBindZoneID(0)…)` runs (`:453-457`), writing `character_data.zone_id/zone_instance=0` directly (`:402-410`). Also the diagnostic dump (`:257-326`) logs every mob in the zone at `LogInfo` on each such disconnect — a raid zone with 400 NPCs produces 400 log lines per event.
- Confidence: confirmed source; frequency unknown (log grep not done to avoid player-data collection).
- Smallest fix: narrow `PreConnectCrashRecoveryZones` to the zones where the RoF2 bulk-spawn crash was actually seen (the trace code targets `GUILDLOBBY`), or exclude instances (`zone->GetInstanceID()>0`) in code; gate the dump behind a log category.
- Test: staging, kill the client during zone-in to an expedition.
- Impact: rule change only (per zone boot) for the scope; rebuild for the code guard. Next: owner decides scope before GoD (expedition-heavy content).

**BUG-02 — Pet inventory not persisted for pet-bag owners.**
- Evidence: `/opt/akk-stack/code/zone/zonedb.cpp:3075-3080, 3144-3146` (skip inventory save), `:3329-3339` (skip inventory load). `Pets:DenyTradeZones=344` only blocks hand-ins in the guild lobby (`npc.cpp:4181-4196`), so elsewhere pets still accept items (`CanPetTakeItem`). Unless `/opt/akk-stack/server/quests/plugins/ascendant_pet_bag_system.pl` re-equips on every pet (re)creation, items handed directly to a pet by a bag owner vanish on zoning/camp. Also the stale `character_pet_inventory` rows of bag owners are never cleared.
- Confidence: likely issue; plugin only located, not read.
- Smallest fix: either deny direct pet trades for bag owners everywhere (`DenyTradeZones=all` style) or keep saving inventory and let the plugin reconcile. Test: hand a weapon to a pet in a normal zone as a bag owner, zone, inspect pet. Impact: rule (if the string accepts "all"—it does not today; `npc.cpp:4185` compares integers) or rebuild. Next: read the plugin.

**BUG-03 — Leash reset no longer restores health.**
- Evidence: `/opt/akk-stack/code/zone/mob_ai.cpp:1143-1150` (comment: rely on `RuleI(NPC, OOCRegen)`). Stored: `NPC:OOCRegen=10` only in ruleset 1; `0` in ruleset 10; absent in rulesets 2/4/5/20 (inheritance semantics unverified). Players can kite a boss across its leash repeatedly to bank damage; with IDLE-02 the damage persists through empty periods.
- Confidence: confirmed source; balance intent unknown. Smallest fix: `RestoreHealth()` when the NPC has no hate after leash **or** ensure every ruleset defines `NPC:OOCRegen`. Test: staging, kite across leash, observe HP. Impact: rebuild (code) or rule rows (no restart; effective per zone boot). Next: owner confirms intent.

**BUG-04 — `Zone:StateSaveBuffs` + HP restore makes boss state survive reboots.** See IDLE-02/03; evidence `zone_save_state.cpp:294-296, 321, 659-682`. Fix: `StateSaveBuffs=false`; do not save `hp` for NPCs that are not at full HP and have an empty hate list (code). Impact: rule only for the first part.

**BUG-05 — `HybridCasterSpellDmgBonus` applies to Cleric although the rule text says Dru/Sha.**
- Evidence: `/opt/akk-stack/code/zone/effects.cpp:367-374, 443-450` vs `common/ruletypes.h:169-170`. Confidence: confirmed source. Fix: align text or code. Impact: rebuild or doc only. Next: owner confirms intended balance.

**BUG-06 — `TemporaryPets` re-anchors `GetPet()` to a swarm pet for clients with no pet.**
- Evidence: `/opt/akk-stack/code/zone/aa.cpp:190-204, 310-324, 546-560`, `client.cpp:7313-7327`. `SetPetID(swarm)` while `GetSwarmInfo()` is set; when the swarm pet times out, `DepopSwarmPets`/`Depop` paths clear via `SetPetID(0)`? Not verified — if not, `GetPet()` returns null after entity removal (safe) but the pet window state (`SetPetCommandState`) may linger; also IDLE-01 shrink would resize a swarm pet. Confidence: unverified hypothesis. Test: summon swarm with no pet, let it expire, check pet window and `/pet` commands. Impact: rebuild if an issue.

**BUG-07 — `GetWeaponDamageBonus` returns 0 off-hand below level 40** (`attack.cpp:3491-3492`); `Flurry` now rolls on every primary double attack instead of triple attack (`attack.cpp:6972-6986`); `Familiar` fade no longer dismisses the pet (`spell_effects.cpp:4499-4500`); AA EXP allowed from level 1 (`exp.cpp:567, 856`). All intentional-looking balance changes without rule gates; listed so BASE-01 captures them. Confidence: confirmed source.

**BUG-08 — Beastlord creation table opens all races** (`world/client.cpp:2071`) — intentional custom; note for BASE-01.

**BUG-09 — `MAX_PP_AA_ARRAY` 240→300** (`common/eq_packet_structs.h:891`) matches RoF2/SoF+ structs (300) and the RoF2 encoder padding loop is now zero-length (`common/patches/rof2.cpp:2660`), but **Titanium** structs are 240 (`common/patches/titanium_structs.h:763`): the Titanium encoder loops were not read; if a Titanium client ever connects it may overflow. Confidence: unverified for Titanium. Impact: none for RoF2-only.

**BUG-10 — Pre-connect "Guild Lobby zone-in trace" logging** (`client_packet.cpp:1159-1246`, `client.cpp:7030-7039`) and the linkdead `LogInfo` escalation (`client.cpp:913-966`) are permanent at Info level. Not a correctness bug; noise and I/O. Cleanup item CLEAN-02.

Already-fixed-upstream-bugs worth keeping (do not revert): `tribute.cpp:437-456` and `world/wguild_mgr.cpp:314-346` (`TRIBUTE_NONE` map insertion crashing RoF2 clients), `mob.cpp:1294-1313` (NaN position guard), `corpse.cpp:571-576` (decay timer not reset on load), `client_packet.cpp:2323` (`return` after adventure-merchant error), `trading.cpp:1397-1409` (atomic claim), `parcels.cpp:458-470`/`trading.cpp:3052-3065` (lore checks), `trading.cpp:3146` (refund).

### Performance

**PERF-05 — Extra DB round-trips in hot paths (custom).**
- `SavePetInfo`/`LoadPetInfo` inventory existence query per character save/load (`zonedb.cpp:3075-3080, 3329-3334`); `CompleteConnect` `FindOne`+`UpdateOne` on `character_data` per zone-in (`client_packet.cpp:905-920`); parcel/bazaar lore subselects `item_id IN (SELECT id FROM items WHERE loregroup = …)` (`parcels.cpp:33-62`, `trading.cpp:45-61`) — `items.loregroup` index presence not checked (schema read only via docs). `OP_ShopPlayerBuy` now runs `DataBucket::GetData` per bucket-gated merchant entry (`client_packet.cpp:14382-14390`) — only when `bucket_name` set.
- Confidence: confirmed source; cost unmeasured. Smallest fix: cache `has_pet_bag` on the client object (refresh on inventory change); add index on `items.loregroup` (DB change); precompute lore groups. Test: query count per save in staging. Impact: rebuild / schema. Next: measure with `player_event_logs`-free query logging in staging.

**PERF-06 — `CommandOwnedTempPetsToAttack` walks the full mob list per pet command** (`client_packet.cpp:17-76`, called at `:11346-11351, 11421, 11513`). O(mobs) per `/pet attack`; in a 500-NPC zone with 30 raiders spamming pet attack this is noticeable but bounded. Fix: track swarm pets per owner. Impact: rebuild. Low priority.

**PERF-07 — Zone state churn**: `SaveZoneState` deletes and re-inserts the full per-zone row set (65k rows server-wide, up to 4.8k in `pojustice`) on every unload; `LoadZoneState` re-creates thousands of NPCs on boot (IDLE-03). Fixing IDLE-03 fixes this.

**PERF-08 — `SPELL_CAST` player event enabled by default with 2-day retention** (`common/events/player_event_logs.cpp:1134-1140`, `client_packet.cpp:4617-4635`): one `player_event_logs` insert per cast server-wide. For 30 players this is modest but it is the single largest new write stream; it is batched by the event-log queue. Confidence: confirmed source; volume unmeasured. Next: confirm the `player_event_log_settings` row for id `SPELL_CAST` (read-only) and decide retention.

PERF-01..04 remain as in the plan; nothing found here changes their ranking. `mob_ai.cpp:1038-1048` (PERF-04) now also calls `zonemap->CheckLoS` before `FindDestGroundZ`, which is cheaper than before but the `CanPathTo` call still precedes the cheap distance checks.

### Cleanup

**CLEAN-01 — Duplicated 60-line damage-bonus blocks** in `effects.cpp:149-211` vs `:272-334` and `:455-508` vs `:555-608` (crit/non-crit paths). Extract `ApplyAscendantSpellDamageMods(spell_id, value, target, is_dot)`; behavior-preserving; makes future balance edits single-site. Rebuild.

**CLEAN-02 — Diagnostic logging left at Info**: guild-lobby zone-in trace, pre-connect crash dump (`client_process.cpp:257-326`), `ReportConnectingState` escalated to Info. Move behind `LogZoning`/a custom category. Rebuild.

**CLEAN-03 — Commented-out code** (`attack.cpp:6994-7008` old flurry, `:3665-3668` old epic fist, `special_attacks.cpp:573` `class_id`). Delete. Rebuild (cosmetic).

**CLEAN-04 — Duplicate helpers**: `HasLoreConflictInPendingParcels` defined in both `parcels.cpp:33` and `trading.cpp:45`; swarm-pet "command anchor" block repeated four times (`aa.cpp`, `client.cpp`). Factor into `Client::AnchorSwarmPetForCommands(NPC*)`.

**CLEAN-05 — `GiveDoubleRiposte[3]` kept "for script compat"** (`zone/common.h:538-539`) while new `GiveDoubleRiposteSkill[HIGHEST_SKILL+1]` is used; `SBIndex` constants removed — grep Perl/Lua exporters for `GiveDoubleRiposte` before deleting.

**CLEAN-06 — `zoning.cpp:796` `shard_instance_duration = 100000000`** (≈3.17 years) and the repository now filters by `expire_at` (`character_data_repository.h:59-66`) — consistent, but the magic number should be a named constant shared with the repository.

**CLEAN-07 — `SyncWorldPositionsToClient` dead `reset_idle` branch** (`client_packet.cpp:17669-17712`), upstream.

---

## (d) Coverage ledger

| Area / path | Coverage | Notes |
|---|---|---|
| Full uncommitted diff (69 files) | **Deeply reviewed** | Every hunk read, grouped by subsystem (`client_packet`, `client_process/client`, trade/inventory/parcel/loot/tribute/bazaar, attack/special/mob, spells/effects/aa/bot, world/database/events, rules/servertalk/exp/zonedb/corpse/groups/raids/pets/perl/lua, entity/zone/mob_ai/npc/pathing/waypoints/hate/zoning). |
| `zone/entity.cpp` idle block, `zone/zone.cpp` (ctor, Process, shutdown timers, AddAuth/GetAuth, Repop), `zone/zone_save_state.cpp` (whole file), `zone/spawn2.cpp:280-310,540`, `zone/npc.cpp` Process head + CanPetTakeItem, `zone/client_packet.cpp` CheckAutoIdleAFK/Sync, ShopPlayerBuy, AdventureMerchantPurchase | Deeply reviewed | |
| `zone/mob_movement_manager.cpp:830-900`, `zone/mob.cpp` ChangeSize/GetDefaultRaceSize/HateSummon diff, `zone/client.cpp:1067`, `zone/pets.cpp:291` | Sampled | |
| `common/ruletypes.h` | Indexed + diff read | Stored values pulled only for named rules (Idle/AFK/Zone/State/OOCRegen/Ascendant/World/Pets/Spells) |
| `zone/client_packet.cpp` handler size checks | Sampled (awk scan) | Not a full packet audit |
| `common/patches/rof2.cpp` | Sampled (AA encoder loops only) | COMPAT-01 not re-read |
| `world/*` diff, `world/client.cpp` char-create table | Diff read | Login flow outside the diff not read |
| Quests | Sampled | `pojustice/#Event_Lashing_Control.lua`, idle-related greps; pet-bag plugin located only |
| Database | Narrow reads | `rule_values` by name, `rule_sets`, `zone` aggregates (`idle_when_empty`, `seconds_before_idle`, `shutdowndelay`), `spawn2.path_when_zone_idle` count, `zone_state_spawns` aggregates (count by zone / top npc_id). No player rows read. |
| Build/CMake | Indexed | `assets/scripts/Makefile:270-300`, `build/CMakeCache.txt` flags |
| Not reviewed | — | `tests/`, `loginserver/`, `ucs/`, `queryserv/`, Lua/Perl bindings beyond the diff, `zone/trap.cpp`, `zone/doors.cpp`, `zone/tasks*.cpp`, `zone/dynamic_zone*.cpp`, bot code beyond the diff, `common/inventory_profile.cpp` slot validation, Titanium/SoD encoders, `zone/questmgr.cpp` spawn paths (needed for the IDLE-03 fix design) |

"No match in these files" is not proof of absence; the exploit survey is a sample of the custom diff plus one scripted scan.

---

## (e) Before vs. after the GoD launch

**Consider before launch (no rebuild options exist for most):**
1. SEC-01 — decide on `World:EnableLoginserverAccountLinking=false` (takes effect at next world restart) and audit the links table (read-only).
2. IDLE-01/04 — set `Character:IdleShrinkSize=0` and `Character:DisableEXPWhenIdle=false` (rules; new zone boots pick them up; `guildlobby` and already-running zones keep the old values until they cycle — per memory, no forced restarts).
3. IDLE-02/03 — for GoD raid/trial/sewer zones: `zone.idle_when_empty=0` (or longer `seconds_before_idle`) and a `shutdowndelay` that outlives a trial; set `Zone:StateSaveBuffs=false`. Plan the `#repop` cleanup of `pojustice`, `thedeep`, `poeartha`, `poair`, `soltemple`, `necropolis` for a GM-chosen moment (clears accumulated event NPCs via `ClearZoneState`). Rows are also aged out by `StateSaveClearDays=7`, but restores happen before that.
4. BUG-01 — narrow `Zone:PreConnectCrashRecoveryZones` from `all` to the guild lobby (or empty) so LD-on-zone-in does not eject raiders from expeditions.
5. BUG-03 — add `NPC:OOCRegen` rows to rulesets 2/4/5/20 if ruleset inheritance does not already provide 10 (verify `RuleManager` fallback in `common/rulesys.cpp` — not read here).

**After launch / next maintenance window (rebuild required):**
- IDLE-03 code fix (filter quest-spawned NPCs from state save; skip HP/buff persistence for un-engaged damaged NPCs).
- SEC-02 merchant price arithmetic; SEC-03 kick gate decision; SEC-04 world reconcile hardening.
- IDLE-01 proper size-memory implementation if shrink is wanted back.
- PERF-05/06/08, CLEAN-01..07, plus the plan's PERF-01 and TEST-01.
- BUG-02 pet-bag inventory semantics after reading the plugin.

Nothing above was implemented; all changes remain owner decisions.
