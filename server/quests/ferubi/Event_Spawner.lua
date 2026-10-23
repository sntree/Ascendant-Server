-- Ascendant: scope event state per instance (public zone keeps its original key)
local function inst_key(key)
	local instance_id = eq.get_zone_instance_id();
	if instance_id == 0 then return key end
	return key .. "-inst" .. instance_id;
end

-- Ascendant: a DZ clear has only 66/46/43/30 camp spawns and no respawns, so 100 kills never happens there;
-- open world was 100, lowered to 30 (about one clear of the smallest camp) for solo players
local function need()
	return (eq.get_zone_instance_id() == 0) and 30 or 20;
end

-- Ascendant: instance keys expire with the instance so a reused instance id starts clean
local function save_count(key, count)
	if eq.get_zone_instance_id() == 0 then
		eq.set_data(key, tostring(count));
		return;
	end
	local remaining = math.floor(eq.get_instance_timer() / 1000);
	eq.set_data(inst_key(key), tostring(count), remaining > 0 and tostring(remaining) or "24h");
end

local blademaster_counter = 0;
local spearmaster_counter = 0;
local hammermaster_counter = 0;
local brawler_counter = 0;

function event_spawn(e)
	blademaster_counter		= tonumber(eq.get_data(inst_key("ferubi-blademaster-count"))) or 0;
	spearmaster_counter		= tonumber(eq.get_data(inst_key("ferubi-spearmaster-count"))) or 0;
	hammermaster_counter	= tonumber(eq.get_data(inst_key("ferubi-hammermaster-count"))) or 0;
	brawler_counter			= tonumber(eq.get_data(inst_key("ferubi-brawler-count"))) or 0;
end

function event_signal(e)
	if e.signal == 1 then
		blademaster_counter = blademaster_counter + 1;
		save_count("ferubi-blademaster-count", blademaster_counter);
		-- eq.GM_Message(MT.LightBlue, "[DEBUG] blademaster spawn count = [".. blademaster_counter .."]");
		-- eq.debug("blademaster spawn count: " ..  blademaster_counter);
		
		if blademaster_counter >= need() then
			eq.unique_spawn(284039,0,0,443,428,102.5,0); -- Pxet_Elite_Blademaster (284039)
			eq.set_timer("resetblade", 3 * 1000); --3 sec reset
		end
	elseif e.signal == 2 then
		spearmaster_counter = spearmaster_counter + 1;
		save_count("ferubi-spearmaster-count", spearmaster_counter);
		-- eq.GM_Message(MT.LightBlue, "[DEBUG] spearmaster spawn count = [".. spearmaster_counter .."]");
		-- eq.debug("spearmaster spawn count: " ..  spearmaster_counter);
		if spearmaster_counter >= need() then
			eq.unique_spawn(284091,0,0,-771,354,98,240); -- Pxet_Elite_Spearmaster (284091)
			eq.set_timer("resetspear", 3 * 1000); --3 sec reset
		end
	elseif e.signal == 3 then
		hammermaster_counter = hammermaster_counter + 1;
		save_count("ferubi-hammermaster-count", hammermaster_counter);
		-- eq.GM_Message(MT.LightBlue, "[DEBUG] hammermaster spawn count = [".. hammermaster_counter .."]");
		-- eq.debug("hammermaster spawn count: " ..  hammermaster_counter);
		if hammermaster_counter >= need() then
			eq.unique_spawn(284090,0,0,570,-486,4,404); -- Pxet_Elite_Hammermaster (284090)
			eq.set_timer("resethammer", 3 * 1000); --3 sec reset
		end
	elseif e.signal == 4 then
		brawler_counter = brawler_counter + 1;
		save_count("ferubi-brawler-count", brawler_counter);
		-- eq.GM_Message(MT.LightBlue, "[DEBUG] brawler spawn count = [".. brawler_counter .."]");
		-- eq.debug("brawler spawn count: " ..  brawler_counter);
		if brawler_counter >= need() then
			eq.unique_spawn(284088,0,0,-723,-643,27,508); -- Pxet_Elite_Brawler (284088)
			eq.set_timer("resetbrawler", 3 * 1000); --3 sec reset
		end
	end
end

function event_timer(e)
	if e.timer == "resetblade" then
		blademaster_counter = 0;
		eq.stop_timer("resetblade");
		save_count("ferubi-blademaster-count", blademaster_counter);
	elseif e.timer == "resetspear" then
		spearmaster_counter = 0;
		eq.stop_timer("resetspear");
		save_count("ferubi-spearmaster-count", spearmaster_counter);
	elseif e.timer == "resethammer" then
		hammermaster_counter = 0;
		eq.stop_timer("resethammer");
		save_count("ferubi-hammermaster-count", hammermaster_counter);
	elseif e.timer == "resetbrawler" then
		brawler_counter = 0;
		eq.stop_timer("resetbrawler");
		save_count("ferubi-brawler-count", brawler_counter);
	end
end
