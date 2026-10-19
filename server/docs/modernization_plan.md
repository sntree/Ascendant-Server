**Ascendant modernization plan — September 29, 2026**

Ascendant is a working production EQEmu server. The objective is to preserve its player experience while identifying measurable performance improvements and useful RoF2 features. This document proposes work; it does not authorize implementation or deployment. The owner will choose what to change.

The initial review covered architecture, deployment, selected engine paths, custom quests, database metadata, and selected RoF2 handlers. It was not an exhaustive code audit or an in-game compatibility test. Source findings, stored database settings, and a brief resource snapshot are available; representative performance benchmarks and staging validation are not.

Agents must first read [modernization_agent_brief.md](/opt/akk-stack/server/docs/modernization_agent_brief.md). Current authorization covers read-only assessment and planning documentation. Later explicit owner instructions supersede this recorded scope.

**The next deliverable is a decision packet.** Complete the three review assignments below and combine their findings into a ranked backlog. Each item must identify the player or operational benefit, evidence, uncertainty, implementation scope, acceptance checks, dependencies, and deployment impact. Do not start fixes while preparing it.

| Assignment | Owner role | Required output |
|---|---|---|
| A — Build and performance | Engine reviewer | Deployment identity gaps; current build configuration; performance candidates; reproducible benchmark design; test coverage gaps. |
| B — Custom systems | Quest and persistence reviewer | Existing feature inventory; duplicated work and database costs; candidate QoL improvements; behavior contracts for scaling, rewards, loot, AA, and pets. |
| C — RoF2 capabilities | Client compatibility reviewer | Feature matrix distinguishing implementation, configuration, content availability, and client limits; shortlist of useful additions. |
| Integration | Coordinator | Coverage ledger, consolidated backlog, recommended first pilot, unresolved product decisions, and proposed staging boundary. |

Run at most three review workers concurrently with one coordinator. Each has a defined scope and returns findings to the coordinator; the coordinator owns these planning documents. Avoid duplicate broad scans and repeated production database queries. Reviews may proceed concurrently; implementation dependencies must be respected.

**Coverage must be explicit.** Track the following areas with one of: not reviewed, indexed, sampled, deeply reviewed, or verified in staging. Record the inspected files/functions, reviewer, evidence, and remaining gaps. Reading filenames or running a search does not constitute a deep review.

| Area | Initial coverage | Remaining work |
|---|---|---|
| Build, dependencies, CI, release scripts | Sampled | Running binary/source identity; reproducible isolated build; prove test failure reporting. |
| World, login, zoning, reconnects | Sampled | State transitions, cross-process contracts, and recovery tests. |
| Combat, spells, AI, pathing, entities | Sampled | Representative workloads and preservation of custom formulas. |
| Inventory, trade, bazaar, parcels | Sampled | Persistence and concurrent purchase behavior; RoF2 presentation. |
| AA, pets, buffs, custom progression | Sampled | Client limits, prerequisite mappings, and regression scenarios. |
| Quests, loot, encounter scaling, rewards | Sampled | Complete custom-system inventory; query measurements; failure behavior. |
| Groups, raids, tasks, expeditions, guilds | Sampled unevenly | Workflow inventory and subsystem-specific review. |
| RoF2 packets, client assets, feature support | Sampled | Full feature matrix and tests against the actual distributed client. |
| Database, rules, migrations, backups, operations | Sampled | Effective per-zone rules; backup completeness and restore rehearsal; upgrade plan. |
| Mercenary behavior and other less-used features | Sampled narrowly | Desired behavior, usage, and implementation completeness. |

**The work sequence has separate approval boundaries.** Stages describe future work, not permission to run it now.

