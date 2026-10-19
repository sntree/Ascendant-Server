# Gates of Discord (GoD, expansion 7) — Ascendant Release Plan

Prepared 2026-09-26 against the live `peq` DB and `/opt/akk-stack/server` tree. Read-only analysis; nothing was changed. Numbers are from the queries listed at the end. Anything from EQ history rather than this DB is marked **[memory]**.


> **Review notes (2026-09-26, spot-verified against the live DB after the agent run)**
> - Confirmed exactly: 713 raid_target / 0 rare_spawn in GoD NPC range; 5->7 turns OFF 1,246 spawn2 / 187 doors / 10 zone_points; droga v0/v1 = 466/577, nurga 92/243, global static instances 7 (nurga v1) and 8 (droga v1); titles max id 426; items 4042-4050 free; spawn2/spawngroup max 3266248/3289046; active players capped at 65.
> - **Correction to finding 3 / 11.2 (Txevu flag):** Cipher of Txevu 60254 IS obtainable: `abysmal/Brevik_Kalaner.lua:102` gives it (and sets `god_txevu_access`) once the `txevu_items` bucket has Sliver 60176 + Fragment 60252 + Splinter 60253. Fragment comes from `uqua/#Specter_of_Barxt.lua:12`, Splinter from `inktuta/#An_Ancient_Sentinel.lua:7`. Only the source of Sliver **60176** (not the Qvic Sliver 60174) was not found — GM-test this chain instead of adding a new drop.
> - Finding 4 (travel) stands: the only zone_point into natimbi is from qinimi (itself a GoD zone); nothing enters Abysmal/Natimbi from outside GoD.
> - "103 GoD abilities" = 408 `aa_ranks` rows at expansion 7 (ranks vs abilities) — consistent.
> - **2026-09-26: expansion 6 (LDoN) applied separately** (`server/sql/2026_09_26_ldon_expansion6_gate.sql`, live at next restart; owner accepted PEQ defaults: Vox/Nagafen lose the 5% drops, Magus Zeir sells Natimbi spells). Everything in Section 2 keyed to max_expansion 5 / min_expansion 6 (Droga/Nurga flip, v0 layouts, 13/5 lootdrops, Magus Zeir) is therefore **no longer part of the GoD switch**. The GoD gate script becomes rule **6 -> 7**; re-measure Section 2 for min_expansion 7 / max_expansion 6 only (expected: 25 epic-1.5 spawns, 42 tome merchant rows, PoK Gunthak stone doors, 2 sirens v1 spawns).

---

## 0. Headline findings (read these before anything else)

