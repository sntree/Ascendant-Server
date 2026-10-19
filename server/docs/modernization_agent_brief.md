**Agent brief for Ascendant improvement planning**

Read [modernization_plan.md](/opt/akk-stack/server/docs/modernization_plan.md) before beginning. Ascendant is a live production EverQuest emulator hosting players. The owner says it is working well and wants a plan for performance, maintainability, and RoF2 feature improvements. Preserve the current experience. This brief records current authorization; newer explicit owner instructions take precedence.

**Current mode is assessment and planning only.** The owner has not authorized implementation, feature activation, deployment, maintenance, or service interruption. You may inspect source, existing documentation, version metadata, and bounded low-cost operational information. Planning documentation is within scope. A delegated reviewer returns findings to the coordinator unless assigned a specific documentation file.

The workspace contains live deployment paths, not an ordinary disposable development checkout:

| Host path | Purpose |
|---|---|
| `/opt/akk-stack/code` | Customized C++ source and build outputs. |
| `/opt/akk-stack/server` | Running server assets, configuration, binaries, quests, and data files. |
| `/opt/akk-stack/server/quests` | Custom and inherited Perl/Lua quest behavior. |
| `/opt/akk-stack/code_updates` | Historical custom change packages; presence does not prove application. |
| `/opt/akk-stack/server/docs` | Project documentation and this plan. |
| `/opt/akk-stack/containers`, `/opt/akk-stack/assets`, `/opt/akk-stack/backup` | Deployment, build, and backup tooling; read scripts before considering execution. |

Production mounts map host `code` and `server` into `/home/eqemu/code` and `/home/eqemu/server` in the EQEmu container. Some deployed binaries and opcode files are symlinks into the source/build tree. A local build or asset edit can therefore affect production deployment state.

During the current review:

- Do not edit source, quests, SQL migrations, configuration, rules, client assets, binaries, shared memory, or database content. Do not stage/commit/reset/stash/pull/rebase the live checkout.
- Do not start, stop, restart, reload, repopulate, kick, signal, attach a debugger/profiler, or send test traffic to production services.
- Do not run CMake, builds, tests, server executables, import/export tools, management helpers, migrations, backups, or restore jobs on production. A command name such as “verify,” “test,” or “status” is not proof that a script is harmless.
- Do not use production-mounted paths as a test environment. A second container alone does not establish isolation.
- Prefer bounded source reads and searches. For Git inspection, use `GIT_OPTIONAL_LOCKS=0`. Preserve uncommitted and untracked custom work.
- Keep database inspection narrow. Prefer metadata and indexed, bounded reads. Do not use write statements, locking reads, `EXPLAIN ANALYZE`/`ANALYZE`, large scans, load generators, or configuration changes during assessment.
- Read-only does not mean cost-free. Avoid duplicating broad scans across agents or collecting player records when aggregate metadata suffices.
- Do not print secrets, full environment/configuration dumps, private account/player information, or raw credentials into reports. Use allowlisted fields and redaction.
- If required evidence would cross these boundaries, document what remains unverified and the proposed isolated method. Continue independent authorized review work; do not perform the dependent action.

**Evaluate improvements, not age alone.** The core already uses C++20 and has substantial working custom behavior. Do not assume a rewrite, language migration, upstream replacement, broad multithreading, or feature removal is beneficial. Identify the smallest change that achieves a concrete benefit. Existing safety fixes and custom balance are part of the baseline.

For each finding, report:

1. Stable ID and concise title.
2. Current behavior and why it matters to players or operators.
3. Evidence: absolute file path and line/function, or exact read-only query and observation date. Identify source, stored database setting, running state, or staging test evidence separately.
4. Confidence: confirmed source fact, likely issue, measured cost, or unverified hypothesis. State what was not tested. Do not present a source concern as an observed player incident.
5. Smallest proposed change, dependencies, alternatives, and rough effort with uncertainty.
6. Test/benchmark method and acceptance criteria, including preserved gameplay and persistence behavior.
7. Production impact and rollback considerations. Identify whether maintenance, rules, schema, quest reload, or client assets would be involved.
8. Remaining decision or next action. Do not quietly implement it.

**Track coverage honestly.** Use the plan's coverage ledger. Record which subsystem paths were actually read and the remaining gaps. “No match found in these files” is not proof that a feature is absent everywhere. Do not claim an exhaustive review or numerical speedup without supporting work.

**Classify features on both sides of the connection.** Distinguish client capability, packet/handler implementation, data/content availability, stored rules, and verified runtime behavior. An opcode, configuration rule, database row, or UI element alone is insufficient proof of a functioning feature. A stored default rule does not prove the current in-memory setting in every zone. Some requested modern features require a custom interface or client changes beyond this repository.

Do not propose basic parcels, barter, mercenaries, pet bags, fellowship bonuses, custom expeditions, AA purchase confirmations, or custom era achievements as new without first checking their current implementation. The direct-inventory bazaar path is a useful counterexample: its rule exists, but the reviewed handler explicitly reports that delivery is not implemented.

**Implementation begins only within a selected, authorized scope.** Once the owner approves a work package, carry that approval forward; do not repeatedly ask about routine work already covered. Establish and verify the approved staging boundary before builds, tests, migrations, failure injection, or gameplay experiments. Keep structural refactors, optimizations, balance changes, and new features separately reviewable. Use small patches and targeted tests that verify meaningful behavior.

Preparing an implementation does not authorize production deployment. Finish the scoped change, evidence, and rollback plan in isolation first, then present the concrete result for the owner's production decision. Never infer that a hot reload is harmless: the reviewed stored default rules enable repopulation and timer resets on quest reload.

**Copyable review assignment**

```text
Read /opt/akk-stack/server/docs/modernization_agent_brief.md and
/opt/akk-stack/server/docs/modernization_plan.md.

Assignment: [A — Build and performance / B — Custom systems / C — RoF2 capabilities].
Scope: [specific subsystem or backlog IDs].

Perform read-only assessment. This is a working production server with connected
players. No implementation, builds, tests, service actions, reloads, database
writes, or production probing are authorized. Return findings to the coordinator;
do not edit shared planning files unless assigned their ownership.

Deliver evidence-backed findings, explicit review coverage, a ranked shortlist,
and a staging validation plan. Separate measured problems from hypotheses and
new features from already implemented systems. Preserve existing custom gameplay.
The owner will choose what to implement after reviewing the decision packet.
```