| Stage | Work and deliverable | Completion requirement |
|---|---|---|
| 1 — Review and prioritize | Complete assignments A–C. Identify source, binary, quest, schema, rule, map, dependency, and client-asset versions without disclosing credentials. Produce the decision packet. | Findings have evidence and confidence labels; coverage gaps remain visible; owner selects the next scope. |
| 2 — Establish isolation | On an approved separate environment, create independent source/build/install paths, world, database, volumes, accounts, ports, and client test setup. Rehearse restoration from an approved copy. | No production writable mounts, endpoints, credentials, login registration, automated jobs, or player traffic. Restored state and rollback artifacts are verified. |
| 3 — Make validation trustworthy | Reproduce the current build in isolation; repair test failure reporting; add targeted RoF2 and custom-system regression coverage. | Passing baseline; deliberate assertion failure produces nonzero exit; client/assets and baseline provenance recorded. Unresolved sanitizer findings are documented and cannot silently pass a gate. |
| 4 — Run one chosen pilot | Benchmark the optimized build, implement a small QoL improvement, or develop an approved feature specification. Keep each candidate separate. | Reviewable scoped diff or design; relevant before/after evidence; no unexplained gameplay or persistence changes; rollback procedure. |
| 5 — Decide on production release | Prepare exact artifacts, migrations, preconditions, rollout procedure, player impact, monitoring, and rollback limits. | Owner explicitly approves that production scope. Binary/database upgrades may require a maintenance window; zero disruption is not assumed. |
| 6 — Repeat selectively | Choose the next measured improvement or requested feature. Extract modules only where doing so supports the selected work. | Each release is independently assessable. Avoid combining database upgrades, upstream merges, balance changes, and feature additions. |

Stage 2 must be evaluated before starting any services. A copied configuration can still reach production or publish a second world to players. Disable production-directed webhooks, notification integrations, backup jobs, cron, launchers, and discovery/registration in the replica. Use synthetic accounts and minimize private data copied into it.

**Initial backlog entries are candidates, not confirmed live incidents.** Source references describe the reviewed checkout; revalidate them before implementation.

| ID | Candidate and evidence | Classification | First useful action |
|---|---|---|---|
| BASE-01 | Source has substantial uncommitted customization; running source-to-binary identity is not yet established. | Release prerequisite | Inventory versions and custom patches; preserve them in an approved isolated baseline. |
| TEST-01 | [tests/main.cpp:60](/opt/akk-stack/code/tests/main.cpp:60) ignores the suite result. | Confirmed source finding | Repair and demonstrate the failure gate in staging. |
| PERF-01 | [Build recipe:281](/opt/akk-stack/assets/scripts/Makefile:281) overrides normal optimized/debug flags; generated build flags confirm this. | Strong performance candidate, benefit unmeasured | Compare identical source/data with current versus deliberate optimized compiler flags. |
| PERF-02 | [Player timer:55](/opt/akk-stack/server/quests/global/global_player.pl:55) and [MySQL helper:10](/opt/akk-stack/server/quests/plugins/MySQL.pl:10) cause repeated polling/config reads/connections. | Source-supported recurring cost | Measure volume; design safe reuse or batching while preserving delivery latency and reconnection behavior. |
| PERF-03 | [Loot selection:661](/opt/akk-stack/server/quests/plugins/ascendant_loot_utils.pl:661) joins six tables and uses random ordering with retries. | Source-supported cost, workload impact unknown | Benchmark equivalent selection and cache candidates; preserve distribution and exclusions. |
| PERF-04 | [AI check:1036](/opt/akk-stack/code/zone/mob_ai.cpp:1036) calculates a route before cheap eligibility checks. | Narrow optimization candidate | Test cheap guards first; verify navigation behavior in representative zones. |
| COR-01 | [Scaling dispatch:568](/opt/akk-stack/server/quests/global/global_npc.pl:568) invokes implementations sharing state. | Likely behavior defect from static review, not reproduced | Reproduce cross-expansion rescans in staging; agree intended balance before a separate correction. |
| COR-02 | [Reward handler:229](/opt/akk-stack/server/quests/global/global_player.pl:229) awards before acknowledging the pending grant. | Persistence failure window, no incident established | Specify durable award/acknowledgement behavior and test failure/retry cases. |
| COMPAT-01 | [RoF2 decoder:5337](/opt/akk-stack/code/common/patches/rof2.cpp:5337) uses unbounded variable-string decoding. | Confirmed source concern, runtime impact untested | Design bounded decoding and an offline packet corpus; no production packet probing. |
| QOL-01 | Existing [AA browser](/opt/akk-stack/server/quests/plugins/ascendant_insight_trainer.pl:265), [guide](/opt/akk-stack/server/quests/guildlobby/Chronicler_Elodin.pl:101), and [expedition checks](/opt/akk-stack/server/quests/plugins/ascendant_expeditions.pl:98). | Extensions to existing systems | Rank AA search, accurate unified guidance, and expedition readiness previews by player value. |
| FEAT-01 | [Direct inventory delivery:15843](/opt/akk-stack/code/zone/client_packet.cpp:15843) always reaches an unimplemented response after its rule check. | Confirmed partial server feature | Write a purchase/delivery design for active traders; enabling the rule alone is insufficient. |
| FEAT-02 | Reviewed trader cleanup ends selling when the client leaves. | Larger offline-selling candidate | Separate feasibility study for persistent listings, reserved inventory, proceeds, and RoF2 UI support. |
| OPS-01 | Reviewed live MariaDB version was 10.5.4; buffer pool was 256 MiB. | Maintenance need; tuning benefit unmeasured | Plan a supported-version upgrade and workload-based tuning separately from gameplay changes. |
| OPS-02 | [Backup script:41](/opt/akk-stack/backup/backup-database.sh:41) disables table locks without explicitly selecting a consistent transaction; hourly table list comes from upstream. | Backup assurance gap | Verify effective dump options, custom-table coverage, failure reporting, and restore consistency in isolation. |

