# ToB/upstream agent brief: player-reported server bugs (2026-10-21)

Paste everything below the line into the ToB/upstream agent session.

---

You're working on the Ascendant EQEmu server code (`/opt/akk-stack/code`, branch `ascendant`). Deployed is tag `ascendant/2026-10-19`, HEAD 5452bb91f. It's a live server with RoF2 and ToB clients both in use.

These come from player reports on Discord, triaged 2026-10-21. Every file:line below was read at HEAD. Treat them as starting points and re-check before you change anything.

**Rules:**
- Make fixes on a branch, on test only. Don't touch prod containers, the DB or reloads; the owner deploys at a normal restart.
- Keep RoF2 behavior unchanged unless an item says otherwise. Gate ToB-only changes on `ClientVersion() >= EQ::versions::ClientVersion::TOB`.
- Match the surrounding code style. Commit with clear messages.
- For each item, report: what changed (file:line), how you tested it (packet log or test char), and any risk.

**Priority order:** B1, B2, B3, B4, B5, B6, B7.

## B1. TGB is always off for ToB clients (HIGH, 1 line)

**Symptom:** ToB players can't land group buffs on another player's group. `/tgb` in ToB says TGB is always on now. MGB works.

**Cause:**
- `utils/patches/patch_TOB.conf:101` has `OP_TGB=0x0000`, so the client never sends it.
- `Client::tgb` is initialised false (`zone/client.cpp:259, 570`) and only set in `Client::OPTGB` (`zone/client_process.cpp:1435-1450`).
- `zone/spells.cpp:2173` (group spell target) and `:2516` (range uses aoe_range) both need `TGB()`. Without it, `spell_target = this`.
- Rule `Character:EnableTGB` = true.

**Fix:** `zone/client.h:934`:
```cpp
bool TGB() const { return tgb || ClientVersion() >= EQ::versions::ClientVersion::TOB; }
```
Check that nothing else assumes `tgb` and `TGB()` match (e.g. `zone/api_service.cpp:761`, lua/perl `TGB()`; those are fine).

**Test:** a ToB caster targets a player in another group and casts a group buff. That group should get it. Targeting an NPC should still buff the caster's own group.

## B2. ToB auto-combine from an inventory container (mixing bowl in a bag) silently fails (HIGH, 1 line)

**Symptom:** recipe-window combine works with world containers (oven), but not with a container in a bag. The button greys briefly, then nothing happens and no message shows. The player first thought it was the Tradeskill Depot; it isn't.

**Cause (medium confidence, no packet capture):** `common/patches/tob.cpp:10066-10085` `DECODE(OP_RecipeAutoCombine)`.
- The decoder's own comment (:10071-10072) says `eq[+4]` (`recipe_id`) is the container object's id, 0 for world containers.
- But :10078 sets `emu->combine.some_id = eq->some_id` (+36). The +4 value is logged and then dropped.
- `GetTradeRecipe(recipe_id, c_type, some_id)` (`zone/tradeskills.cpp:1569-1585`) needs the bag's item id. Bag recipes are keyed by container item id; 17901 Mixing Bowl alone has about 3222 recipe entries.
- So the lookup fails, and it takes the silent "Unknown recipe for HandleAutoCombine" branch (`tradeskills.cpp:611-615`).

**Fix:**
```cpp
emu->combine.some_id = eq->recipe_id ? eq->recipe_id : eq->some_id;
```

**Verify first:** turn on Netcode logging and do one in-bag combine. The raw log line at `tob.cpp:10075` should show the bowl's item id (e.g. 17901) in `recipe_id`.

**Optional:** make the unknown-recipe branch send a chat message, the way the other failure paths do.

**Also check:** the alchemy/tinkering/poison checks at `tradeskills.cpp:636-668` reply with `OP_TradeSkillCombine` instead of `OP_RecipeAutoCombine`. Confirm that's harmless for ToB.

## B3. ToB item-loss paths: bulk moves and shared bank phantom slots (HIGH)

**Symptoms:**
- The MQ "bebags" script deleted items moved out of the bank (Straps restored them by parcel).
- Items in ToB shared bank slots 3+ vanished at logout. Patched in 41b75bcf7, but there are gaps.

The server has `SHARED_BANK_SIZE = 2` (`common/patches/tob_limits.h:80`).

### 3a. OP_MoveMultipleItems has no ToB decode

- `patch_TOB.conf:255` maps 0x6bf7, but there's no entry in `tob_ops.h`/`tob.cpp`, so the raw ToB struct goes to `Handle_OP_MoveMultipleItems` (`zone/client_packet.cpp:11898`).
- It adds `Slot + SHARED_BANK_BEGIN` (around :11944, :11979-11987) with no bound check. `CalcSlotId` returns INVALID_INDEX for shared bank slot >= 2 (`common/inventory_profile.cpp:1137-1158`).
- `to_slot` 0xFFFFFFFF passes `IsValidSlot` (`zone/inventory.cpp:1695`), and `SwapItem` treats `to_slot == INVALID_INDEX` as **destroy the cursor item** (`inventory.cpp:1810`).
- Also, `m_inv.GetItem(mi->from_slot)->IsStackable()` (around :11949) has no null check.

