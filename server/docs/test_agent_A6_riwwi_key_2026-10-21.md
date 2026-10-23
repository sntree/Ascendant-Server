# Test agent instruction: A6, Riwwi Polished Silvery Key spawns are shared across all Riwwi zones (2026-10-21)

Paste everything below the line into the test agent session.

---

**Report:** #god-bug-reports "Polished Silvery Key - inconsistent spawning".
- A player parked 3 characters in Riwwi for hours (open world, a respawning DZ and a raid DZ), checking every 10-30 minutes with MQ ground search, and got only 2 keys in 24 hours.
- Another player did get one in a respawning instance.
- The key is Polished Silvery Key 52154 ("Fourth Manacle Key", Breakdown in Communication).

**Script:** `server/quests/riwwi/keymaster.lua`.
- It runs a 5s timer (`bic_check`). If no 52154 is on the ground in *this* zone and the cooldown isn't active, it drops the key at one of 5 random spots (decay 90 min) and starts the cooldown.
- The cooldown is `eq.set_data("riwwi_ground_spawn", "1", 91*60)`.

**Bug:** `COOLDOWN_KEY = "riwwi_ground_spawn"` is a single global data bucket.
- The ground check is per zone, but the cooldown is shared by open-world Riwwi **and every Riwwi instance**.
- So only one Riwwi zone on the whole server gets a key every 91 minutes: whichever zone's 5s check runs first after the bucket expires.
- On prod on 10-21 there was exactly one `riwwi_ground_spawn` bucket, and the zone logs show 23 Riwwi zone processes that day (instance 0, 136, 143, ...), all competing for it. That matches "2 keys in 24h".

**Fix:** scope the cooldown per instance, using the same pattern as `ferubi/Event_Spawner.lua` `inst_key()`:
```lua
local function cooldown_key()
  local instance_id = eq.get_zone_instance_id()
  if instance_id == 0 then return COOLDOWN_KEY end
  return COOLDOWN_KEY .. "-inst" .. instance_id
end
```
Use `cooldown_key()` in `is_cooldown_active()` and `start_cooldown()`.
- The open-world key name is unchanged, so prod's current bucket keeps working.
- Instance keys already expire (91 min), so nothing leaks into a reused instance id.
- While you're there, call `eq.get_data` once in `is_cooldown_active()`, not twice.

**Also check:**
- The keymaster NPC spawns in Riwwi DZ versions/instances too (both respawning and raid). If it doesn't spawn in some DZ type, report which.
- No other BiC ground-key script in GoD zones uses a global cooldown the same way. Grep `server/quests` for `set_data(` with a fixed key next to `create_ground_object`, and list any you find.

**Test on test:**
1. Open world plus two Riwwi instances running at once.
2. Each zone gets its own key within a few seconds of boot (no active cooldown).
3. Pick up the key in one instance. The others' keys stay. That instance's next key comes about 91 min later, with its own `riwwi_ground_spawn-inst<id>` bucket.

**Deliverable:**
- A backup of `riwwi/keymaster.lua` as `keymaster.lua.bak-20261021`.
- A unified diff, plus the result of the grep for other global ground-key cooldowns.
- In-game test notes.

**Rollout:** the owner copies the file to prod with the next update. It's a quest file, so it's live on the zone's next boot.
