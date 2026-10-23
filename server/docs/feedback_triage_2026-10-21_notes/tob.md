# #tob-client-feedback (8 threads, all read incl. replies)

### TGB Broken? (1562125280561209528, 2026-10-20, last 10-20)
- Summary: ToB players can't land group buffs on another player's group; /tgb says TGB is always on now. MGB etc fine.
- Category: bug (server) — HIGH impact for buffers.
- Status: still happening. No staff answer (only player Rekka's guess, which is right).
- Evidence: `utils/patches/patch_TOB.conf:101` OP_TGB=0x0000 (client never sends it) -> `Client::tgb` stays false (`zone/client.cpp:259,570`). `zone/spells.cpp:2173` requires `CastToClient()->TGB()` for group spells to use the target, else `spell_target = this`; `spells.cpp:2516` same for range. Rule Character:EnableTGB=true. Confidence high.
- Fix (C++, one line): `zone/client.h:934` -> `bool TGB() const { return tgb || ClientVersion() >= EQ::versions::ClientVersion::TOB; }`. Keeps RoF2 toggle; ToB matches Live "always on". Needs build + restart.
- Suggested reply: "Yep, confirmed. The new client doesn't tell the server TGB is on, so group buffs only hit your own group for now. Fix is lined up for the next update."

### PSA: don't use 'bebags' with the ToB client (1561821802018115707, 10-19)
- Summary: moving items out of bank with the bebags MQ script deletes them.
- Category: bug, duplicate of known ToB OP_MoveMultipleItems issue (no ToB decode).
- Status: Straps found the items and parceled them back in-thread; player thanked. Underlying bug still open.
- Evidence: `patch_TOB.conf:255` OP_MoveMultipleItems=0x6bf7 has no entry in tob_ops.h / tob.cpp; raw packet hits `Handle_OP_MoveMultipleItems` (`zone/client_packet.cpp:11898`). Shared-bank slots >=2 resolve to INVALID_INDEX via CalcSlotId and SwapItem treats that as destroy (`zone/inventory.cpp:1810`). High.
- Action: hand to ToB agent (in brief). Optional: close thread after fix.
- Suggested reply: "Thanks for the heads up. Until the next update, skip bebags (or any bulk-move script) on the new client and move stuff by hand."

### Minor Adv Loot Bug (1561495608106418188, 10-18, last 10-18)
- Summary: (a) giving a loot item to an out-of-zone char leaves the row stuck; only resolved by letting it roll. (b) buying stackables from a vendor fast leaves the quantity window stuck open.
- Category: bug x2 (server).
- Status: (a) Straps said "I'll patch that" — NOT fixed at HEAD. Commit e7483cb27 fixed Loot/Sell/Store refusals only. (b) no staff reply, open.
- Evidence (a): `zone/corpse_advloot.cpp:1016-1021` — out-of-zone target -> chat line + SendAdvLootClearPending only, no state re-send. High on path, medium on exact packet the client needs.
- Evidence (b): `Handle_OP_ShopPlayerBuy` (`zone/client_packet.cpp:15136+`) returns with no buy reply at :15237 (lore), :15305 (money), :15322/:15330 (no room). Sell path already sends a dummy reply (:15446-15456). Also: money is taken at :15289 before the room check and NOT refunded on :15322/:15330 (affects RoF2 too; stock upstream comment says so). Medium that missing reply = stuck window.
- Fix: both C++ → ToB agent brief.
- Suggested reply: "Both of these are on the list. The loot one's a bug on our side, and the vendor window one too. Should be sorted next update."

### E3Next work with it? (1561317389109305344, 10-18, last 10-19)
- Summary: E3N on the ToB MQ build; trade/rez window names changed. Rekka (community dev) shipped a ToB E3N build; Straps gave window details.
- Category: client/third-party tooling — answered.
- Status: resolved by community; nothing server side.
- Action: none (maybe pin Rekka's release link).
- Suggested reply: none (thread is healthy).

### Tradeskill from Depot not working (1561159759057526814, 10-17)
- Summary: crafting with depot components works in a world oven, not from a mixing bowl in a bag. Button greys, nothing happens.
- Category: bug (ToB decode). Likely affects ANY in-bag auto-combine from the recipe window, not just depot.
- Status: open, no staff reply.
- Evidence: `common/patches/tob.cpp:10078` sets `emu->combine.some_id = eq->some_id` but the decoder's own comment (:10071) says the container's id is in `eq->recipe_id` (+4) and is logged but unused. `GetTradeRecipe` (`zone/tradeskills.cpp:1569-1585`) needs the bag's item id (e.g. 17901 Mixing Bowl) when some_id != 0; it gets the wrong value -> "Unknown recipe" silent fail at `tradeskills.cpp:611-615`. Medium (no packet capture; logs rotated).
- Fix: `tob.cpp:10078` -> `emu->combine.some_id = eq->recipe_id ? eq->recipe_id : eq->some_id;` Confirm first with one bag combine + Netcode logging (raw line at :10075 should show 17901 in recipe_id).
- Suggested reply: "Thanks, reproduced on our end. It's the bag containers, not the depot. Fix is in the works; ovens/forges etc work fine in the meantime."

### Extra shared bank space eats items (1561145493172658267, 10-17, archived)
- Summary: items in ToB shared bank slots past the first 2 deleted on logout.
- Category: already fixed (partially).
- Status: Straps patched (41b75bcf7, live) + parceled items. Remaining gaps: (1) after a rejected move the client isn't resynced, so it shows the item in the phantom slot until relog (`zone/client_packet.cpp:11876` resync only when both slots valid); (2) bulk moves (OP_MoveMultipleItems) into those slots can still destroy; (3) server tells ToB "unlimited shared bank slots" (`tob.cpp:6839` entries[11] = -1) — setting it to 2 may hide the extra slots entirely (unverified). High on code paths.
- Action: hand gaps to ToB agent.
- Suggested reply: none (already handled). Optional: "Should be fixed now. Shout if anything else vanishes."

### Buy All AA doesn't work (1561110088473649192, 10-17, archived)
- Summary: Buy All in AA window buys 1 rank; Straps can't reproduce.
- Category: needs info / probably working as intended.
- Status: unresolved, Straps couldn't reproduce.
- Evidence: `zone/client_packet.cpp:2179-2201` loops up to 1000 ranks for action 11 and stops at the first rank that fails CanPurchase (level_req, points, expansion, prereq: `zone/aa.cpp:1730-1852`). Most likely the next rank needs a higher level or more points. Medium-high server logic is fine.
- Action: ask which AA + which character; check rank 2 level_req vs their level. If they're truly eligible, enable AA logging for that zone and have them retry.
- Suggested reply: "Which AA was it, and on which character? Buy All stops at the first rank you can't buy yet (level or points), so that might be it. If not, we'll dig in."

### Tradeskill combine messages displaying incorrectly (1561154458589593662, 10-17, archived)
- Summary: with items already on the cursor, a combine result shows the ToB "overflow" message.
- Category: client behavior / working as intended (cosmetic).
- Evidence: combine results go through SummonItem to cursor; with a busy cursor they queue as ItemPacketLimbo, which the ToB client calls overflow. Nothing lost. High.
- Action: none.
- Suggested reply: "That's just the new client's way of saying it went to your cursor queue. Nothing's lost."

## Top issues in this channel
1. TGB off for every ToB player (one-line fix).
2. Bag auto-combine broken (one-line decode fix, confirm with a log).
3. Item loss paths: bulk moves (bebags), shared bank phantom slots, vendor buy with full bags takes money.
4. Adv loot give-to-out-of-zone stuck row; vendor quantity window stuck.
5. Buy All AA — needs player info.
