-- Plane of Time B phase 3 pathing safety.
-- P3 trash can path below the platform when aggroed from underneath; keep
-- their guard spots above the floor and reset under-floor pulls.

local P3_FLOOR_Z = 361.0;
local P3_MIN_Z = 355.0;
local P3_PATHING_TIMER = "p3_pathing_safety";

local p3_trash_ids = {
	223005, 223006,
	223026, 223027, 223028, 223029, 223030,
	223033, 223034, 223035, 223036,
	223039, 223040, 223041, 223042, 223043, 223044, 223045,
	223048, 223049,
	223052, 223053, 223054, 223055, 223056,
	223059, 223060, 223061, 223062, 223063, 223064,
	223067, 223068, 223069, 223070, 223071, 223072
};

local function safe_spawn_z(e)
	local spawn_z = e.self:GetSpawnPointZ();
	if spawn_z < P3_FLOOR_Z then
		return P3_FLOOR_Z;
	end

	return spawn_z;
end

local function move_home(e)
	e.self:GMMove(
		e.self:GetSpawnPointX(),
		e.self:GetSpawnPointY(),
		safe_spawn_z(e),
		e.self:GetSpawnPointH(),
		true
	);
end

local function reset_under_floor_pull(e)
	e.self:WipeHateList();
	move_home(e);
	e.self:StopTimer(P3_PATHING_TIMER);
end

function P3_Trash_Spawn(e)
	move_home(e);
end

function P3_Trash_Combat(e)
	if e.joined then
		e.self:SetTimerMS(P3_PATHING_TIMER, 2000);
	else
		e.self:StopTimer(P3_PATHING_TIMER);
		move_home(e);
	end
end

function P3_Trash_Timer(e)
	if e.timer ~= P3_PATHING_TIMER then
		return;
	end

	if not e.self:IsEngaged() then
		e.self:StopTimer(P3_PATHING_TIMER);
		move_home(e);
		return;
	end

	-- Only reset when the MOB itself has been dragged under the floor.
	-- Do NOT key off the top-hate player's Z: legitimate raiders can be
	-- below P3_MIN_Z (lower areas, knockback, pets, a straggler in the
	-- phase 2 area), which would otherwise wipe hate and heal the mob to
	-- full every 2 seconds and brick the encounter.
	if e.self:GetZ() < P3_MIN_Z then
		reset_under_floor_pull(e);
	end
end

function event_encounter_load(e)
	for _, npc_id in ipairs(p3_trash_ids) do
		eq.register_npc_event("p3_pathing", Event.spawn, npc_id, P3_Trash_Spawn);
		eq.register_npc_event("p3_pathing", Event.combat, npc_id, P3_Trash_Combat);
		eq.register_npc_event("p3_pathing", Event.timer, npc_id, P3_Trash_Timer);
	end
end