**Fix:** add a ToB decode that translates each slot with `TOBToServerSlot`. Reject (don't destroy) any slot that doesn't map, the same way OP_MoveItem uses `TOB_REJECT_SLOT` (`tob.cpp:9938-9945`). Bound-check and null-check in the handler for all clients.

### 3b. No resync after a rejected move

`client_packet.cpp:11876` only calls `SwapItemResync` when both slots pass `IsValidSlot`. A `TOB_REJECT_SLOT` destination fails that check, so the client keeps showing the item in a phantom slot until relog, while the server has it on the cursor.

**Fix:** resync, or send the cursor item back, when either slot is the reject value.

### 3c. The server advertises unlimited shared bank slots

`tob.cpp:6839` sends `eq->entries[11] = -1; // Shared Bank Slots` in `ENCODE(OP_SendMembership)`.

**Try:** `invtype::SHARED_BANK_SIZE` (2) and see whether ToB then hides or locks slots 3+. That would close the whole class of bug at the source. Not verified.

**Context:** the owner also has a pending MOA bank-box patch waiting on these bank fixes.

## B4. ToB cursor-bag contents saved one slot off, so the character crashes on load (HIGH, root cause not located)

**Symptom:** a ToB player buys bags from a merchant while a bag (starter Backpack) and items are on the cursor, and goes linkdead. Afterwards the character crashes the client on load.

- Hand-fixed twice: Mortician (char 130277) on 10-19 and Menanu (130278) on 10-21. In both, the backpack was stuck on the cursor with its contents saved one sub-slot off.
- No server crash dumps, so it's client-side, from bad inventory data.
- Nivmizzit (130276) currently has an empty Backpack (17005) on the cursor (slot 33).

**Ask:**
- Find the off-by-one in the ToB↔server sub-slot translation for the cursor bag (`CURSOR_BAG_BEGIN` 6010; `TOBToServerSlot`/`ServerToTOBSlot` and the merchant buy / cursor push path).
- Add a defensive load-time fix: if a cursor bag's children don't line up, move the bag and its contents to a free general or bank slot instead of sending bad data.

**Reproduce on test:** starter Backpack on cursor with items in it, buy a bag from a merchant on ToB, camp or LD, log back in, and inspect `inventory` rows 33 and 6010-6209.

## B5. Merchant buy: money lost when bags are full, and the ToB quantity window gets stuck (MEDIUM, all clients for the money part)

`zone/client_packet.cpp` `Handle_OP_ShopPlayerBuy` (15136-15400):
- `TakeMoneyFromPP` runs at :15289, *before* the room checks at :15322 (cursor occupied) and :15330 (no free slot). Both return without a refund. The upstream comment at :15286 even asks about it.
- These paths send **no** OP_ShopPlayerBuy reply: :15237 (lore), :15305 (money check), :15322 and :15330. The ToB quantity window then stays open (the player reports it clears only when you start another purchase).
- The sell path already sends a dummy reply "to satisfy the client" (:15446-15456).

**Fix:**
- Move the room check above `TakeMoneyFromPP`, or refund with `AddMoneyToPP(mpo->price)` on those returns.
- Add a helper that queues a "nothing bought" OP_ShopPlayerBuy (quantity 0, price 0, same npcid/playerid/itemslot) on the silent failure returns.

**Test:** on ToB, buy a stackable with full bags, then buy rapidly. Confirm the window closes and no money is lost. Run the RoF2 regression too.

## B6. Advanced Loot: "give" to an out-of-zone member leaves the row stuck (MEDIUM)

`zone/corpse_advloot.cpp:1016-1021`:
- `GetClientByName` fails for an out-of-zone target, so the server sends a chat line and `SendAdvLootClearPending` only, then returns.
- e7483cb27 fixed refusals for Loot/Sell/Store with `RefuseAdvLootRequest` (result 6, :161-165), but Give has no sequence number and wasn't covered.
- Only a roll resolution (0x11 state / 0x13 roll result keyed by ask id, :1314-1343) unsticks the row.

**Fix:** after `SendAdvLootClearPending`, re-send the row state. If `transaction_id` names a live ask, use `SendAdvancedLootStateTo(c, transaction_id, ask->second)`; otherwise `SendAdvancedLootItems(c)`.

Do the same at :976-978 (item missing / GM-locked) and :962-964 (not master looter). `client_packet.cpp:2354-2358` (corpse not found) sends nothing at all; compare the Loot path at :2305-2316.

Don't echo 0xe naming the master looter: that moves the item to their Personal list.

## B7. Spell-id hardening, alongside the pending int32 attack_proc fix (LOW, audit)

The 65535 sentinel bug (NPC spell-list procs as uint16 with -1 = 65535; `zone/mob_ai.cpp:2440`, zonedb.h attack_proc) was fixed by deleting `spells_new` 65535. The int32 code fix is still pending with you.

When you do it, also audit:
- uint16 spell-id fields anywhere, and any `0xFFFF`/`0xFFFE` sentinel compares.
- Spell ids now go up to 74875, and 65534 is a real spell (Ecliptic Reinforcement 3).

Never import a spell at 65535 (`reserved_spell_ids.tsv`).

---

**Deliverable:** a branch with B1-B7 (or the subset you finish) and a short report per item.

**Deploy notes:**
- After the build, the owner copies `utils/patches/patch_TOB.conf` to `server/assets/patches/`.
- No schema migrations expected. Flag one if you add any.
