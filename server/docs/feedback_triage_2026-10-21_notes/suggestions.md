# #suggestions-and-feedback triage (2026-09-21 .. 2026-10-21)

Read-only pass. No Discord writes, no DB writes. All SQL below is proposed, not applied; per policy it goes in with the next normal restart (no hotfix, no reload).

---

### Allow early learning spells (1561845247481221252, created 2026-10-19, last 2026-10-19)
- Summary: DemonMage wants to scribe spells before reaching the spell's level, so looted scrolls don't take up bag/bank space.
- Category: suggestion (client limitation as asked)
- Status: no staff reply yet.
- Evidence checked: server-side scribe handler `code/zone/client_process.cpp:1494-1519` checks only class/equipability (`Character:RestrictSpellScribing`) and scroll/spell match. It has **no level check**, so the "not high enough level" block comes from the client. Server `ScribeSpell()` (used by `#scribespell`, `code/zone/gm_commands/scribespell.cpp:65-74`) also has no level check, so a quest NPC could scribe an above-level scroll for you. Confidence: med (I didn't test client display of above-level spells in the book on RoF2/ToB).
- Feasibility: you can't make the client's own scribe accept it without a client patch. Workaround: a Guild Lobby "scribe" NPC script that takes a scroll hand-in, checks class, calls `$client->ScribeSpell(scroll_effect, $client->GetNextAvailableSpellBookSlot())` and eats the scroll. Players still can't mem it until they reach the level (memorize is client-gated plus the server `HasSpellScribed`).
- Classification: **needs design decision by owner** (QoL, not era-accurate; it also lets people pre-buy and pre-scribe the whole book). Effort: ~1-2h quest script plus a test on both RoF2 and ToB to check how the book shows above-level spells.
- Recommended action: owner decides. If yes, add the hand-in NPC script (quest only, no DB).
- Solo/policy flags: none.
- Suggested reply: "Client blocks scribing above your level on its own, but we might be able to do it through an NPC. Will take a look."

### Druid Spell - Sylvan Infusion (1561131522961182780, created 2026-10-17, last 2026-10-17)
- Summary: joe says that with the server cast-time cap, the L65 GoD heal Sylvan Infusion is much worse than the L64 Karana's Renewal, and asks for Infusion to be buffed.
- Category: suggestion (balance). The claim is accurate.
- Status: no staff reply.
- Evidence checked: `spells_new`. 3232 Karana's Renewal: 10.0s cast, 600 mana, effect 147 (percent heal) 75% capped at 4680. 4883 Sylvan Infusion: 3.75s cast, 607 mana, direct heal 1791-2441. `rule_values` `Ascendant:CapSpellCastTimeMS = 3010` (`code/zone/spells.cpp:382,400`) cuts both to about 3.0s, so Renewal does about 2x the healing for the same mana and time. The same thing happens for clerics: Complete Heal (id 13, 10s, 7500 HP, 400 mana) beats Holy Light 4880 (2625-3275, 650 mana), Pious Light 5265 and Ancient: Hallowed Light 6140 at 3s. Shaman Kragg's Mending 2435 (75%/1950 cap) is a smaller outlier. Confidence: high.
- Classification: **needs design decision by owner**. The cause is the global cast-time cap, so this is a class-wide and system-wide balance issue, not one spell. Options: (a) raise Sylvan Infusion's base/max (one spells_new row; quick, but the client spell file description won't match), (b) exempt percent heals / Complete Heal from the cap or give them their own cap (C++ rule change in `spells.cpp`), or (c) leave it, since big slow heals being king is a known server choice. Effort: (a) 10 min SQL plus restart; (b) ~1-2h code plus build.
- Example (a), if owner picks it: `UPDATE spells_new SET effect_base_value1=4200, max1=4800 WHERE id=4883;` rollback `UPDATE spells_new SET effect_base_value1=1791, max1=2441 WHERE id=4883;` (numbers are the owner's call; Druid Sylvan Light / Cleric equivalents would want the same treatment for consistency.)
- Solo/policy flags: none. Never hotfix: spell DB change waits for restart.
- Suggested reply: "Yeah, the cast time cap makes the big heals way better than the GoD ones. Looking at how we want to handle it across all the healers, not just druid."

### Quest Items Buffed (1560819322987876483, created 2026-10-17, last 2026-10-17)
- Summary: Utum says quest/class/chase items don't get the same spell dmg / heal amt / heroic stats as drops of similar difficulty, and asks for GoD chase quest items to be buffed (links the eqprogression GoD quest overview).
- Category: suggestion. The claim is accurate based on sampled data.
- Status: no staff reply.
- Evidence checked: sampled GoD quest rewards from `abysmal/#Marius_Alyonn.pl:44` (Dakkamor's Helm of the Divine 68855 and the other class helms 68860-68920). The Ascendant variant 768855 has heroics=1, healamt=0, spelldmg=0. GoD raid drops' Ascendant variants (txevu/qvic/vxed) such as 768818 Mask of the Sacred Evil, 768812, 768816, 768820 and 768814 carry healamt 5 / spelldmg 5 / heroics 10-14. So the variant generator gives quest items much weaker mod2s. Confidence: med (small sample; I didn't read the variant generator).
- Classification: **needs design decision by owner**. Possible fix: re-roll or raise mod2s on the Enhanced/Exalted/Ascendant variants of a whitelist of GoD quest rewards (class helms, BiC rewards, Soulwhisper 62581/762581, etc.), using raid-tier mod2s as the target. Effort: medium (~half day: build the item list from the eqprogression guide plus quest grep, then a bulk UPDATE on the 3xxxxx/5xxxxx/7xxxxx variants with a rollback snapshot). DB-only, waits for restart.
- Solo/policy flags: none. Helps solo players, since quest gear is more reachable than raid drops.
- Suggested reply: "Fair point, the quest stuff does come out weaker on the extra stats. Will look at bumping the GoD quest rewards."

### Taelosian Morphemic Staff (1560161102430601306, created 2026-10-15, last 2026-10-16)
- Summary: joe asks for the Taelosian Morphemic Staff (or a global faction tweak) to make the Yunjo Slave Resistance grind for Breakdown in Communication #3 bearable. Ali (Guide) suggested putting it on the Marks vendor; "we shall see".
- Category: suggestion
- Status: not done. The item exists but has no source.
- Evidence checked: `items` 66617 (and variants 366617/566617/766617) has factionmod1=1761 Nihil +700 and factionmod2=1770 Yunjo Slave Resistance +700, slots 26624 (primary/secondary/range), nodrop, lore, all classes. No merchantlist or recipe rows; lootdrop not checked beyond the item search (no loot hits). Server applies worn item faction bonuses (`code/zone/bonuses.cpp:458-490`, used in `Client::GetFactionLevel` `code/zone/client.cpp:8298`). The BiC Riwwi NPCs check con via `$faction` / `GetFaction()` (`riwwi/#Namosa_Bubundu.pl:4,12`, `riwwi/Kayin_Wanjala.lua:6,18`), which goes through GetFactionLevel, so the staff would count while equipped. Confidence: high.
- Classification: **quick win** (DB only), with an owner pick of price/vendor. The Marks of Ascendance vendor is Exarch_Valeth 344046 in guildlobby (alt_currency 3; current list slots 1-12, costs 1-20 marks).
- Proposed SQL: `INSERT INTO merchantlist (merchantid, slot, item, faction_required, level_required, alt_currency_cost, min_expansion, max_expansion) VALUES (344046, 13, 66617, -1100, 0, 5, -1, -1);` rollback `DELETE FROM merchantlist WHERE merchantid=344046 AND item=66617;` (check merchantlist column names/defaults with DESCRIBE before applying; cost is owner's call.) Effort: 10 min plus restart (or the normal merchant reload during maintenance).
- Solo/policy flags: helps solo players (the faction grind is a 3-box ask per the OP).
- Suggested reply: "Going to toss it on the marks vendor with an upcoming update."

### Coral Hilted Shortsword (1554170946003411024, created 2026-09-28, last 2026-09-28)
- Summary: huan7845 says the bard swarm pet from the Coral Hilted Shortsword proc mezzes the mob, which ends the pet's fight; the mob wakes up and regens. Asks to remove mez songs from the pet.
- Category: **bug** (reported as a suggestion). The pet's spell list makes it fight against its owner.
- Status: no staff reply. Still present.
- Evidence checked: item 24594/724594 proceffect 3639 Call of Rizlona, which summons swarm pet `SwarmPetBard` (npc_types 654, L60, npc_spells_id 11 "Default Bard List"). npc_spells_entries for list 11 at L60 include Sionachie's Dreams 868, Dreams of Ayonae 1100, Ancient: Lullaby of Shadow 1197, Song of Twilight 1753, Song of Midnight 1756 (fear), Occlusion of Sound 1451 (memblur), Brusco's Bombastic Bellow 1747, Ervaj's 2936, Angstlich's 42620, plus Solon's Bewitching Bravura 750 (charm). Most are tagged type 1 (nuke), so the pet casts them in combat. Only spell 3639 points at SwarmPetBard. List 11 is shared by 9 npc_types, so don't edit the list itself. Confidence: high.
- Classification: **quick win**. Effort: 5 min SQL (simplest), or ~30 min for a dedicated pet list that keeps the DoTs/buffs.
- Proposed fix (simplest, melee-only pet): `UPDATE npc_types SET npc_spells_id=0 WHERE id=654;` rollback `UPDATE npc_types SET npc_spells_id=11 WHERE id=654;`. Better option: a new npc_spells list (next id 3760) with only DoT songs (742 Denon's Desperate Dirge, 743/744/3567 Tuyen's chants, 1764) and the group buffs (709, 714, 2605, 2610), and point 654 at it.
- Solo/policy flags: none.
- Suggested reply: "Yeah, that pet shouldn't be mezzing what you're fighting. Fix goes in with the next update."

### Progression Ideas (1550884807779688578, created 2026-09-19, last 2026-10-11)
- Summary: BinHouse5 posts long-form feedback. Before 09-21: universal "omnipotent" tomes off raid lockouts, and content locks. Ali answered both on 09-19: no universal tomes or tome swapping, and no expansion locks (self-impose if you want). After 09-21: (1) asks for access to pre-revamp zone versions for self-locked progression; (2) PoHate v2 T1 armor drop rate feels low vs PoFear; (3) where to farm books at 60 in Velious; (4) Ascendant item algorithm gives low-level gear (e.g. Ascendant Warrens Dusty Choker) mod2s (Spell Dmg 5 / Heal Amt 5 / Shielding / Avoidance) so strong that people keep it.
- Category: suggestion / general feedback. Mostly discussion, with no concrete ask beyond (1).
- Status: Ali/Icydoc answered (1) partly: only Sleeper's Tomb is dual-state, as a special exception. (2)-(4) have no staff reply.
- Evidence checked: thread messages only. I didn't run DB checks on Hate drop rates or the mod2 roller. Confidence: med.
- Classification: (1) **not recommended**: large effort (dual zone versions, spawns, content flags), and it cuts against the revamp direction. (2) **needs design decision** (look at the hateplaneb tier-1 armor lootdrop chances vs pofear; ~1h to analyse). (3) a player question; a guide/community can answer. (4) **needs design decision**: a system-wide change to the Ascendant variant generator (flat mod2s regardless of item level). Large effort and touches every item, so low priority. It also links to the Quest Items thread: the variant generator's mod2s don't scale with content tier in either direction.
- Recommended action: none needed now. Optionally a short reply on (1).
- Solo/policy flags: none.
- Suggested reply: none (staff already engaged; the latest post is a progress update, not an ask).

### Boost Krasnok's Private Reserve to 100% drop rate (1554937701646467183, created 2026-09-30, last 2026-09-30)
- Summary: asks for Krasnok's Private Reserve (one of four Hate's Fury boss event keys) to drop more often from the Drunken Buccaneer (was ~30-35%). Straps replied "It now drops significantly more commonly".
- Category: already fixed
- Status: fixed by staff in-thread; verified.
- Evidence checked: npc 228111 the_Drunken_Buccaneer (spawned by `hatesfury/#drunk_counter.pl:11`) uses loottable 14394, lootdrop 22923 "the_Drunken_Buccaneer_misc", item 56007 chance **90%**, probability 100. The old 35% row (lootdrop 11584, npc 6299) isn't used by the live NPC. Confidence: high.
- Recommended action: close (no further work).
- Suggested reply: none (Straps already confirmed; thread can be closed).

### Liminal in PoTranq (1518283477991292928, created 2026-06-21, last 2026-10-05; messages since 09-21 only)
- Summary: Icydoc bumped on 10-05. Ali pinged Straps: with PoP done, put Liminal by the Valor tunnel in Tranq, and maybe near the Dulak docks too.
- Category: suggestion
- Status: not done. Liminal (npc 59160) spawns in 23 zones (guildlobby hub plus discovery points); none in potranquility, dulak or hatesfury.
- Evidence checked: `quests/global/Liminal.pl` (discovery is fully generic: proximity in any non-lobby zone saves the spawn's own XYZ as the travel point, so a new zone only needs a spawn row). PoTranq door ports: TACTICPORT500 (-19.7,-152.6,-618.3), VALORPORT500 (20.0,-163.3,-618.3), HONORPORT500, SOLROPORT500 around (0..31, -200..-232, -618). Confidence: high.
- Classification: **quick win** (DB only). Effort: ~15 min, plus an in-game /loc to pick the exact spot (the coordinates above are the port doors; Liminal should stand a bit up the tunnel, e.g. near 0,-110,-618, but verify). Add a spawngroup + spawnentry(59160) + spawn2 in potranquility (and optionally dulak near the docks, after a /loc). Rollback: delete those three rows by id. Takes effect on the next normal restart.
- Solo/policy flags: none.
- Suggested reply: "Good call, will get one added in Tranq with an upcoming update."

### Tome icons (1508509434937081978, created 2026-05-25, last 2026-10-16; messages since 09-21 only)
- Summary: Jubi asked on 10-16 to revisit class-specific icons for AA tomes.
- Category: suggestion (needs client assets)
- Status: answered back in May: staff won't build it themselves, but will pass on tasteful icons if someone makes them (Ali/Beorc). No new icons have been posted since.
- Evidence checked: the 229 Tome of Advancement items (121620-121848) all use icon 1497 and have 52 distinct class masks (many tomes are multi-class, e.g. 448 = MNK/BRD/ROG, 33089). Illegible tomes 121571-121618 already differ by tier (icons 3370/3290/3629). Names are already up to 61 chars, so class tags in the name won't fit under the 64-char limit. Confidence: high.
- Classification: **needs design decision by owner** (and art). A per-class icon only works cleanly for single-class tomes, and it needs a client asset patch for both RoF2 and ToB. A cheaper option: give single-class tomes existing stock icons per archetype (melee/priest/caster/hybrid) without new art. That's DB only (`items.icon`), ~1h, but the result is less clear. Low player impact.
- Recommended action: none beyond restating the earlier answer.
- Suggested reply: "Still open to it if someone makes the icons, just not something we're making ourselves right now."

### Bank box for MOA (1476366908642164806, created 2026-02-25, last 2026-10-20; messages since 09-21 only)
- Summary: Icydoc bumped twice (10-05, 10-20). Ali: Beorc has a patch ready, but it waits until the ToB bank slot problems are fully fixed. Ali also asked people not to bump.
- Category: suggestion (planned, blocked on ToB bank work)
- Status: answered by staff in-thread (10-20). No action needed from this triage.
- Evidence checked: thread only. I didn't verify the patch's contents. Confidence: med.
- Classification: needs owner timing (already planned). It depends on the ToB bank-slot fixes landing first.
- Suggested reply: none (Ali answered 10-20).

---

## Top issues in this channel (player impact first)
1. **Coral Hilted Shortsword bard swarm pet mezzes/fears its own target.** A real bug. 5-minute DB fix (npc 654 npc_spells_id), or a dedicated list.
2. **Heal balance under the 3s cast-time cap.** Old 10s heals (Karana's Renewal, Complete Heal) beat the GoD heals by ~2x for druids and clerics. Owner design call: buff the new spells or exempt/limit long heals in the cap.
3. **GoD quest reward variants get weak mod2s** (Ascendant class helm heroics 1 vs raid drops' 5/5/10+). Owner decision, about half a day of DB work. It ties into the "Ascendant generator doesn't scale with tier" feedback in Progression Ideas.
4. **Taelosian Morphemic Staff on the Marks vendor.** Quick DB add. The faction bonus works server-side for the BiC Riwwi checks.
5. **Liminal in PoTranq (+ Dulak).** Quick DB spawn add. Needs an in-game /loc.
6. Allow early spell scribing. Possible through an NPC script (server has no level check); owner decision.
7. Krasnok's Private Reserve: already fixed (90%), close. Bank boxes: planned, blocked on ToB. Tome icons: waiting on community art.