1. **The 5 -> 7 rule bump is not "free" like 4 -> 5 was.** It turns OFF 1,246 `spawn2` rows, 187 `doors`, 10 `zone_points` and 3 `ground_spawns` (all `max_expansion = 5`, all in **version 0** of droga / nurga / griegsend / citymist / sirens), and it switches **public Droga and Nurga to their LDoN-era revamped version 1** via the static-instance router (`instance_list` ids 7 and 8, `common/content/world_content_service.cpp:293` `FindZone`, called from `world/client.cpp:816` and `zone/zoning.cpp:763`). Griegsend / City of Mist / Sirens are already served by their v1 static instances (ids 9-11) today, so nothing visible changes there. Droga/Nurga is a real player-visible change (old 466/92-spawn layouts -> revamped 577/243-spawn layouts). Decide whether that is wanted (Section 2).
2. **PEQ GoD data ships 713 `raid_target` flags and 0 `rare_spawn` flags.** Whole raid zones are flagged wholesale (uqua 89/89 NPCs, txevu 185/185, ikkinz 260/422, inktuta 69/85, tacvi 44/62). With Ascendant's hooks that means every trash mob in Txevu would get 1-3 raid-pool bonus items (`global_npc.pl:335-341`), use the raid scaling profile, and ~80 trigger/controller NPCs (traps, `_`, `#Main_Spawner`, `lockout_uqua`, `lockout_ikkinz`, `Champ_Event`, `ZMTZ_Dummy`, `#Jerimo_Jiao` who *is the Uqua expedition giver*) would be depopped in Normal expeditions (`global_npc.pl:122-136`). **583 flags must be removed, 130 kept** (Section 4).
3. **Progression gaps in the DB:** the Txevu flag item **60254 "Cipher of Txevu" has no drop and no quest giver anywhere** (`qvic/player.lua:14` is the only reference) so Txevu (`zone.flag_needed = 1`) is GM-only as shipped. Kod'Taz and Qvic flags are implemented (Section 1.3).
4. **There is no implemented route into Abysmal Sea / Natimbi.** No `zone_points` target 279/280, no boat NPC in nedaria/abysmal, `nedaria/Magus_Wenla.lua` only goes to Butcherblock/Commonlands/Everfrost/NRo/SRo, `natimbi/Magus_Releua.lua` says "cannot reach any available destinations". The only things that turn on are the wizard/druid Natimbi port spells sold by Magus Zeir (22114, ecommons, `merchantlist.min_expansion = 6`, an LDoN leftover). A transport decision is required (Section 11.1).
5. **GoD AAs are probably already purchasable.** In this DB `aa_ranks.expansion` uses the *original* PEQ numbering for PEQ rows (3 = Luclin, 4 = PoP, **7 = GoD** e.g. Persistent Casting, 8 = OoW) and the custom 10-21 numbering only for the re-tagged "universal" copies. The 40 GoD AAs mapped in `aa_custom_mapping` (Feral Swipe, Doppelganger, Sinister Strikes, Exquisite Benediction, Coat of Thistles, ...) sit at `expansion = 7`, which already passes the trainer filter `ur.expansion <= 16` (`ascendant_insight_trainer.pl:284-286`). Their 40 illegible tome IDs (121571-121618) are **not** in `ascendant_tome_translator.pl` (0 matches), so they never drop. **Do NOT bump `$CURRENT_MAX_EXPANSION` to 17** — expansion 17 here is 172 level-70 post-GoD abilities (Twinheal, Spires, Glyph Spray...). Section 10.
6. **GoD gear does not need a core-stat multiplier boost.** Auto-T3 already exceeds PoP by block (god2 T3 hp 363 vs pop3 243; god4 409 vs pop5 363). It only needs the hand-tuned heroic/secondary pass (heroics avg 3-4 vs PoP 26-38). Section 7.
7. GoD spell scrolls (item range 59846-64579) are already on ungated PoK/EC merchants (220 in poknowledge, `min_expansion = -1`); 35 scrolls are on no merchant. Nothing to switch on; verify the 35 (Section 11.6).
8. Player base: 754 chars active in 30 days, 176 at the cap of 65 (`Character:MaxLevel = 65`), none above. GoD raid mobs are L70-80 with 1-3M HP (Tacvi finals 2.0-2.9M, Inktu`Ta guardians 11M). Scaling and gear decisions below assume a 65 cap.

---

## 1. Inventory

### 1.1 Expansion-7 zones (`zone` table, 23 rows / 22 zones)

| id | short | long name | min_lvl | flag_needed | min_status | safe (x,y,z) | versions | notes |
|---|---|---|---|---|---|---|---|---|
| 279 | abysmal | The Abysmal Sea | 0 | | 0 | 0,-199,140 | 0 | Hub (272 NPCs, 241 quest files, tradeskills). safe_z 140 = ship deck, plausible |
| 280 | natimbi | Natimbi, the Broken Shores | 0 | | 0 | -1557,-853,239 | 0 | Entry zone; Cragbeast Queen raid |
| 281 | qinimi | Qinimi, Court of Nihilia | 50 | | 0 | -1053,438,-16 | 0 | |
| 282 | riwwi | Riwwi, Coliseum of Games | 50 | | 0 | 454,-650,35 | 0 | |
| 283 | barindu | Barindu, Hanging Gardens | 50 | | 0 | 590,-1457,-123 | 0 | Sewer/Vxed/Tipt DZ hub |
| 284 | ferubi | Ferubi, Forgotten Temple | 55 | | 0 | 1485,596,111.8 | 0 | |
| 285-288 | snpool/snlair/snplant/sncrematory | Sewers of Nihilia x4 | 0 | | 0 | ~ | snplant has v0,1,10 | Instance-only (created by `barindu/#High_Priest_Diru_Riwirn.lua:32`, `#Gamesh.lua:32`) |
| 289 | tipt | Tipt, Treacherous Crags | 0 | | 0 | -448,-2374,12 | 0 | Instance-only (`barindu/#Apprentice_Udranda.lua`) |
| 290 | vxed | Vxed, the Crumbling Caverns | 60 | | 0 | -427,-3552,14 | 0,1 | v0 = 6-man, v1 = 6-24 (Udranda.lua:24-25); v1 adds #Vuranish_Xxuro |
| 291 | yxtta | Yxtta, Pulpit of Exiles | 65 | | 0 | 1235,1300,-348 | 0 | Uqua giver #Jerimo_Jiao here |
| 292 | uqua | Uqua, the Ocean God Chantry | 0 | | 0 | -17,-7,-26 | 0 | Raid DZ (`yxtta/#Jerimo_Jiao.lua`) |
| 293 | kodtaz | Kod'Taz, Broken Trial Grounds | 65 | **1** | 0 | -1475,1548,-302.12 | 0 | Ikkinz trial givers here |
| 294 | ikkinz | Ikkinz, Chambers of Transcendence | 0 | | 0 | -157,23,-2 | 0-6 | v0-2 group trials, v3-6 raid trials (`lua_modules/constants/instance_versions.lua:30-36`) |
| 295 | qvic | Qvic, Prayer Grounds of Calling | 65 | **1** | 0 | -2515,767,-647 | 0 | Raid zone (public) |
| 296 | inktuta | Inktu'Ta, the Unmasked Chapel | 0 | | 0 | 0,65,-2 | 0 | Raid DZ (`qvic/Aganetti_the_Keeper.lua`), own 108h lockouts (`inktuta/zone_status.lua`) |
| 297 | txevu | Txevu, Lair of the Elite | 0 | **1** | 0 | -332,-1,-420 | 0 | Raid zone (public), flag item missing |
| 298 | tacvi | Tacvi, The Broken Temple | 0 | | 0 | 4,9,-8 | 0 | Raid DZ (`txevu/player.lua:27`, `txevu/Hamari_Nedu.lua:36`), 8 events x 108h lockouts (`tacvi/zone_status.lua:57-77`) |
| 299 | qvicb | Qvic, the Hidden Vault | 0 | | **80** | 0,0,-6.25 | 0 | GM-only, 0 quest files, leave |
| 998 | fhalls | The Forgotten Halls | 0 | | **80** | -74,-843,-11 | 0 | GM-only (L2-4 NPCs), leave |

No GoD zone row has `min_expansion`/`max_expansion`/`content_flags` set; all 4,852 v0 spawn2 rows in GoD zones are `-1/-1` (0 rows at min_expansion 7, 1 row at >7). Entry is gated purely by `zone.expansion = 7` vs `Expansion:CurrentExpansion` (`zone/zoning.cpp:349-372`; `#gm on` bypasses, and `GM:MinStatusToZoneAnywhere` bypasses `CanEnterZone` entirely, `zoning.cpp:1420`).

### 1.2 Level profile per zone (spawned NPCs, level > 1; L99 = trigger/invisible NPCs)

| zone(ver) | spawn pts | NPCs | lvl min/avg/max | max hp | classification |
|---|---|---|---|---|---|
| abysmal | 282 | 271 | 40 / 68.9 / 99 | 70,000 | hub |
| natimbi | 351 | 82 | 43 / 47.4 / 99 | 501,148 (Cragbeast Queen) | group + 1 raid event |
| qinimi | 184 | 114 | 35 / 50.7 / 99 | 571,000 (Mass of Stone) | group + raid events |
| riwwi | 163 | 87 | 40 / 53.1 / 99 | 9,999,999 (placeholder) | group + 1 raid event |
| barindu | 217 | 144 | 30 / 55.5 / 99 | 250,000 | group + raid events |
| ferubi | 244 | 64 | 60 / 63.6 / 71 | 1,050,000 (Zun`Muram Votal) | group + raid events |
| snpool/snlair/snplant/sncrematory | 176/207/143/157 | 82/102/63/82 | 53-65 | 20-37k | group DZ |
| tipt | 54 | 35 | 60 / 65.4 / 99 | 60,000 | group DZ |
| vxed v0 / v1 | 168 / 168 | 66 / 68 | 64 / 65.3 / 99 | 55,754 / 310,400 | group DZ / mini-raid DZ |
| yxtta | 336 | 127 | 62 / 66.2 / 99 | 720,000 | group + Primal raid event |
| kodtaz | 447 | 203 | 64 / 66.2 / 99 | 2,147,483,647 (pile-of-bones placeholder) | group + raid events |
| ikkinz v0/v1/v2 | 57/56/60 | 49/51/49 | 61-72 | 75k / 750k / 75k | group trials |
| ikkinz v3/v4/v5/v6 | 75/120/105/24 | 48/81/79/14 | 61-72 | 600k/750k/526k/569k | raid trials |
| uqua | 79 | 47 | 63 / 73 / 99 | 400,000 (+1.5M scripted) | raid |
| qvic | 239 | 142 | 65 / 66.9 / 74 | 2,900,000 | raid (public) |
| inktuta | 50 | 22 | 65 / 67.6 / 72 | 11,000,000 | raid DZ |
| txevu | 324 | 130 | 43 / 68.0 / 80 | 2,105,145 | raid (public) |
| tacvi | 62 NPC types | — | up to 80 | 2,858,600 | raid DZ |

### 1.3 Progression / flagging as implemented here

- **Kod'Taz (293, flag_needed):** `natimbi/player.pl:1-9` grants zone flag 293 on clicking door 1 if qglobals `god_kodtaz_access`, `god_vxed_access`, `god_tipt_access` are all set. Those come from the Barindu chain: sewers (`barindu/#High_Priest_Diru_Riwirn.lua:80` sets vxed access), Vxed (`vxed/#Stonespiritist_Ekikoa.lua:19` tipt access), Tipt (`tipt/#Master_Stonespiritist_Okkanu.lua:19` kodtaz access), plus `barindu/Scribe_Gurru.lua:57-60`. Temporary "T" flags via data buckets `<charid>-god_vxed` / `-god_tipt`.
- **Ikkinz group trials 1-3:** `kodtaz/Gazak_Klelkek.lua`, `Maroley_Nazuey.lua`, `Kenra_Kalekkio.lua` create expeditions into ikkinz versions 0/1/2 (1h replay lockout, `Kenra_Kalekkio.lua:96`). Zoners `kodtaz/#Zoner1-3.lua`.
- **Ikkinz raid trials 1-4:** ikkinz versions 3-6 (`ikkinz/player.lua:20-23`); door keys 60225/60227/60233/60240/60249. Requester NPC not traced here (see open questions).
- **Qvic (295, flag_needed):** `kodtaz/Tublik_Narwethar.pl:180-185` — hand in 60174 (Sliver, drops from 294621 `a_pile_of_bones`, Ikkinz raid trial 4) while holding 60252 (Fragment, from `abysmal/Brevik_Kalaner.lua`) -> `god_qvic_access = 1` + zone flag 295. `qvic/player.lua:5` upgrades to 2 on entry; `natimbi/#Madronoa.lua:15` ports flagged players to Qvic (GM bypass at Admin > 80 + `#gm on`).
- **Inktu`Ta:** `qvic/Aganetti_the_Keeper.lua:33` creates the DZ once Cynosure Kvanjji (295140/295149) is dead; 2h + 22/44/59/107h event lockouts (`uqua/lockout_uqua.lua`-style, `inktuta/zone_status.lua`).
- **Txevu (297, flag_needed):** `qvic/player.lua:13-16` sets flag 297 on door 2 if the player has **60254 Cipher of Txevu — which nothing drops or gives**. GM raid pull-through exists (`qvic/player.lua:19-31`).
- **Tacvi:** `txevu/player.lua:23-30` — Zun Statue (door 23) with 64034 Zun'Muram's Signet of Command on cursor (drops from 297150 #Zun`Muram_Tkarish_Zyk) creates the Tacvi DZ (54-man, replay lockouts).
- **Uqua:** `yxtta/#Jerimo_Jiao.lua:24` creates the DZ.
- **Interaction with Ascendant expeditions:** `plugin::CreateExpedition` (`ascendant_expeditions.pl:39-192`) makes a DZ of the *current* zone; entering an instance still runs `CanEnterZone` (`zoning.cpp:1415-1476`) so `flag_needed` zones (kodtaz/qvic/txevu) require the zone flag even inside a Tyrael expedition. Zones with their own DZ/lockout scripts (uqua, ikkinz, inktuta, tacvi, sewers, vxed, tipt) must be kept **out** of Tyrael, exactly like potimea/potimeb (`ascendant_expedition_config.pl:270-271`).

---

## 2. What the 5 -> 7 bump switches ON and OFF outside GoD zones

Measured per table (only values 6/7 for min, 5/6 for max are affected; `lootdrop_entries`, `object`, `traps`, `start_zones`, `fishing`, `forage`, `tradeskill_recipe`, `global_loot`: **0 rows**; `loottable_entries`/`spawn_conditions`/`npc_spells` have no expansion columns).

**Turns ON (min_expansion 6 or 7):**
- `spawn2`: 25 rows in classic zones — epic 1.5 pre-quest NPCs **[memory: PEQ tags them 7]**: southkarana 4 (#Sir_Telian_Mottsworth 14170, #Rendolf_Deathbringer, #Finnelzi_Springworthy, #Tehana_Elsent), northkarana 3 (Gorri_Fallstine, #Sir_Gerwin_Thunderblade, a_dire_griffon), qey2hh1 3 (#Yuanda, #Sir_Arlindo_Goldheart, Mystik), lakerathe 3 (a_fallen_student, #Kimm_McShannel, #Egonis), commonlands 2, qrg 2, poknowledge 2 (Tenada_Jeried 202047, Vivian_the_True 202295 L75, max_expansion 99), butcher/ecommons/kaladima/kaladimb/kithicor/qcat 1 each.
- `merchantlist`: 42 rows / 6 merchants at 7 — PoK tome vendors Blane_Darkblade 202156 (ROG), Beorobin_Amondson 202158 (MNK), Kurlond_Axebringer 202288 (BER, 21 rows), Heldin_Swordbreaker 202310 (WAR), Zeflmin_Werlikanin 345004 guildhall (Etched Marble of Barindu); plus **11 rows at 6**: Magus_Zeir 22114 (ecommons -186,-1616) selling Natimbi Gate/Translocate/Portal/Circle/Ring (59878-59882). Needs `#reload merchants global`.
- `doors`: 3 — PoK Gunthak stone 9761 `POKGTHPORT500` (1230,-760,-160.81) + its base 12287, and gunthak 12119 `POKTELE` back to PoK. (LoY analysis already flagged these.)
- `zone`: droga v1 and nurga v1 rows (`min_expansion 6`) become valid.

**Turns OFF (max_expansion 5 or 6):**
- `spawn2`: **1,246 rows**: droga v0 466 (465 at 0..5 + 1 at 4..5), griegsend v0 377, sirens v0 157, citymist v0 152, nurga v0 92, sirens **v1** 2 (24575/24576 an_enthralled_bulthar + #Ulth_the_Enraged 125060/125097, max_expansion 6 — not rare_spawn, no pool impact).
- `doors`: 187 (griegsend v0 104, citymist v0 42, droga v0 23, sirens v0 14, nurga v0 4). `zone_points`: 10 (droga 3, nurga 3, citymist 2, sirens 2). `ground_spawns`: 3 (citymist 2, nurga 1). `zone`: droga v0 row (`max_expansion 5`).
- **Net effect** via the static-instance router (`world_content_service.cpp:293-330`, `instance_list` 7 nurga v1, 8 droga v1, 9 griegsend v1, 10 citymist v1, 11 sirens v1): griegsend/citymist/sirens public traffic is *already* routed to v1 today (their v0 zone rows are 99..5 / 99..99 / 0..7-but-no-v0-instance), and expeditions already use v1 (`ascendant_expedition_config.pl:78-82,131-135,214-218`). **Droga and Nurga flip from old v0 (466/92 spawns) to revamped v1 (577/243 spawns, LDoN 2003 layouts).** No Ascendant plugin references droga/nurga zone ids as versions, but check `ldon_zone_ids`/`kunark_zone_ids` (81, 107) still map — they do by zoneidnumber, unaffected.
- The `rathunt.pl:241` location list already includes abysmal/natimbi/yxtta/barindu/qinimi/riwwi; those become reachable/valid after release (currently they resolve to unreachable zones).

Nothing else is hidden in non-GoD zones the way Cazic-Thule was for LoY (0 spawn2 rows at min_expansion 6, 25 at 7, all listed above).

---

## 3. Named tiers (`rare_spawn`) — data-driven, same method as LoY rev 3

Filters: not raid_target, own loottable, non-merchant class, "named-style" name (not `a_`/`an_`), GoD group zones. Counts are distinct NPC ids.

**Tier A — shared-slot rares (chance <= 50 in a multi-entry spawngroup): 98**
- barindu 15: 283018 #Mastruq_Utk_Hykat (L67 100k 12%), 283159 #Kyv_Rux_Vhedt (10%), 283160 #Ra`tuk_Kjyt_Gykk (5%), 283161 #Frenzied_Noc_Corpsethrasher (5%), 283163 #Aneuk_Overseer (5%); **REVIEW** the 10 lower-tier 50%/33% L43-47 4,100-hp entries (283003/283061/283084/283087/283103/283105/283126/283135/283136/283141 — Kunimi_Falade, Vasha_Firaji, Proteri_Amari, Njanu_Dinari, Rasnil_Sanusel): trash-sized HP; LoY precedent kept 50% slots (Ralo the Black) but these look like flavour nameds — suggest opt-in block.
- ferubi 7: 284044, 284085, 284086, 284099, 284100, 284101, 284102 (#Rav_Hunter 3%).
- kodtaz 26: 293105, 293107 (Forgotten_Martyr), 293139, 293143 (Forlorn_Relic_Devotee, multi-slot 5-25%), 293181, 293183, 293188, 293189-293200 (#Coerced_Tiv_Chanter ... #Pixtt_Airstrangler), 293203, 293204, 293206, 293208, 293209, 293234, 293235.
- natimbi 4: 280075 #Kyv_Heartpiercer, 280078 #Vimidon, 280079 #Duara_Yuenki, 280081 #Tumia_Lehio.
- qinimi 6: 281103, 281132, 281133 (#Rav_Krixt_Tekrik L65 50k), 281134, 281135, 281137 #Rker_the_Penmaster.
- qvic 9: 295062 Hexxt_Ilk_Klokk (L69 105k 20%), 295119, 295121, 295128, 295139, 295141, 295142, 295143, 295144.
- riwwi 9: 282094 Night_Watchman_Fxi, 282096, 282106, 282107, 282110, 282111, 282112, 282113, 282114.
- sewers 12: snpool 285081-285083; snlair 286100-286102; snplant 287141-287143; sncrematory 288078-288080.
- vxed 6: 290057 #Chavoor_the_Trickster, 290058, 290060 #Kyv_Hunter, 290061, 290063, 290064.
- yxtta 4: 291093 #Pixtt_Souleater, 291121 #Hexxt_Huntmaster, 291122 #Pixtt_Flamedirge, 291139 #Pixtt_Ravager (all ~105k, 5%).

**Tier B — static 100% single-entry nameds with own loottable (hp >= 20k): 7**
279002 Galdorin_Visigothe (abysmal L70 70k — **REVIEW**, hub quest NPC?), 284074 #Smith_Rondo (ferubi L70 100k, Vxed access event), 281030 Mass_of_Stone (qinimi L65 571k — **REVIEW: raid-sized static; [memory] Qinimi "Mass of Stone" is a scripted encounter**), 295064 Averixx_Quimeri (qvic), 289000 #Kyv_Heartstriker_Jhiru (tipt event), 291071 Stoic_Guardian, 291073 Vrex_High_Priest (yxtta).

**Tier C — script-spawned (no spawnentry, own loottable, hp >= 20k): 29**
280069 Reborn; 281119 #Executioner_Hexxt_Xocik, 281123 Pixtt_Tixxrt_Kvrok, 281125 Kabeka_Kret (L65 300k — **REVIEW**, raid-sized); 282071, 282072, 282076 #Kizki_the_Slayer (105k), 282077, 282080, 282083; 283046/283154 Construct_of_War; 284039, 284077 SpearGroup (**exclude**, controller), 284088-284091 Pxet_Elite_*, 284097 Smith_Rondo, 284104, 284105; 287145 #Ancient_Kayserops; 289033 #Garjah_Zotaki; 290069 Blackfall_Borer; 293185, 293216, 293219, 293220 ##Pixtt_Priest_Summoner, 293233 #Ikaav_Spiritstealer.

**Demote raid_target -> rare_spawn ("glorified nameds" / group-instance bosses): 11**
280090 #Ritual_Conduit and 280091 #Noc_Juggernaut (druid epic 1.5, `natimbi/player.pl:14-16`), 283047 Aneuk_Controller, 283102 A_Kyv_Runner (37k), 284093-284096 Master`s_Apprentice (130k adds), 290072 #Vuranish_Xxuro (vxed v1 6-24-man DZ boss — **REVIEW**; keeping raid would depop him only if vxed had a raid tier, which it will not), 294138/294629 #Malevolent_Priest (750k but ikkinz v1 = **group** trial 2), 296013 #Nijacka_the_Scourge (9% roaming slot in inktuta).

Expected result: **~145 rare_spawn** (98 + 7 + 29 + 11) before review edits; Tier A + demotions have several REVIEW items.

**Trigger / controller NPCs to strip `raid_target` from (would break events in Normal expeditions or flood raid loot): 80 identified** (bodytype 11/33/34/67, hp <= 200 or 2^31-1, level <= 10, name patterns): uqua 292004 #Main_Spawner, 292024 Summoning_Altar, 292028 #Aura_of_Destruction, 292040 #a_bubble, 292041 #floating_foam, 292042/292051-292061 #Trap_Uqua_1-8 & #Gas_Chamber_1-2, 292079 lockout_uqua, 292080-292082 cheater triggers, 292084 a_jet_flame; ikkinz 294141/294597/294614/294619 #Trigger_Ikkinz_1-5, 294631 lockout_ikkinz, 294579/294585/294596/294621/294627/294628 a_pile_of_bones (11 hp loot chests — 294621 drops the Qvic Sliver!), 294342 Crumbling_Monolith, 294605/294606 Altar_Monument; inktuta 296003 a_strange_noise, 296016/296018/296019 `_`, 296017 #curse_trigger, 296020/296045-296047 a_loose_tile, 296068 #The_Bones_of_Noqufiel (chest), 296075 #noqufiel_trigger, 296004 an_onyx_protector (11M hp guardians x4); txevu 297001 Champ_Event, 297039 `_`, 297041 arena, 297042 feaster, 297052 #Bearer_of_Reflection, 297073 a_decaying_corpse, 297075/297076 a_jailer, 297212 #priest_trigger, 297220 ZMTZ_Dummy, 297224 The_sound; tacvi 298003-298008 `_`, 298040 #Sesnil_Yisnon, 298046/298146/298203/298204 Reflection_of_Kretv_Krakxt; yxtta 291113 **#Jerimo_Jiao** (Uqua giver), 291005/291118 a_silent_monolith (525k static x6 — [memory] BiC objects), 291119/291120 Caridwi (720k static quest NPC); qvic 295063 an_onyx_protector (2.9M static). All of these are in the "un-flag 583" set in Section 4; the important part is they must NOT be re-flagged as rare either.

---

## 4. Raid targets

Current: 713 `raid_target` in 279000-299999 (natimbi 4, qinimi 4, riwwi 1, barindu 7, ferubi 9, vxed 1, yxtta 14, uqua 89, kodtaz 11, ikkinz 260, qvic 15, inktuta 69, txevu 185, tacvi 44). **Proposed keep = 130 (all already flagged, 0 new), un-flag = 583** (natimbi 3, barindu 2, ferubi 4, yxtta 6, uqua 77, kodtaz 5, ikkinz 246, qvic 1, inktuta 62, txevu 151, tacvi 26).

Proposed true-raid list (id, level, hp, spawn method; all `raid_target = 1` today):

| zone | boss | id | lvl | hp | spawn |
|---|---|---|---|---|---|
| natimbi | #Cragbeast_Queen | 280080 | 70 | 501k | 100% static (`natimbi/#Cragbeast_Queen.lua`) |
| qinimi | #Mastruq_Commander_Gorlakt | 281089 | 78 | 1.5M | script |
| qinimi | #Spiritlord_Mind / #Spiritlord_Body | 281090 / 281091 | 76 | 500k | script (Mind has no loot) |
| qinimi | Pixtt_Xictic_Krvne | 281126 | 67 | 505k | script |
| riwwi | #Taskmistress_Krisz | 282105 | 70 | 850k | script |
| barindu | #Colossus_of_War, #Ikaav_Ixvet_Pox (+283162 alt), Overseer_Oltix, #Viarglug | 283049, 283050, 283162, 283065, 283156 | 67-71 | 175-305k | script / static (Oltix, Pox static) — **REVIEW Oltix/Viarglug** (glorified nameds?) |
| ferubi | Packmaster_Skoiat_Pizak, Zun`Muram_Votal, Weapon_Master_Vtiink_Vzaan, Pixtt_Votal (+284103) | 284067, 284080, 284092, 284079, 284103 | 68-71 | 140k-1.05M | static/script |
| yxtta | #Primal_Reptile/#Primal_Arachnid/#Primal_Insect, #Pixtt_Suir_Mindrider, #a_cragbeast_bonecruncher, #Tqiv_Trusik_Fanatic, #a_cavernous_lifesipper, #a_voracious_beetle | 291102, 291115, 291117, 291111, 291050, 291123, 291124, 291125 | 70 | 246-669k | static / script |
| kodtaz | Pixtt_Nemis, Pixtt_Grand_Summoner (+#293212 1M), #Hexxt_Chaos_Provoker, #Ageless_Relic_Protector x2 | 293044, 293113, 293212, 293214, 293222, 293223 | 67-70 | 200k-1M | static / script |
| uqua | #Vrex_Barxt_Qurat (final), Vrex_Barxt_Qurat, Guardian_of_Destruction x3, #Devastating_Construct, Tqiv twins, constructs of rage/fury, #a_destructive_channeler x2 | 292069, 292068, 292067, 292070, 292073, 292065, 292021, 292022, 292015, 292018, 292074, 292075 | 66-73 | 205k-1.5M | script (finals no loottable!) |
| ikkinz raid v3-v6 | Guardian/Custodian_of_Righteousness, Guardian/Sentinel_of_Glorification, Guardian_of_Transcendence, Altar_Overseer, #Keeper_of_the_Altar, #Vrex_Xalkak_Nixki (3.3M), #Guardian_of_Glorification/#Guardian_of_Transcendence alts, #Spirit_of_Tunik_Tamuik, #Stalwart_Caretaker, #Oracle_of_the_Altar, #Altar_Construct | 294341, 294340, 294471, 294464, 294500, 294604, 294620, 294595, 294582, 294586, 294577, 294601, 294613, 294618 | 68-75 | 376k-3.3M | static per version / script |
| inktuta | #Noqufiel (L80 x2 + ##), #Noqufiel L73, #Kelekdrix, #Mimezpo_the_Oracle | 296065, 296066, 296074, 296052, 296024, 296027 | 70-80 | 326k-2.05M | script (Noqufiel/Kelekdrix have no loottable — loot via chest 296068) |
| txevu | #Zun`Muram_Tkarish_Zyk (final), #Ikaav_Nysf_Lleiv, #Ixt_Hsek_Syat, #High_Priest_Nkosi_Bakari, #Ancient_Cragbeast_Matriarch, #Ukun_Bloodfeaster, Aneuk twins, Ixt_Quall_Nalnn/Ixt_Kial_Ivesi (6 variants, 33% slots), #Mastruq_Champion, Mastruq_Beast_Warden, 15 #Vrex_Invoker variants, 3 #Vrex_Stonemaster, 2 #Inquisitor | 297150, 297090, 297211, 297218, 297056, 297082, 297023, 297025, 297032, 297043, 297044, 297049, 297074, 297091, 297034, 297048, 297057-297061, 297166, 297167, 297182-297184, 297197-297199, 297087-297089, 297148, 297149 | 66-80 | 240k-2.1M | static/script |
| tacvi | PXK, PKK, PRT, ZMKP, ZMSB, ZMMD (+4 phases), ZMYV, TMCV (298055 L80 2.86M + 298014), Living_Phylactery, #Balance_of_* x4 | 298039, 298201, 298032, 298029, 298018, 298020, 298050-298053, 298023, 298055, 298014, 298113, 298125-298128 | 73-80 | 211k-2.86M | script (`tacvi/zone_status.lua`) |
| qvic | Mnat/Dvoin/Ptav_M`sha, Iqthinxa_Karnkvi (+#295150), #Cynosure_Kvanjji (+##295149 1.4M), Rav_Gernkki/Marnkki/Karnkki, 4 Arbitors | 295001, 295087, 295134, 295130, 295150, 295140, 295149, 295131-295133, 295145-295148 | 69-74 | 364k-2.26M | static / script |

28 of these 130 have no loottable (Uqua finals, Noqufiel, Kelekdrix, Tunat 298014, ZMMD phases, Balances, Stonemasters, Cynosure 295140, Ytvagi) — loot comes from chests/scripts; they still trigger `raid_levelblock_loot` (1-3 items) which is desirable.

**"Finishing GoD" set (proposal, decide with the owner):** the six zone finals — 292069 #Vrex_Barxt_Qurat (Uqua), 294595 #Vrex_Xalkak_Nixki (Ikkinz raid trial 4, `ikkinz_chambers_of_destruction`) **[memory: needs confirmation that Nixki, not 294604 Altar_Overseer, is the final]**, 295130/295150 Iqthinxa_Karnkvi (Qvic), 296065/296066/296074 #Noqufiel (Inktu`Ta), 297150 #Zun`Muram_Tkarish_Zyk (Txevu), 298055 #Tunat`Muram_Cuu_Vauax (Tacvi). Rationale: mirrors PoP "T1+T2 gods" (accessible tier) but GoD's whole identity is the linear Uqua -> Ikkinz -> Qvic -> Inktu`Ta -> Txevu -> Tacvi ladder, and Tacvi is where the era ends. Cheaper alternative if the owner wants it reachable by more guilds: drop Tacvi/Txevu and require Uqua + Ikkinz 4 + Qvic + Inktu`Ta + Cragbeast Queen + Zun`Muram Votal.

**Tracking (precedent = Aurelian Stoneward):** `EVENT_KILLED_MERIT` fires once per raid member (`zone/attack.cpp:2772-2785`) and per group member (`attack.cpp:3061-3066`), so per-boss scripts can do exactly what `codecay/200055.pl` / `potorment/Saryrn.lua:98-107` / `sseru/#Lord_Inquisitor_Seru.pl:47-55` do: `quest::set_data("god_<boss>_" . $account_id, $char_name)` + server-first world emote. Keys: `god_vrexbarxt_`, `god_vrexxalkak_`, `god_iqthinxa_`, `god_noqufiel_`, `god_tkarishzyk_`, `god_tunatmuram_`. Because several bosses have alt npc ids, put the hook in **each** id's script (Lua for tacvi/inktuta encounters, Perl/Lua elsewhere) or add a small `%GOD_COMPLETION_BOSSES = (id => key)` map handled in `global_npc.pl` EVENT_KILLED_MERIT (no such sub exists today; adding one there is cleaner than 12 script edits and survives PEQ script refreshes). Then extend `guildlobby/Aurelian_Stoneward.pl` with `CheckGoDComplete`, `HandleGoDRequest`, popup ids 5011/5012, charm hand-in 4038 -> new charm, `GrantCompletionTitles` block, and the hail menu (LoY has no completion tracker; GoD would be the 6th era).

---

## 5. Completion rewards

**How titles work here:** `titles` table (226 rows, max id 426; custom era titles 398-424; sets pair a serious + joke title on the same `title_set`, e.g. 419 "the Lunar Ascendant"/420 same set, 421 "the Godbreaker"/422 "How do I fix my Flags", 423/424 LDoN). Granted with `quest::enabletitle(id)` (`Aurelian_Stoneward.pl:199-215`, idempotent, per-char bucket `title_<era>_<id>_<charid>`).
**Proposal:** id 427 suffix **"the Discordant"** (or "Muramite's Bane"), id 428 suffix joke e.g. "Still Waiting on a Tacvi Flag", both `title_set = 427`, `class -1`, `char_id -1`, `status -1`, `item_id -1`. Rollback = DELETE ids 427-428.

**Charm chain (all slot 1, classes/races 65535, worneffect 3660, click 3343):** 2854 First (hp 110), 2855 Second (185), 2827 Third (232, aug7), 2829 Fourth (298/278 mana, ac 31, heroics 6-7, sd 12, ha 6, regen 9, aug7), 4038 Fifth (identical to Fourth + augslot2type 8). No `item_tier_map` rows exist for any charm (verified) — the charm is deliberately outside the T1/T2/T3 system; keep it that way (a single reward item, no variants).
**Proposal: item 4042 "Charm of the Sixth Age"** (4042-4050 are free; 4038-4041 are the PoP custom block; max custom id below 300000 is 147494 so any small id is fine). Stats scaled ~1.3x Fifth and benchmarked against god4 T3 avg (hp 409, ac 57, stats 142, heroic ~4) and the proposed god4 heroic pass: hp 390, mana 365, endur 365, ac 42, str/sta/dex/agi 28, int/wis 28, cha 10, heroic 9 each (63 total), resists mr/fr/cr/dr/pr 22, spelldmg 18, healamt 12, attack 15, regen 12, manaregen 12, avoidance 4, accuracy 5, augslot1type 7, augslot2type 8, augslot3type 7, lore "Discord is just order you haven't broken yet", nodrop 0 (matches chain), reqlevel 0. Create with `INSERT ... SELECT` from 4038 then UPDATE the numbers so all 300+ item columns carry over. Grant path: hand-in of 4038 at Aurelian after `god_awarded_<charid>` (same pattern as `Aurelian_Stoneward.pl:537-552`). Companion rewards following PoP: three polymorph wands 4043-4045 (Mastruq / Ukun / Kyv illusions — needs a clickeffect spell id each; PoP used 33632-33634; check `spells_new` for free illusion spells or reuse existing GoD illusion spells) — optional.
Items live in shared memory: the charm/wand INSERTs must be applied **before a normal restart**, ideally in the pre-apply phase.

---

## 6. Loot pools and dispatch

`ascendant_loot_pools.pl` additions (after `loy_raid_level_blocks`, line 1511):
- `god_zone_ids`: 279 abysmal, 280, 281, 282, 283, 284, 285, 286, 287, 288, 289, 290, 291, 292, 293, 294, 295, 296, 297, 298. None of these ids appear in classic/kunark/velious/ldon/luclin/pop/loy `*_zone_ids` (grep `279 =>`/`280 =>` = 0 hits), so first-match-wins is safe. Because abysmal is a hub, add it to the `_rare_pool_zone_ids` exclusion in `ascendant_loot_utils.pl:389-391` (`if ($zone_exp eq 'god') { @zone_ids = grep { $_ != 279 } @zone_ids; }`) so hub NPCs never become rare-roll sources (mirrors PoK/PoTranq 202/203).
- `god_rare_pools`: `return {}` (DB loottable roll, like pop/loy — `uses_rare_loottable_mischief` returns 1 for anything after `pop` in `active_expansions()`, `ascendant_loot_utils.pl:233-246`).
- `god_level_blocks` (from the tier lists' level spread): `1: 1-54` (natimbi/qinimi/barindu lowbie nameds L43-54), `2: 55-62` (qinimi/riwwi/sewers L57-62), `3: 63-66` (sewers/ferubi/kodtaz/tipt L63-66), `4: 67-255` (kodtaz/yxtta/qvic/vxed L67-70). Each block has >= 6 rare sources.
- `god_raid_pools` (equippable, non-Summoned, non-book/note/aug-type 20/21/54, chance > 0, from the 130 proposed raid ids; **all 277 have T3 variants**):
  - block 1 (boss <= 70, 101 items): 52111,52113,52114,52116,52118,52119,52125,52422,67736,67737,67738,67739,68358-68365,68442-68449,68547,68548,68550,68551,68553,68554,68556,68557,68559,68560,68562,68563,68565,68566,68568,68569,68637-68666,68670,68671,68672,68703-68706,68708-68715,68719,68722,68744-68747,68758,68772,68780,68788,68802,68803
  - block 2 (71-74, 68): 52115,52432,68549,68552,68555,68558,68561,68564,68567,68570,68723,68724,68725,68740-68743,68748-68757,68771,68773,68775,68778,68779,68781,68782,68783,68785,68786,68787,68789,68790,68808,68834,68835,68837,68838,68840-68848,69131,69133,69155-69162,69168,69169,69175,69176
  - block 3 (75-77, 55): 68770,68776,68777,68784,69041,69046,69068,69071,69072,69073,69076,69077,69079,69081,69083,69086,69088,69090,69095-69098,69110-69125,69130,69132,69134,69135,69154,69163-69166,69177,69178,69179,69251-69255
  - block 4 (78+, 50): 69040,69042-69045,69047-69057,69060-69067,69069,69070,69074,69075,69078,69080,69082,69084,69085,69087,69089,69099,69100,69106,69126-69129,69146-69153
  (67736-67739 in block 1 are the Uqua "trinkets" that block the destruction aura — `uqua/player.lua:22-23` — consider excluding them from bonus loot like the PoP key 9433 in `excluded_bonus_loot_item_ids`, `ascendant_loot_utils.pl:173-174`. Also exclude Qvic/Ikkinz key items if any equippable: 60225/60227/60233/60240/60249 are not in the pools.)
- `god_raid_level_blocks`: `1: 1-70`, `2: 71-74`, `3: 75-77`, `4: 78-255`.
- `god_raid_block_overrides` (new, dispatch exists only for pop at `ascendant_loot_utils.pl:118-122`): force Tacvi L75 Zun`Murams (298018, 298020, 298023, 298029, 298050-298053) and Txevu L75 (297023, 297025, 297032/297043/297044/297049/297074/297091, 297056, 297082) into block 3 (they are), and force zone finals into block 4: 292069 (L73), 294595 (L75), 295130/295150 (L74), 296052 (L73), 298014 (L77). Add `if ($exp eq 'god') { return plugin::god_raid_block_overrides(); }`.

`ascendant_loot_utils.pl` edits: line 15 append `'god'` to `active_expansions()`; lines 64, 76, 96, 105, 114 add `god` branches; line 118-122 add `god` overrides branch; line 389 abysmal exclusion.
`global_npc.pl:337`: `... || $zone_exp eq 'loy' || $zone_exp eq 'god')`.
Fellowship `get_merged_pool` (`ascendant_loot_utils.pl:198-225`) returns empty for god (rare pools empty) exactly as for pop/loy — no change.

---

## 7. Gear stats and the boost script

Measured 2026-09-26 (avg per block; "GoD" = drops of the 130 proposed raid ids; PoP re-measured the same way for like-for-like):

| block | items | hp | mana | ac | stats | heroic | resists | sd | ha | atk | regen |
|---|---|---|---|---|---|---|---|---|---|---|---|
| god1 base (<=70) | 101 | 137 | 130 | 22 | 52 | 0 | 47 | 0 | 0 | 4.4 | 1.0 |
| god1 T3 | 101 | 233 | 220 | 31 | 71 | 3.1 | 63 | 1.4 | 1.2 | 7.5 | 1.2 |
| god2 T3 (71-74) | 68 | 363 | 343 | 46 | 122 | 3.4 | 98 | 1.0 | 1.1 | 11.4 | 1.9 |
| god3 T3 (75-77) | 55 | 440 | 380 | 50 | 143 | 3.0 | 129 | 1.1 | 1.2 | 7.6 | 1.0 |
| god4 T3 (78+) | 53 | 409 | 392 | 57 | 142 | 4.1 | 119 | 1.3 | 1.8 | 17.0 | 3.1 |
| pop2 T3 (67-70, raw drops) | 71 | 149 | 135 | 15 | 50 | 3.1 | 37 | 0.8 | 1.2 | 2.3 | 0.1 |
| pop3 T3 (71-74) | 99 | 252 | 233 | 33 | 95 | 15.9 | 70 | 5.8 | 4.9 | 11.0 | 3.2 |
| pop4 T3 (75-79) | 119 | 259 | 235 | 31 | 103 | 26.5 | 72 | 9.7 | 8.5 | 14.4 | 5.3 |
| pop5 T3 (80+) | 42 | 290 | 273 | 41 | 124 | 34.0 | 85 | 12.6 | 13.0 | 21.9 | 8.1 |
| GoD group-named T3 (non-raid 40-200k hp droppers) | 69 | 171 | 157 | 25 | 57 | 2.9 | 48 | | | | |
| PoP group-named T3 (rare_spawn droppers) | 326 | 123 | 111 | 18 | 46 | 5.7 | 37 | | | | |

(Owner's pool-based PoP numbers — pop3 hp 243 heroic 25.9, pop4 249/30.7, pop5 363/37.7 — are consistent with these.)

Conclusion: **no hp/mana/ac/stat/resist multipliers for GoD T3** (they already sit 1.3-1.7x above the PoP block of the same boss level, which matches GoD's historical power jump). What is missing is the heroic/secondary pass. Proposed per-block targets (T3 only), each one PoP block up plus ~10%:

| block | heroic per attrib (formula) | target heroic sum | regen | attack (STR/DEX>5) | avoid (AGI/STA>5) | spelldmg (INT>5) | healamt (WIS>5) |
|---|---|---|---|---|---|---|---|
| god1 | `3 + (id*p)%4` (3-6) | ~25 (vs pop3 26) | 4 | 12 | 3 | 12 | 10 |
| god2 | `4 + (id*p)%5` (4-8) | ~32 (vs pop4 31) | 6 | 16 | 4 | 14 | 12 |
| god3 | `5 + (id*p)%6` (5-10) | ~40 (vs pop5 38) | 8 | 20 | 5 | 17 | 15 |
| god4 | `6 + (id*p)%7` (6-12) | ~48 (new top) | 10 | 26 | 6 | 20 | 18 |

Heroic formula/primes identical to `luclin_raid_ascendant_boost.sql:19-25` and `2026_09_25_loy_raid_ascendant_boost.sql:97-110` (`CASE WHEN i.astr > 0 THEN GREATEST(IFNULL(i.heroic_str,0), base + ((i.id * 7) % range))`; primes 7/11/13/17/19/23/29; cha uses smaller range). Script `2026_10_XX_god_raid_ascendant_boost.sql`: backup table `asc_patch_god_raid_t3_YYYYMMDD` (same columns as the LoY one), `tmp_god_raid_blk (base_id, blk)` populated with the 274 base ids above, `INSERT IGNORE` backup from `item_tier_map tier_code = 3` variants (`i.id > 700000`), a single heroic/secondary UPDATE with `CASE b.blk` for the per-block bases, `GREATEST` everywhere, computed from live values (heroic pass is monotonic so re-runs are no-ops). Rollback restores all columns from the backup and drops the table. Verification: `backup_rows` = 277; per-block AVG(heroic) ~25/32/40/48. Group-named GoD T3 needs nothing (already 1.4x PoP named T3). Also give the Uqua trinkets 67736-67739 and Qvic/Ikkinz keys a `WHERE NOT IN` if you want them untouched. Takes effect at the next normal restart (never `#hotfix`).

---

## 8. Mob scaling — `ascendant_encounter_scaling_god.pl`

Copy **`ascendant_encounter_scaling_loy.pl`** (556 lines; it is itself a verbatim copy of the PoP module with only the zone map and names changed — diff verified — and its comments already say "Velious/LDON/Luclin/PoP") and rename `Loy`->`God`, `LOY_`->`GOD_`, `_loy_`->`_god_`, `loy`->`god` (the LoY checklist step 1). Zone map `%GOD_ZONES`: 280-298 (omit 279 abysmal like nedaria was omitted at `ascendant_encounter_scaling_loy.pl:25-27`, and omit 299/998).
Profile values: start **resist-only** like PoP/LoY (`ascendant_encounter_scaling_loy.pl:96-140`: hp/melee/atk 1.00 everywhere; resists raid -101/-32/-23/-4, named -82/-17/-14/-3, trash -50/-11/-8/-2; stat floors raid 2.5/6.0, named 2.0/5.0, trash 1.5/3.5 per level). GoD-specific concerns and recommended deltas relative to the doc's Velious tables (`ascendant_server.md:42-66`):
- GoD trash hits 250-1,300 at L65-70 with 20-45k HP and raid bosses 1,000-7,000 max_hit with 1-3M HP against a 65-cap population; PoP resist-only worked because PoP damage is lower. Recommend enabling **HP scaling for solo/duo only** on trash/named (hp_solo_floor 0.60 / ceiling 0.85, group 1.00) and leave raid HP at 1.00 — GoD raids are meant to be raids. Keep melee/ATK at 1.00 (the Velious table's >1.00 group multipliers would make Tacvi impossible).
- Resist deltas: GoD mobs have very high MR/resists by design; the raid -101 solo floor is fine, but consider a slightly deeper **named** floor (-90) so the 5% shared-slot nameds in kodtaz/yxtta (105k hp) are killable by a group.
- Add `%GOD_NPC_EXCLUDE` for the invulnerable/guardian NPCs that must not be touched: 296004, 295063 (onyx protectors), 291005/291118 (monoliths), the `_` controllers 296016/296018/296019/298003-298008, 292079/294631 lockout NPCs, 297220 ZMTZ_Dummy.
- Mechanics that scaling must not fight: Uqua destruction aura (`uqua/player.lua:14-35`), Tacvi/Inktu`Ta phase bosses that swap npc ids mid-fight (298050-298053 ZMMD phases, 296052->296065 Noqufiel), charm-credit wave mobs (`lua_modules/charm_credit.lua`, vxed base-zone counter). Because the module snapshots on engage and restores on disengage, phase swaps are safe; just keep `hp_*` at 1.00 for raid.
- Kill switch `$GOD_SCALING_ENABLED`.
`global_npc.pl` edits: line 149 add `&& !plugin::IsGodScalingZone($zoneid)`; lines 534/550 add `plugin::GodScaling_OnEngage($npc);` / `GodScaling_OnDisengage`; line 571 `GodScaling_Rescan`.

---

## 9. Expeditions and Tyrael

`ascendant_expedition_config.pl` (append after line 406, new `# ---- Gates of Discord ----` block): natimbi (`has_raid_tier => 1`: Cragbeast Queen), qinimi (raid: Gorlakt/Spiritlords/Xictic), riwwi (raid: Krisz), barindu (raid: Colossus/Ikaav/Oltix), ferubi (raid: Votal/Packmaster/Weapon Master), kodtaz (raid: Nemis/Grand Summoner/Chaos Provoker), yxtta (raid: Primals — **but** note that a Normal DZ there depops nothing important once Jerimo/monoliths/Caridwi are un-flagged), qvic (raid tier only — every boss is raid; a Normal Qvic would be empty of bosses: offer `has_raid_tier => 1` and accept that "Normal" is a trash-only zone, or omit qvic from Tyrael), txevu (same as qvic). **Do not add** uqua, ikkinz, inktuta, tacvi, snpool/snlair/snplant/sncrematory, vxed, tipt, abysmal, qvicb, fhalls — they are instance-only / have native DZ + lockout scripts (potime precedent, `ascendant_expedition_config.pl:270-271`). `entry_override` is not needed (safe points are inside the zones).

Tyrael (npc 186202, L70, 100k) spawn rows — 9 zones, IDs continue from `spawn2` max 3266248 / `spawngroup` max 3289046 (safe point +15 x, like `2026_09_04_loy_planeshifter_tyrael_spawns.sql`):

| spawngroup | spawn2 | zone | x | y | z |
|---|---|---|---|---|---|
| 3289047 | 3266249 | natimbi | -1542 | -853 | 239 |
| 3289048 | 3266250 | qinimi | -1038 | 438 | -16 |
| 3289049 | 3266251 | riwwi | 469 | -650 | 35 |
| 3289050 | 3266252 | barindu | 605 | -1457 | -123 |
| 3289051 | 3266253 | ferubi | 1500 | 596 | 111.8 |
| 3289052 | 3266254 | kodtaz | -1460 | 1548 | -302.12 |
| 3289053 | 3266255 | yxtta | 1250 | 1300 | -348 |
| 3289054 | 3266256 | qvic | -2500 | 767 | -647 |
| 3289055 | 3266257 | txevu | -317 | -1 | -420 |

No odd safe_z (all are the real floor heights of those zones; kodtaz/qvic/yxtta/txevu are deep negative by design). Same spawngroup params (delay 45000, mindelay 15000, despawn_timer 100), spawnentry chance 100, respawntime 1200, cond_value 1. `#repop` each after applying; walk-check for walls.

---

## 10. AAs / tomes

- 103 PEQ GoD abilities exist at `aa_ranks.expansion = 7` (level_req 1); 40 of them have `aa_custom_mapping` rows (tier 2 except Virulent Venom tier 3) with illegible tome ids 121571-121618. They already satisfy `ur.expansion <= 16` (`ascendant_insight_trainer.pl:284-286`) so they should already be listed at the trainers — **GM verify** (browse a Beastlord/Necro trainer for Feral Swipe / Swarm of Decay). The same is true for 32 OoW-mapped AAs at expansion 8 (Destructive Fury, Shield Block) — check whether that is intended.
- **Do not change `$CURRENT_MAX_EXPANSION` (16)**: expansion 17 rows are 172 level-70 later-era abilities.
- **Add the 40 GoD tome ids to `ascendant_tome_translator.pl`** (`$tomes{<classmask>}{2}` / `{3}` arrays, lines 45+) so they enter the random drop pool; per-class split from `aa_custom_mapping.original_classes`. Without this GoD AAs are only obtainable with credits from other tomes.
- Fix the misleading comment at `ascendant_insight_trainer.pl:12`.

---

## 11. Other systems

1. **Travel (blocking):** options — (a) extend `nedaria/Magus_Wenla.lua` with an "Abysmal Sea" option (`MovePC(279, 0, -199, 140, 0)`) and add a reverse option to `abysmal/Magus_Pellen.lua` (already ports to Natimbi) — cheapest, script-only, no SQL; (b) add a PoK stone door to Natimbi (no PEQ asset; would need a custom door); (c) rely on Magus Zeir port spells + wizard/druid — not acceptable for a 50-player server. Recommend (a) plus adding abysmal/natimbi to the guild-lobby transport if one exists (`Nyra_Silvermark.pl` is bazaar<->lobby only).
2. **Txevu flag:** either script a drop of 60254 Cipher of Txevu from Iqthinxa Karnkvi (295130/295150) via `EVENT_DEATH_COMPLETE` summon to killers **[memory: on live the Txevu flag came from Qvic bosses]**, or set `zone.flag_needed = ''` for 297 (and optionally 293/295) so Tyrael raids can enter. Owner decision; default recommendation: keep flags, add the Cipher drop.
3. **PoK Gunthak stone** 9761/12287 + gunthak 12119 turn on at 7 (desired; LoY doc noted it). No GoD PoK stone exists.
4. **Merchants:** 42 tome rows at 7 (Berserker/Rogue/Monk/Warrior disc tomes in PoK) + Magus Zeir 11 at 6 — `#reload merchants global`.
5. **Tradeskills:** 0 `tradeskill_recipe` rows gated at 6/7; Abysmal Sea tradeskill quests (241 scripts) go live with zone access.
6. **Spells:** GoD scrolls are ungated on PoK vendors; 35 scrolls (59846-64579) are on no merchant — list them and decide (likely rune-quest spells **[memory]**).
7. **Gambling:** `ascendant_gambling.pl` has no `GetGamblePool` (doc `ascendant_server.md:435` is stale); pools are static id lists — optionally add a few GoD T3 items to `@JACKPOT`/`@LEGENDARY`.
8. **Caster rules** `Ascendant:PureCasterRaidTargetDmgBonus 25` / `DoTCasterRaidTargetDmgBonus 20` apply to every kept raid_target — another reason to strip the 583 flags.
9. **Content flags:** none needed (LoY used none). `zone.expansion` stays 7 (unlike LDoN's rewrite to 2).
10. **Charm credit** (`lua_modules/charm_credit.lua`, vxed encounter) already covers the Vxed counter.
11. **Zone-level `min_level`** on qinimi/riwwi/barindu (50), ferubi (55), vxed (60), yxtta/kodtaz/qvic (65) are enforced in `CanEnterZone` (`zoning.cpp:1431-1437`) — fine for a 65 cap.
12. `Expansion:CurrentExpansion` is read at boot and by `#reload content_flags global` (LoY doc) — same procedure.

---

## 12. Ordered implementation plan

Dates use the intended release week; replace `10_XX`.

**Phase A — invisible pre-apply (safe while live; nothing gated by rule 5 changes):**
1. Quest/plugin files (no restart needed beyond `#reloadquest`/zone reload; GoD zones are not running anyway):
   - new `plugins/ascendant_encounter_scaling_god.pl` (Section 8) — `perl -c` in the container;
   - `plugins/ascendant_loot_pools.pl` append `god_*` subs (Section 6); `plugins/ascendant_loot_utils.pl` lines 15/64/76/96/105/114/118-122/389;
   - `global/global_npc.pl` lines 149, 337, 534, 550, 571 (+ optional `EVENT_KILLED_MERIT` completion map);
   - `plugins/ascendant_expedition_config.pl` GoD block;
   - `plugins/ascendant_tome_translator.pl` 40 GoD tome ids;
   - `guildlobby/Aurelian_Stoneward.pl` GoD era (popup ids 5011/5012, buckets `god_*`, charm 4038 -> 4042);
   - boss `EVENT_KILLED_MERIT` hooks (or the global map);
   - `nedaria/Magus_Wenla.lua` + `abysmal/Magus_Pellen.lua` travel;
   - optional Iqthinxa Cipher drop script.
2. `2026_10_XX_god_named_flags.sql` (+ `2026_10_XX_rollback_god_named_flags.sql`): backup `asc_patch_god_flags_YYYYMMDD` (npc_id, name, old_rare, old_raid), temp tables `tmp_god_rare` / `tmp_god_raid_keep` / `tmp_god_unraid` / `tmp_god_demote`; UPDATE `raid_target = 0` for the 583 (all GoD-range raid_target ids NOT IN keep list — but list them explicitly like LoY did, don't use NOT IN for the apply), `raid_target = 0, rare_spawn = 1` for the 11 demotions, `rare_spawn = 1` for Tier A/B/C. Verification SELECTs: `god_raid_target (expect 130)`, `god_rare_spawn (expect ~145)`, `triggers_still_raid (expect 0)` over the 80 trigger ids, `backup_rows (expect 130 + 583 + 145 minus overlaps)`. Then `#repop` is not needed (zones not running).
3. `2026_10_XX_god_planeshifter_tyrael_spawns.sql` (+ rollback deleting spawn2 3266249-3266257 / spawngroup 3289047-3289055 / spawnentry npc 186202). Verify 9 rows.
4. `2026_10_XX_god_completion_rewards.sql` (+ rollback): titles 427/428, item 4042 (+ optional wands 4043-4045). Verify `SELECT id FROM titles WHERE id IN (427,428)` = 2, `items 4042` = 1.
5. `2026_10_XX_god_raid_ascendant_boost.sql` (+ rollback): heroic pass, backup 277 rows, per-block AVG checks.
6. **Normal restart** (items 4042+, boosted T3, shared memory). This restart can be any scheduled reboot before release day.

**Phase B — GM test (rule still 5, `#gm on` bypasses `zone.expansion`):** with a GM char: zone to abysmal/natimbi (via `#zone`), hail Tyrael in all 9 zones, create Normal + Raid expeditions in natimbi and kodtaz, confirm Cragbeast Queen (280080) absent in Normal / present in Raid, confirm `#Jerimo_Jiao` and monoliths present in a yxtta Normal DZ; kill a Tier A named in kodtaz and check the rare loottable roll + tier upgrade debug; kill Cragbeast Queen and check 1-3 raid-pool items; run Vxed (Udranda) and Tipt DZs to confirm the temp/perm flag chain; run Ikkinz group trial 1 (Gazak); Uqua via Jerimo (check the aura, gas chambers, lockout NPC 292079 un-flagged); Qvic door 3 -> Inktu`Ta (Aganetti) and confirm Noqufiel bones chest; Txevu Zun statue -> Tacvi with 64034 and confirm `tacvi/zone_status.lua` lockouts; scaling debug lines `[GodScaling]` on engage; Aurelian GoD popup; charm hand-in on a test char with buckets pre-set; insight trainer shows GoD AAs; Magus Wenla -> abysmal -> Pellen -> natimbi. Also zone to droga/nurga with `#gm on` after temporarily... (cannot test the 7-routing without the rule; accept from code reading, or test on the local test stack).

**Phase C — release day switch:** `2026_10_XX_god_gate_release.sql` (+ rollback): `UPDATE rule_values SET rule_value='7' WHERE ruleset_id=1 AND rule_name='Expansion:CurrentExpansion' AND rule_value='5';` (nothing else — no min_status to clear; qvicb/fhalls stay 80). Then in game: `#reload content_flags global`, `#reload merchants global`, `#reload zone_data global`, `#repop` in poknowledge (Vivian/Tenada + Gunthak stone door — doors need a zone restart or `#reload doors`), southkarana/northkarana/qey2hh1/lakerathe/commonlands/qrg/ecommons (Magus Zeir list) if they are up; droga/nurga: players already inside v0 will be in a zone whose spawns vanish on repop — announce and boot those zones. Verification: rule value 7; `SELECT COUNT(*) FROM spawn2 WHERE zone='droga' AND version=1` = 577; a non-GM zoning to natimbi succeeds.

**Rollback:** gate rollback (7 -> 5) + reloads restores the pre-release state instantly; flags/Tyrael/rewards/boost rollbacks restore from their backup tables; items need a restart. Tyrael rows can stay (harmless) if only the gate is rolled back.

**Post-release monitoring (first 72h):** `gm_audit_log`; count of `KILLED_RAID_NPC` player events in GoD zones vs raid list (any trash appearing means a missed flag); shard rate (`shard_cd_*`) and raid-pool drops from `quest::debug` `RAID LEVELBLOCK BONUS` lines in zone logs; expedition creation errors in txevu/qvic (flag denials — `LogInfo "does not have the flag"` in zone logs); watch for the Droga/Nurga complaints; `#Jerimo_Jiao` present in yxtta; Tacvi/Inktu`Ta lockout bucket growth.

---

## 13. Open questions / decisions (owner) and player-expert input

1. Droga/Nurga v0 -> v1 flip: accept, or pin them (add `max_expansion = 99` to the v0 spawn2/doors/zone rows — 566 rows — or set `zone.expansion` differently)? **Owner.**
2. Travel into Abysmal Sea: Magus script edit (recommended) vs PoK door. **Owner.**
3. Txevu flag: add Cipher drop vs clear `flag_needed` on 297 (and 293/295). **Owner.**
4. "Finishing GoD" boss set: six finals (Tacvi end) vs accessible set. Confirm Ikkinz raid 4 final id (294595 vs 294604). **Owner + player expert.**
5. Raid vs glorified-named judgement (LoY/Ali-style review): Overseer_Oltix 283065, #Viarglug 283156, Pixtt_Xictic_Krvne 281126, Mass_of_Stone 281030, Kabeka_Kret 281125, #Vuranish_Xxuro 290072, Pixtt_Votal 284079/284103, the yxtta 245k "minis" 291050/291123-291125, Ikkinz raid-trial adds (Custodian/Sentinel/Acolytes), Txevu Invoker/Stonemaster/Inquisitor sets, Tacvi Balances. **Player expert.**
6. Tier A REVIEW items: barindu 4,100-hp 50% "nameds", kodtaz Forlorn_Relic_Devotee multi-slots. **Player expert.**
7. Qvic/Txevu Normal expeditions (boss-less) — offer or omit. **Owner.**
8. Scaling: resist-only (PoP/LoY precedent) or add solo HP floors for trash/named. **Owner.**
9. Charm 4042 numbers and whether wands are wanted; title wording. **Owner.**
10. GoD/OoW AAs already visible at trainers — intended? **Owner (verify in game).**
11. The 35 GoD spell scrolls with no merchant. **Owner.**
12. Ikkinz raid-trial requester NPC and the 4 door keys — not traced; confirm the chain is complete before release. **GM test.**

---

## Key queries run (all SELECT/SHOW/DESCRIBE)

1. `SELECT ... FROM zone WHERE expansion = 7` (zone inventory, flags, safe points, versions).
2. Level profile: `spawn2 JOIN spawnentry JOIN npc_types JOIN zone(expansion=7) ... GROUP BY zone, version` with expansion filters.
3. Side effects: per table `WHERE min_expansion IN (6,7) OR max_expansion IN (5,6)` on spawn2 (by zone/version), merchantlist, doors, zone_points, ground_spawns, object, traps, lootdrop_entries, zone, start_zones, fishing, forage, tradeskill_recipe, global_loot; detail joins for the 25 spawn2 rows, 6 merchants, 3 doors; `instance_list WHERE is_global=1`.
4. Flags: `npc_types WHERE id BETWEEN 279000 AND 299999 GROUP BY FLOOR(id/1000)` (rare/raid counts); full raid_target list; hp >= 200k list with spawnentry count/min chance/zones; trigger-pattern list (bodytype 11/33/34/67, hp <= 200 or 2147483647, name patterns).
5. Tiers: Tier A (chance <= 50, group entries > 1), Tier B (chance 100, single entry, hp >= 20k), Tier C (no spawnentry, loottable > 0, hp >= 20k) with named-style name filters.
6. Ikkinz per-version top-HP NPCs; vxed/snplant versions.
7. Proposed raid set (130 ids in a temp table): kept/un-flag counts per zone; equippable item ids per boss-level block; bosses with no loottable.
8. Gear: AVG hp/mana/ac/stats/heroic/resists/sd/ha/atk/regen/dmg for base and T3 (`item_tier_map tier_code=3`) by block for GoD raid drops, PoP raid drops, GoD/PoP group-named drops; T3 coverage (277/277).
9. Rewards: items 2827/2829/2854/2855/4038-4041/17662/17663/17675/2830 full stats; `item_tier_map` for charms (none); `titles` >= 398; free ids 4042-4050.
10. AAs: `aa_ranks.expansion` distribution with level_req; known-AA expansion lookup; `aa_custom_mapping` counts by expansion; the 40 GoD mapped AAs + tome ids; grep of those ids in the tome translator (0).
11. Spells/merchants: GoD scroll counts on merchants by zone; 35 unsold; Magus Zeir rows.
12. Population: `character_data` class counts, active-30d level distribution, rules (`Character:MaxLevel`, `Expansion:*`, `Ascendant:*RaidTarget*`).
13. Travel: zone_points into/out of GoD zones, doors to abysmal/natimbi, boat NPCs, Magus NPCs in nedaria/abysmal/natimbi.
14. IDs: `MAX(spawn2.id)` 3266248, `MAX(spawngroup.id)` 3289046; existing Tyrael spawn zones (96).

### Critical Files for Implementation
- /opt/akk-stack/server/quests/global/global_npc.pl (lines 122-136, 149, 335-341, 527-551, 564-572, new EVENT_KILLED_MERIT map)
- /opt/akk-stack/server/quests/plugins/ascendant_loot_pools.pl (append god_* after line 1511) and /opt/akk-stack/server/quests/plugins/ascendant_loot_utils.pl (lines 15, 56-122, 382-394)
- /opt/akk-stack/server/quests/plugins/ascendant_encounter_scaling_loy.pl (template for ascendant_encounter_scaling_god.pl) and /opt/akk-stack/server/quests/plugins/ascendant_expedition_config.pl (append after line 406)
- /opt/akk-stack/server/quests/guildlobby/Aurelian_Stoneward.pl (completion tracker: add GoD era, charm 4038 -> 4042)
- /opt/akk-stack/server/sql/2026_09_04_loy_named_flags.sql and /opt/akk-stack/server/sql/2026_09_25_loy_raid_ascendant_boost.sql (templates for the GoD flag and heroic-pass scripts)
---

## 14. Owner decisions (2026-09-26)

- **Expansion 6 applied separately** (see review notes); GoD gate = rule 6 -> 7.
- **Travel:** launch path was the boat from Nedaria's Landing to the Abysmal Sea (*Queen of Thorns*; `nedaria/a_navigator.lua`, `a_Wayfarers_Brotherhood_tracker.lua`), then Magus Pellen's Farstone to Natimbi (`abysmal/Magus_Pellen.lua:7`, already works). Boat not implemented -> stand-in: add "Abysmal Sea" to `nedaria/Magus_Wenla.lua` (+ reverse option in abysmal). Pending owner confirmation vs PoK stone.
- **Finishing set: accessible set** — 292069 #Vrex_Barxt_Qurat (Uqua), Ikkinz raid trial 4 final (confirm 294595 vs 294604), 295130/295150 Iqthinxa_Karnkvi (Qvic), #Noqufiel 296065/296066/296074 (Inktu`Ta), 280080 #Cragbeast_Queen (Natimbi), 284080 Zun`Muram_Votal (Ferubi). Txevu and Tacvi are NOT required.
- **Scaling: resist-only** (PoP/LoY precedent).
- **Qvic / Txevu: raid tier only** in `ascendant_expedition_config.pl`.

## 15. Instance compatibility audit (2026-09-26)

Scope: the 9 open-world zones proposed for Tyrael raid instances (natimbi, qinimi, riwwi, barindu, ferubi, kodtaz, yxtta, qvic, txevu). GoD's native DZ zones (uqua, ikkinz, inktuta, tacvi, vxed, tipt, 4 sewers) were checked too: 0 hardcoded-instance calls; uqua/ikkinz/inktuta/tacvi already use instance ids in 10-17 files each.

### 15.1 Same-zone `MovePC(<own zone id>, ...)` — ejects players from the instance
`Client::MovePC(zoneID, x,y,z,h)` calls `ProcessMovePC(zoneID, 0, ...)` (`zone/zoning.cpp:585-587`); in-zone handling only when `instance_id == zone->GetInstanceID()` (`zoning.cpp:698`). Inside an expedition the player is zoned to the **public** zone and leaves the expedition.

| zone | file:line | what breaks |
|---|---|---|
| txevu | `#Zun-Muram_Tkarish_Zyk.lua:69,73` | Txevu final: banish-to-jail mechanic ejects raiders to public Txevu |
| qvic | `Iqthinxa_Karnkvi.lua:112,124,136` | Qvic final: 3 ports eject raiders |
| qinimi | `encounters/execution.lua:61,136,166`, `#Councilman_Sislono_Nislan.pl:54,110`, `#Trigger_Qinimi_1.pl:92` | Execution event / BiC ports |
| riwwi | 21 arena scripts, line 17 (`#Dvax.lua:19`, `#Pixtt_Master_Hviqu.lua:30`) `MovePC(282,-168,330,4.3,0)` | Arena ejection |
| kodtaz | `Pixtt_Nemis.lua:5` | Nemis port |

Fix (existing convention, e.g. `potimeb`/`codecay`): Lua `MovePCInstance(<id>, eq.get_zone_instance_id(), x,y,z,h)`; Perl `$pc->MovePCInstance(<id>, $instanceid, ...)`. Identical behaviour in the public zone (instance 0), so safe to pre-apply.

### 15.2 `eq.spawn_condition("<zone>", 0, ...)` — toggles the PUBLIC zone instead of the instance
`SpawnConditionManager::SetCondition` applies locally only if `instance_id == zone->GetInstanceID()` (`zone/spawn2.cpp:1152`); otherwise it is sent to world for instance 0.

| zone | file:line | event |
|---|---|---|
| txevu | `#Ikaav_Nysf_Lleiv.lua:6,108`, `encounters/acm.lua:68,72`, `arena.lua:7`, `encounters/highpriest.lua:5,17`, `encounters/bloodfeaster.lua:12,154`, `encounters/champ.lua:11` | Every Txevu event (Invokers, ACM, arena, Bloodfeaster, Champion, Ikaav) — adds never spawn in the instance AND public Txevu spawns toggle |
| natimbi | `#Cragbeast_Queen.lua:28` | Cragbeast Queen (a finishing boss) |

Fix: replace `0` with `eq.get_zone_instance_id()` (already used 39+ times server-wide, e.g. `spawn_condition(this_zone, instance_id, ...)`). Perl 3-arg `quest::spawn_condition($zonesn, cond, val)` (riwwi `a_darklurch-s_corpse.pl`) already uses the current instance (`embparser_api.cpp:953-955`) — OK.

### 15.3 Event state stored zone-wide (shared between public zone and every instance)
- `ferubi/Event_Spawner.lua:16-70` — `eq.set_data("ferubi-*-count")` global buckets drive the Master's Apprentice event; parallel instances corrupt each other's counters. Fix: key by instance, e.g. `"ferubi-" .. eq.get_zone_instance_id() .. "-blademaster-count"`.
- `yxtta/player.lua:150` — `primal_door_status` global bucket (3.5 h) gates the Primal event; one group opening it opens/locks it everywhere. Fix: suffix with instance id.
- `kodtaz/player.lua:9`, `yxtta/player.lua:84` — door "fern" qglobals (zone-scoped, 1 h). Low impact; optional.
- Per-character epic/flag globals (necro/paladin/druid epic, `god_*_access`, `bic_*`) are correct as-is.

### 15.4 Nested expeditions — cannot start from inside a Tyrael instance
A character already in an expedition cannot create another (`zone/expedition_request.cpp:264`). These givers only work from the **public** zone: `qvic/Aganetti_the_Keeper.lua` (Inktu`Ta), `txevu/Hamari_Nedu.lua` + `txevu/player.lua:23-30` (Tacvi), `yxtta/#Jerimo_Jiao.lua` (Uqua), `kodtaz/Gazak_Klelkek/Maroley_Nazuey/Kenra_Kalekkio.lua` (Ikkinz), `barindu/#Apprentice_Udranda.lua` (Vxed/Tipt), `natimbi/#Kemik_Arulon.lua` (paladin epic DZ). Recommendation: keep progression in the public zones (Tyrael instances are for killing that zone's bosses) and add a hail line to each giver when `e.other:GetExpedition()` is set: "Leave your current expedition first." Announce in patch notes.

### 15.5 Implementation note
All 15.1-15.3 edits are no-ops in the public zone, so they belong in **Phase A** (pre-apply). Keep `.bak` copies outside `quests/` (convention: `backup/<change>_<date>/`), `perl -c` Perl files, and GM-test each event once inside a Raid expedition during Phase B.

## 16. Applied in Phase A (2026-09-26) — quest scripts

Done and syntax-checked (42 files; backup + README at `backup/god_prep_20260926/`): all Section 15.1-15.3
instance fixes; banish removal (Txevu Zun`Muram Tkarish Zyk jails, Inktu`Ta Kelekdrix + Noqufiel);
death touch removal (Uqua gas chamber cheaters -> port to zone-in); Magus Wenla "Abysmal Sea"
(hidden until `eq.is_gates_of_discord_enabled()`) + Magus Pellen return to Nedaria.
**Policy (owner, 2026-09-26): no banish or death-touch mechanics.** Leash/summon-back ports
(Tacvi/Ikkinz "pull to boss", Qvic Iqthinxa Rav leash) are kept — they are not banishes.
Not done yet: nested-expedition hail messages (15.4), all SQL scripts, plugins, rewards.

## 17. Cross-check against eqprogression.com GoD raid guides (2026-09-26)

Source: https://www.eqprogression.com/gates-of-discord-raid-guide-overview/ and its Kod'Taz progression, Ikkinz raid 4, Qvic, Txevu, Tacvi and Inktu`Ta sub-guides.

**Corrections applied to this plan**
- **Ikkinz Raid Trial #4 final = #Keeper_of_the_Altar 294620** (L72, 989k), not 294595/294604. The Sliver of the High Temple drops from "a pile of bones" after the Keeper (matches 294621). **Finishing set updated:** Uqua 292069, **Ikkinz 4 = 294620**, Qvic Iqthinxa 295130/295150, Inktu`Ta Noqufiel 296065/296066/296074, Cragbeast Queen 280080, Zun`Muram Votal 284080.
- **Death touch found and removed:** Inktu`Ta Cursebearers (296059-296064, npc_spells 1052) proc'd 2156 "Deadly Curse of Noqufiel" (-100000) at 100%. Applied `server/sql/2026_09_26_god_remove_cursebearer_death_touch.sql` (attack_proc -1; rollback alongside). Earlier DT search only covered cast lists, not melee procs — include `npc_spells.attack_proc` in future audits.
- Banishes the guide describes (Kelekdrix pit, Noqufiel punt to previous room) are the ones already removed in Section 16.

**Confirmed (guide matches DB raid list)**
- Qvic: Cynosure Kvanjji, Dvoin/Mnat/Ptav M`sha (random, may take up to 6h to appear in an instance — fine for Tyrael DZs), Iqthinxa Karnkvi.
- Txevu (8): Ancient Cragbeast Matriarch, Aneuk twins, High Priest Nkosi Bakari, Ikaav Nysf Lleiv, Ixt Kial Ivesi / Ixt Quall Nalnn, Mastruq Champion event, Ukun Bloodfeaster, Zun`Muram Tkarish Zyk (final).
- Tacvi (8): Pixtt Xxeric Kex, Pixtt Kretv Krakxt, Pixtt Riel Tavas, Zun`Muram Shaldn Boc, Mordl Delt, Yihst Vor, Kvxe Pirik, Tunat`Muram Cuu Vauax (final).
- Natimbi Cragbeast Queen; Ferubi Packmaster Skoiat Pizak + Zun`Muram Votal; Kod'Taz Summoning Circle (Pixtt Grand Summoner); Yxtta Primals + Pixtt Suir Mindrider; Uqua.

**Review items raised by the guide (player expert)**
- **Smith Rondo 284074** (ferubi): guide lists it as a raid encounter; plan has it as Tier B rare_spawn (L70 100k) — decide raid vs named.
- **Nijacka the Scourge 296013** (inktuta): guide lists it as the first Inktu`Ta encounter; plan demotes it to rare_spawn (9% roaming slot). Inktu`Ta is a native raid DZ (not Tyrael), so raid_target mainly affects loot/scaling — lean keep raid.
- Mimezpo the Oracle is optional in the Inktu`Ta sequence (already in raid list).
- Kod'Taz access per guide needs only Tipt; this DB requires kodtaz+vxed+tipt qglobals (`natimbi/player.pl:1-9`). Accept or relax — owner.

## 18. Owner calls + key/flag chain verification (2026-09-26)

- **Nijacka the Scourge 296013: keep raid_target** (owner). Remove from the Section 3 demotion list.
- **Smith Rondo event is a raid** (owner). Proposed flags: 284074 #Smith_Rondo (static item turn-in NPC) **no flag** (drop from Tier B); Pxet Elite x4 (284039, 284088-284091) rare_spawn; 284092 Weapon_Master_Vtiink_Vzaan raid_target (keep); 284093-284096 Master`s_Apprentice **no flag** (un-flag raid, do not make rare — adds); 284084 #Smith`s_Spectral_Memory **never flagged** (flag giver); 284097 Smith_Rondo (killable, spawns after Vzaan per guide) — **pending owner** (raid vs none).
- **Txevu chain complete (resolves Section 0 finding 3 / Section 17 note):** Tublik Narwethar turns unfinished Sliver 60174 into finished 60176 (`kodtaz/Tublik_Narwethar.pl:178-183`); Brevik Kalaner combines 60176 + Fragment 60252 (Uqua) + Splinter 60253 (Inktu`Ta) into Cipher 60254.
- Keys (100% drops, all on raid keep list): Polished Stone Key 17287 <- 297211 #Ixt_Hsek_Syat (Txevu caves door 20); Jade Inlaid Key 17288 <- 297218 #High_Priest_Nkosi_Bakari (doors 6,7,10,21,56,57; `txevu/player.lua:46-48`); Zun`Muram's Signet of Command 64034 <- 297150 (Tacvi request, `txevu/Hamari_Nedu.lua`, `txevu/player.lua:20`). Door key checks work inside instances.
- Guide sources: eqprogression master key list (GoD section), gates-of-discord-keys, qvic-access-gates-of-discord, gates-of-discord-smith-rondo-raid-guide.

## 19. Owner decisions (2026-09-26, round 2)

- **Smith Rondo:** 284097 Smith_Rondo (killable, spawns after Vzaan) = **raid_target**. Rest of Section 18 table as proposed.
- **Titles approved:** 427 "the Discordant" + 428 joke title (same set).
- **Charm approved:** 4042 "Charm of the Sixth Age" as proposed in Section 5; no wands.
- **Uqua trinkets 67736-67739:** excluded from raid bonus loot (`excluded_bonus_loot_item_ids`).
- **GoD Ascendant gear: generous heroic + secondary pass** ("plenty of heroic stats and extra stats"). Replaces the Section 7 targets (T3 only, GREATEST never lowers):

| block (boss lvl) | heroic per attrib | ~heroic sum | regen | attack | avoid | spelldmg | healamt | extra (accuracy / shielding / spellshield / dotshielding / strikethrough) |
|---|---|---|---|---|---|---|---|---|
| god1 (<=70) | 4-7 | ~30 | 5 | 14 | 3 | 14 | 12 | 3 / 2 / 2 / 2 / 1 |
| god2 (71-74) | 5-9 | ~38 | 7 | 18 | 4 | 16 | 14 | 4 / 3 / 3 / 3 / 2 |
| god3 (75-77) | 6-11 | ~46 | 9 | 23 | 5 | 19 | 17 | 5 / 4 / 4 / 4 / 3 |
| god4 (78+)  | 8-13 | ~55 | 12 | 28 | 6 | 22 | 20 | 6 / 5 / 5 / 5 / 4 |

  (PoP5 T3 today: heroic 37.7, regen 10.2, attack 24.4, sd 16 — god3/god4 exceed it.) Extra stats only on items that fit the role (melee: accuracy/strikethrough; tank: shielding/avoidance; caster/healer: spellshield/dotshielding), same CASE pattern as the heroic pass.

- **AA unlock schedule (owner, supersedes Section 10 "do not bump"):** Classic/Kunark = expansions <=11; Velious +TSS & SoF; Luclin +SoD; PoP +UF (current `$CURRENT_MAX_EXPANSION = 16`); **GoD+ adds House of Thule (17) and beyond** (one step per later unlock).
  - At GoD release: set `$CURRENT_MAX_EXPANSION = 17` (`ascendant_insight_trainer.pl:13`) AND lower `level_req` 70 -> 65 on the 172 expansion-17 ranks the trainer serves (the trainer hides next ranks with level_req > 65, line 340, and blocks purchase below level, line 584). New script `2026_10_XX_god_hot_aa_level_req.sql` with backup table + rollback; verify 172 rows. Same treatment will be needed for 18-21 (level 71-74) at later unlocks.
  - Still add the 40 GoD tome ids to `ascendant_tome_translator.pl`.

## 20. Owner round 3 + eqprogression quest review (2026-09-26)

**Decisions:** keep strict Kod'Taz access (kodtaz + vxed + tipt qglobals, `natimbi/player.pl:1-9`); add "leave your current expedition first" hail line to the nested DZ givers (15.4); use Claude's leanings for the borderline raid/named list (Section 13 #5) marked REVIEW for Ali; Barindu 4,100-hp 50% "nameds" left unflagged.

**Spells (correction to Section 0 #7 / 11.6):** PoK spell vendors were deliberately gated by an earlier patch (`asc_patch_pok_god_oow_spell_vendor_backup`, 346 rows / 18 merchants) to **min_expansion 8** (a few 9) — GoD spells are NOT buyable during GoD. Era-accurate path (eqprogression GoD spells guide) is **Reyfin Malakwa** (`abysmal/Reyfin_Malakwa.pl`, 6 hand-ins, 59 rewards) with Taelosian Geomancy Stones Eril 59975 / Yiktu 59974 / Jelki 59906 (53-55 NPCs each across 5-11 GoD zones, ~11-19%) and Chaos Runes 59907 (4 Qvic NPCs, 90%) for the Ancient: Chaos spells (59846-59857, tomes 59909-59919). The other "unsold" scrolls are non-GoD (guild-hall anchors, illusions, low-level). Nothing to change; GM-test one Reyfin turn-in.

**Quest chains verified present (NPC scripts + every component has a source):**
- Lodestar: Phosphorescent Bile 52111 (Cragbeast Queen) + Unlit Ornate Beacon 52112 (Uqua) -> combine 52122.
- Tome of New Beginnings 67606: 14 Tacvi "Writings" (67607-67620, each 1 lootdrop) -> `abysmal/#De-van_Szostek.lua` + 6 hand-in NPCs (#Sophia_Florence, #Verna_Abella, #Aprilia_Marrow, #Jenna_Gurell, #Gizzmu_the_Wise, #Irena_Pereira).
- Assistant Researcher's Symbol 67625: `abysmal/#Alexsa_Whyte.pl` (gives device 67594), Distilled Salts 67595 (6 Natimbi ground spawns), Tongue of the Tunat'muram 67591 (Tacvi final), Tongue of the Zun'muram 67592 (5 Zun`Muram bosses).
- Taskmistress Krisz items: Krisz drops 52116/52118/52119/52133; combines 10937 Handbag, 10938 Equifinis (+Tip 52117 from 2 Yxtta nameds), 10944/10949 Jade Figurine (+Sketch 52139, 14 droppers); Choker 52137 via `yxtta/Caridwi.lua` (Caridwi must stay un-flagged — already in the 583 un-flag set). `riwwi/#Ranguali.lua` is hint dialogue only (as on live).
- BiC: 15 scripts across GoD zones (bic_* qglobals); Shard of Dark Matter 67564-67566 drop. Pxet Elites (Smith Rondo) also feed BiC #4.
- Muramite class armor: direct raid drops per guide (Qvic/Inktu`Ta/Txevu; bones of Noqufiel / piles of bones) — covered by loot, no turn-in NPC.

## 21. eqprogression review, remaining pages (2026-09-26)

Pages read: Tipt/Vxed, Yxtta riddle, Yxtta raids (Primals/Mindrider), Uqua, Kod'Taz Summoning Circle, Cragbeast Queen, Ikkinz raid trials 1-3, Ikkinz group trials 1 and 3, Ferubi Packmaster/Votal, Loop of the Lizard Slayer, Lizardscale Plated Girdle, Visage of the Brute.

**Action items added**
- **Cragbeast Queen in instances (finishing boss):** spawn2 157415 has `_condition 1` (natimbi "CBQ", default 0); spawn_events 64 (1am game time -> 1) / 65 (3am -> 0); respawn 432000 s. Inside a Tyrael instance she would only appear 1-3am game time (~6 real min per 72). Add to `natimbi/player.pl` EVENT_ENTERZONE: `if ($instanceid > 0) { quest::spawn_condition("natimbi", $instanceid, 1, 1); }` (public zone unaffected). Note: `eq.toggle_spawn_event(65, ...)` in `#Cragbeast_Queen.lua` toggles the DB event row (shared with public natimbi) — GM-test that an instance kill doesn't leave the public DespawnCBQ event disabled. She is a key drop for 4 quests (Lodestar bile 52111, Loop jewel 52114, Girdle scales 52125, Visage flesh 52113).
- **Ikkinz raid trials 1-3 requester found:** `kodtaz/Tublik_Narwethar.pl:77-160` ("Sanctuary of the Righteous/Glorified/Transcendent") uses **legacy instances** (`quest::CreateInstance("ikkinz",3..5,21600)` + `AssignRaidToInstance`, qglobal lockouts `ikkylockout*`) — no expedition, so no nested-DZ conflict; no hail hint needed for Tublik. Finals: Trial 1 Spirit of Tunik Tamuik, Trial 2 Guardian of Glorification (splits at 50%), Trial 3 **Vrex Xalkak Nixki 294595** (3M), Trial 4 Keeper of the Altar 294620. Artifacts of Righteousness/Glorification/Transcendence 60170-60172 drop from piles of bones (verified).

**Verified present (items have sources, scripts exist)**
- Tipt/Vxed via `barindu/#Apprentice_Udranda.lua`; flags from Okkanu (Tipt) / Ekikoa (Vxed). Guide: either grants Kod'Taz; this server intentionally requires all three (owner decision, Section 20).
- Summoning Circle: Grand Summoner's Glyph 60151 <- #Deranged_Supreme_Stoneservant. Guide notes the event is "buggy" in open world (infinite golem respawn) — GM-test in a Tyrael raid instance.
- Loop of the Lizard Slayer 52121: Glowing Bile Drenched Jewel 52114 (CBQ) + Gemless Pristine Loop 52115 (Uqua Devastating Construct / Spectral Trushar Worshipper).
- Lizardscale Plated Girdle 52123 / Woven Sash 52124: `natimbi/Larsaki.lua`; Scales 52125 (CBQ), Untainted Spectral Essence 52128 (11 droppers), Polished Stonemite Shell 52127.
- Visage of the Brute 52120: Misshapen Cragbeast Flesh 52113 (CBQ) -> Caridwi (yxtta, riddle area; keep un-flagged).

**Mechanics noted (no banish/DT found beyond Sections 16-17)**
- Knockbacks (Fling/Throw/Cataclysm of Stone), feign-death strikes (Bury, Strike of Glory, Destructive Crush), mana/endurance drains, Uqua wrong-key Detonation (4,500 PBAE) — all non-lethal-by-design, kept.
- **Pixtt Suir Mindrider** (Yxtta) casts "Curse of the Ikaav" — unresistable **charm** + mana drain. Not banish/DT; owner may want it treated like one (FYI).

## 22. Implementation status (2026-09-26) — release = one script

**Applied (invisible until expansion 7; each has a rollback in server/sql/):**
| script | result |
|---|---|
| 2026_09_26_god_remove_cursebearer_death_touch.sql | npc_spells 1052 proc removed |
| 2026_09_26_god_remove_unresistable_charm.sql | Mindrider 2685 entry + 5023 proc removed (backup 2 rows) |
| 2026_09_26_god_named_flags.sql | raid_target 126 (new: Mass of Stone 281030, Kabeka Kret 281125, Smith_Rondo 284097), 555 un-flagged, 35 demoted, rare 155 total; backup 836 |
| 2026_09_26_god_planeshifter_tyrael_spawns.sql | 9 Tyrael spawns (spawn2 3266249-57 / spawngroup 3289047-55) |
| 2026_09_26_god_completion_rewards.sql | titles 427/428, item 4042 Charm of the Sixth Age |
| 2026_09_26_god_raid_ascendant_boost.sql | 271 T3 items; heroic avg 27.7/37.0/45.5/57.0 by block (backup + row dump) |

Quest/plugin changes: backup + README at `backup/god_prep_20260926/` (65 files + new ascendant_encounter_scaling_god.pl). Policy: no banish / death touch / unresistable charm. Leanings on the Section 13 borderline list are applied, marked for Ali's review.

Deviations from earlier sections: Ikkinz Custodian/Sentinel/Guardians kept raid (guide lists them as trial bosses); Ikkinz raid-trial minibosses demoted to rare instead of unflagged; tome translator unchanged (37/40 GoD-era tomes already present, 3 have no tome item; GoD-era AAs are in the "classic" bucket by owner schedule); HoT AA unlock limited to the 52 expansion-17 ranks of the 40 trainer abilities (1,207 native PEQ HoT ranks untouched).

**Needs a normal restart before release** (shared memory): charm 4042 and the T3 pass. Any restart between now and release covers it.

**Release day:**
1. Backup (`make mysql-backup` or table dump of rule_values/aa_ranks).
2. `server/sql/2026_09_26_god_gate_release.sql` — rule 6 -> 7, 52 HoT trainer ranks level_req -> 65 (expect 52 / 0).
3. `#reload content_flags global`, `#reload merchants global`, `#reload doors global` (or restart). Quest-side switches (Magus Wenla Abysmal option, Aurelian GoD era, HoT trainer bucket, Tyrael raid-only) key off `is_gates_of_discord_enabled()` and turn on automatically.
4. Verify: non-GM can take Magus Wenla (Nedaria) -> Abysmal Sea, Magus Pellen -> Natimbi; Tyrael in natimbi offers Normal + Raid, in qvic/txevu Raid only.
Rollback: `2026_09_26_rollback_god_gate_release.sql` + the same reloads.

**GM test checklist (can start now with #gm on; zones bypass the expansion check):**
- Tyrael placement in all 9 zones (not in walls/under floor).
- Natimbi Raid DZ: Cragbeast Queen spawns immediately (instance hook); kill -> completion bucket + SERVER FIRST emote; confirm public DespawnCBQ event not left disabled.
- Qinimi/Txevu/Qvic/Riwwi/Kod'Taz events inside a Raid DZ: no ejection to public zone (MovePCInstance), Txevu event adds appear in the instance (spawn_condition instance id), Zun`Muram Tkarish Zyk no longer jails.
- Inktu`Ta: Kelekdrix/Noqufiel no banish; Cursebearers no death touch; exiles no charm.
- Uqua gas chamber cheater ports to zone-in (no death); trinkets not in bonus loot.
- DZ givers inside a Tyrael DZ show the "Leave it first (/dzquit)" message.
- Aurelian: GoD option hidden before release; after release shows 6-boss status; 4038 -> 4042 swap.
- Insight trainer: HoT AAs appear only after rule 7 (level 65 purchasable).

## 23. Kill tracking + SERVER FIRSTs (2026-10-11)

`global/global_npc.pl` EVENT_KILLED_MERIT `%GOD_KILL_TRACKING` now covers **50 GoD raid encounters (62 NPC ids)**:
per-account `god_<key>_<account_id>` + `first_kill_<key>` = "Name|Guild" + world emote. Key convention matches the
82 Classic-PoP `first_kill_*` keys (no expansion prefix; no clashes). Final-kill NPCs from tacvi/inktuta zone_status.lua
and uqua scripts. Guild is captured via `quest::getguildnamebyid($client->GuildID())`.

Found while doing this: the existing Perl boss scripts use `$uguild`, which this build never exports (only
`$uguild_id`), so all 49 stored Perl-era first kills are "Name|" (empty guild) and their emotes printed "Name <>";
29 Lua-era values are bare "Name". LoY has no first-kill tracking at all.

## 24. Wayfarer magi -> Nedaria's Landing restored (2026-10-12, production)

Applied the test-server spec ("Restore Nedaria's Landing on the Wayfarer magi (gated by LoY)"), byte-identical to
Ascendant Test (all 9 post-edit checksums match): butcher/Magus_Tira, ecommons+commonlands/Magus_Zeir,
everfrost/Magus_Delin, nro+northro/Magus_Arindri, sro+southro/Magus_Jerira (Teleport: Nedaria 4580),
guildlobby/Magus_Alaria (MovePC to Nedaria next to Magus Wenla). Gate: `eq.is_the_legacy_of_ykesha_enabled()` or a GM
with #gm on. Live now (LoY is live). This is the overland route to Magus Wenla, i.e. into GoD at release.
Backup: backup/magi_nedaria_20261012/.

## 25. snlair seal hand-in no longer despawns player pets (2026-10-12, production)

Test-server spec applied byte-identically (checksum a25104af…): `snlair/encounters/tools.lua` `trigger_spawn` now
skips NPCs with an owner (pets, charmed mobs, familiars) and swarm pets when clearing the zone for the tool event.
Stock PEQ bug. Inert until expansion 7 (snlair is GoD). Backup: backup/snlair_pet_fix_20261012/.

## 23. TLP-style Kod'Taz access (2026-10-13, owner request)
Sewers no longer required; finishing Vxed **or** Tipt grants Kod'Taz. Edited (backups in `backup/god_access_tlp_20261013/`):
- `barindu/#Apprentice_Udranda.lua` — Vxed and Tipt DZs open to anyone (dropped the sewers/Vxed prerequisites); hail text updated.
- `vxed/#Stonespiritist_Ekikoa.lua` — always permanent flag, sets `god_tipt_access` + `god_kodtaz_access`; no temp flags, no 7-per-instance cap.
- `tipt/#Master_Stonespiritist_Okkanu.lua` — always permanent, sets `god_kodtaz_access`.
- `natimbi/player.pl` door 1 (NIHOBELISK800 -> kodtaz) — grants zone flag 293 if `god_kodtaz_access` or `god_tipt_access` qglobal, or a `<charid>-god_vxed` / `-god_tipt` bucket (incl. old "T" temp flags).
Rollback: copy the backups back. Takes effect as each zone boots (none were running at edit time).

## 24. Ikkinz solo fixes (2026-10-13, player reports)
- Research minimums on raid-trial key combines: 10355 Axtekilitek (100), 10356 Kruxilitek (100), 10896 Engraved Rune (200) -> `server/sql/2026_10_13_god_ikkinz_research_no_minimum.sql` (+ rollback). Recipes read per combine; no restart. Earlier "no Research needed" answer was wrong (only Tublik's Kod'Taz chain had been checked).
- `ikkinz/#Stalwart_Caretaker.lua` (raid trial 4, v6): hail gives Runic Inscription 60234 (live = pickpocket); death also opens doors 15/16; non-matching hand-ins returned. Backup in `backup/god_access_tlp_20261013/`.