Do not automatically turn every finding into a change. Select by player/operational value, evidence, effort, compatibility risk, and ability to test. Review confirmed correctness concerns independently from performance experiments so intentional behavior changes remain visible.

**The proposed first performance pilot is PERF-01.** It can compare compiler configurations without redesigning gameplay. The proposed first player-facing pilot is a narrowly scoped part of QOL-01. FEAT-01 is the clearest unfinished native request path, but its transaction design makes it a larger commitment than a guide or search improvement. The owner chooses the pilot after seeing its brief.

**Performance acceptance uses a reproducible baseline.** Run the same source/data, workload, hardware, client build, and settings for comparisons; change one factor at a time. Include warm-up and repeated runs. Capture zone-loop execution time and scheduling delay at p50/p95/p99, CPU, memory trend, database query count/time, zoning latency, and packet volume. Cover idle, hub, group/raid combat, spawning, zoning, trading, and reconnect scenarios as relevant. Set numerical targets after measuring baseline variability. A change must show repeatable benefit beyond that variability with no unexplained behavior regression.

Do not equate a configured 32 ms zone timer with achieved latency. Use deterministic randomness or appropriate distribution comparisons for combat and loot. Inventory/currency conservation and durable persistence are correctness requirements, not performance tradeoffs. Production instrumentation or heavier monitoring requires its own approved scope.

**RoF2 acceptance covers the actual distributed client.** Record the executable build and required client data assets. Test login/character selection, repeated zoning, reconnect recovery, AA purchase/prerequisites/profile limits, spells/buffs, pet controls, full bags/cursor/bandolier/augments, loot/corpses, trades/bazaar/parcels, groups/raids, expeditions/lockouts, and persistence after relog as applicable. Preserve packed packet layouts and slot translations; internal refactors stay behind those boundaries. Do not claim in-game validation from source review alone.

**Feature classifications require separate evidence.** Parcels, barter, base mercenaries, evolving items, shared tasks, custom expeditions, pet bags, fellowship bonuses, and custom era achievements already have local implementations. Several have enabled stored default rules or populated definitions; those observations do not prove all running zones or content paths expose them. Direct inventory delivery is partial. Offline selling is a separate research/design item. Native housing, achievement/collection windows, and later-client interfaces require client/protocol research before estimating implementation. Task collection objectives and custom achievements do not prove native achievement-window support.

For FEAT-01, scope the first design to existing active traders. Specify pricing/vouchers, restrictions, capacity/lore checks, inventory placement, purchase identity, persistence, client acknowledgements, and recovery. Define whether a full inventory rejects or uses an explicitly chosen fallback. Test synthetic concurrent buyers, stale listings/prices, insufficient funds, invalid quantities, duplicate requests, disconnects, and failures around persistence. Each outcome must preserve exactly one transfer/payment or no purchase. Existing direct and parcel purchasing must remain correct. Offline selling and new economics are separate decisions.

**Every implementation proposal must be reviewable before deployment.** Include the exact diff/artifacts, acceptance evidence, unresolved risks, expected player impact, deployment steps, and rollback constraints. A database restore can discard new player progress; it is not an automatic rollback after players resume. Prefer compatible migrations and explicit recovery plans. Do not execute rollout or rollback as part of preparing the proposal.

Reference material: [EQEmu rules](https://docs.eqemu.dev/server/operation/server-rules/), [RoF2 build identification](https://docs.eqemu.dev/play/play-guide/), [evolving items](https://docs.eqemu.dev/server/items/evolving/), [MariaDB maintenance policy](https://mariadb.org/about/), and [transactional dump options](https://mariadb.com/docs/server/clients-and-utilities/backup-restore-and-import-clients/mariadb-dump#--single-transaction). Current upstream documentation may differ from this customized checkout; source and effective state must be checked.
