local BIC_ITEM_ID        = 52154
local RESPAWN_SECONDS    = 45 * 60 -- Ascendant: was 91 minutes
local CHECK_INTERVAL_MS  = 5 * 1000
local TIMER_NAME         = "bic_check"
local COOLDOWN_KEY       = "riwwi_ground_spawn"

-- {x, y, z, heading, decay_ms}
local SPAWN_LOCS = {
  {-1,   -201,   1,  0, 5400000},
  {-129,  625,  71,  0, 5400000},
  {-736, -327,   0,  0, 5400000},
  {-172, -233,  18,  0, 5400000},
  {-678,  495, -36,  0, 5400000},
}

local function is_item_on_ground(item_id)
  local list = eq.get_entity_list():GetObjectList()

  if not list or not list.entries then
    return false
  end

  for obj in list.entries do
    if obj.valid and obj:GetItemID() == item_id then
      return true
    end
  end

  return false
end

local function spawn_random_ground_item(item_id)
  local idx = math.random(#SPAWN_LOCS)
  eq.create_ground_object(item_id, unpack(SPAWN_LOCS[idx]))
end

-- Ascendant: one cooldown per instance (like ferubi/Event_Spawner.lua), so open world and every DZ each get a key
local function cooldown_key()
  local instance_id = eq.get_zone_instance_id()
  if instance_id == 0 then return COOLDOWN_KEY end
  return COOLDOWN_KEY .. "-inst" .. instance_id
end

local function is_cooldown_active()
  local value = eq.get_data(cooldown_key())
  if value == nil or value == '' then
    return false
  else
    return true
  end
end

local function start_cooldown()
  eq.set_data(cooldown_key(), "1", tostring(RESPAWN_SECONDS))
end

function event_spawn(e)
  eq.set_timer(TIMER_NAME, CHECK_INTERVAL_MS)
end

function event_timer(e)
  local item_present = is_item_on_ground(BIC_ITEM_ID)

  if not item_present and not is_cooldown_active() then
    spawn_random_ground_item(BIC_ITEM_ID)
    start_cooldown()
  end
end
