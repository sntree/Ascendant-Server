--[[
 Bertox Event ## By Drogerin
--]]
local charm_credit = require("charm_credit");

local trash_dead			= 0;
local wave_trash_dead		= 0;
local forced_trash_dead		= 0;
local trash_wave_size		= 30;
local event_active			= false;
local trash_wave_active		= false;
local trash_watchdog_dead	= 0;
local trash_watchdog_ticks	= 0;
local raddi_spawned			= false;
local wavadozzik_spawned	= false;
local zandal_spawned		= false;
local akkapan_spawned		= false;
local bhaly_spawned			= false;
local pzo_spawned			= false;
local meedo_spawned			= false;
local qezzin_spawned		= false;
local bert_spawned			= false;
local darwol_spawned		= false;
local feig_spawned			= false;
local xhut_spawned			= false;
local kavillis_spawned		= false;

local trash_ids = { 200042, 200043, 200062, 200063, 200064, 200065 };
local first_adans = { 200046, 200045, 200044, 200047 };
local second_adans = { 200049, 200048, 200050, 200051 };
local final_adans = { 200054, 200053, 200052, 200022 };
local event_timers = {
	"bert_trash_watchdog",
	"force_darwol",
	"force_feig",
	"force_xhut",
	"force_kavillis",
	"force_raddi",
	"force_wavadozzik",
	"force_zandal",
	"force_akkapan",
	"force_final_adans",
};
local forced_progress = {
	force_darwol		= 42,
	force_feig			= 46,
	force_xhut			= 50,
	force_kavillis		= 54,
	force_raddi			= 108,
	force_wavadozzik	= 112,
	force_zandal		= 116,
	force_akkapan		= 120,
	force_final_adans	= 125,
};

function AnySpawned(npc_ids)
	local el = eq.get_entity_list();
	for _, npc_id in ipairs(npc_ids) do
		if el:IsMobSpawnedByNpcTypeID(npc_id) then
			return true;
		end
	end

	return false;
end

function StartTrashWave()
	wave_trash_dead = 0;
	trash_watchdog_dead = trash_dead;
	trash_watchdog_ticks = 0;
	trash_wave_active = true;

	eq.spawn_condition("codecay", eq.get_zone_instance_id(), 1, 0);
	eq.spawn_condition("codecay", eq.get_zone_instance_id(), 1, 1);
end

function DepopTrash()
	for _, npc_id in ipairs(trash_ids) do
		eq.depop_all(npc_id);
	end
end

function StopTrashWave()
	trash_wave_active = false;
	eq.spawn_condition("codecay", eq.get_zone_instance_id(), 1, 0);
end

function StopEventTimers()
	for _, timer_name in ipairs(event_timers) do
		eq.stop_timer(timer_name);
	end
end

function StartEventTimers()
	StopEventTimers();
	eq.set_timer("bert_trash_watchdog", 60000);
	eq.set_timer("force_darwol", 600000);
	eq.set_timer("force_feig", 660000);
	eq.set_timer("force_xhut", 720000);
	eq.set_timer("force_kavillis", 780000);
	eq.set_timer("force_raddi", 1320000);
	eq.set_timer("force_wavadozzik", 1380000);
	eq.set_timer("force_zandal", 1440000);
	eq.set_timer("force_akkapan", 1500000);
	eq.set_timer("force_final_adans", 2160000);
end

function ForceProgress(required_trash_dead)
	if forced_trash_dead < required_trash_dead then
		forced_trash_dead = required_trash_dead;
	end

	CheckProgress();
end

function ShouldDelayForceTimer(timer_name)
	if timer_name == "force_feig" then
		return not darwol_spawned;
	elseif timer_name == "force_xhut" then
		return not feig_spawned;
	elseif timer_name == "force_kavillis" then
		return not xhut_spawned;
	elseif timer_name == "force_raddi" then
		return AnySpawned(first_adans);
	elseif timer_name == "force_wavadozzik" then
		return AnySpawned(first_adans) or not raddi_spawned;
	elseif timer_name == "force_zandal" then
		return AnySpawned(first_adans) or not wavadozzik_spawned;
	elseif timer_name == "force_akkapan" then
		return AnySpawned(first_adans) or not zandal_spawned;
	elseif timer_name == "force_final_adans" then
		return AnySpawned(first_adans) or AnySpawned(second_adans) or not akkapan_spawned;
	end

	return false;
end

function SpawnBertoxxulous()
	if bert_spawned then
		return;
	end

	eq.signal(200056, 9000); -- NPC: Summoner_of_Bertoxxulous
	StopEventTimers();
	StopTrashWave();
	eq.spawn2(200055,0,0,-61.75,-.22,-288.5,384.8); -- Summon Bertoxxulous.
	eq.zone_emote(MT.NPCQuestSay,"A sinister vision enters your mind of a faceless one handsome yet dead and decaying. The vision then shifts to that of a torn bestial creature and a loud shout is heard, 'Defilers death comes for you today!'")
	bert_spawned = true;
end

function CheckProgress()
	if not event_active then
		return;
	end

	local progress_dead = trash_dead;
	if forced_trash_dead > progress_dead then
		progress_dead = forced_trash_dead;
	end

	if progress_dead >= 42 and not darwol_spawned then		--42 Trash
		eq.zone_emote(MT.NPCQuestSay,"An unsettling feeling of fear passes through you as you hear the summoners finish a dark incantation then cry out saying, 'We call to you corrupted King of Lxanvom, Darwol Adan, your master has need of you!' A bestial squeak thunders through the crypt as a foul fiend of Bertoxxulous is summoned forth.");
		eq.spawn2(200046,0,0,-3.09,280.74,-245.20,255.5); -- NPC: #Darwol_Adan
		darwol_spawned=true;
	end

	if progress_dead >= 46 and not feig_spawned then	--46 Trash
		eq.spawn2(200045,0,0,-203.46,0.68,-275.82,128.8); -- Spawn Feig Adan
		eq.zone_emote(MT.NPCQuestSay,"An unsettling feeling of fear passes through you as you hear the summoners finish a dark incantation then cry out saying, 'We call to you corrupted King of Lxanvom, Feig Adan, your master has need of you!' A bestial squeak thunders through the crypt as a foul fiend of Bertoxxulous is summoned forth.");
		feig_spawned=true;
	end

	if progress_dead >= 50 and not xhut_spawned then	--50 Trash
		eq.spawn2(200044,0,0,-1.24,-280.37,-245.82,511.2); -- Spawn Xhut Adan
		eq.zone_emote(MT.NPCQuestSay,"An unsettling feeling of fear passes through you as you hear the summoners finish a dark incantation then cry out saying, 'We call to you corrupted King of Lxanvom, Xhut Adan, your master has need of you!' A dark vision flashes through the crypt as a foul fiend of Bertoxxulous is summoned forth.");
		xhut_spawned=true;
	end

	if progress_dead >= 54 and not kavillis_spawned then	--54 Trash
		eq.spawn2(200047,0,0,203.03,1.63,-275.82,381.8); -- Spawn Kavilis Adan
		eq.zone_emote(MT.NPCQuestSay,"An unsettling feeling of fear passes through you as you hear the summoners finish a dark incantation then cry out saying, 'We call to you corrupted King of Lxanvom, Kavilis Adan, your master has need of you!' A faint buzzing is heard through the crypt as a foul fiend of Bertoxxulous is summoned forth.");
		kavillis_spawned=true;
	end

	if not AnySpawned(first_adans) and progress_dead >= 108 and not raddi_spawned then -- 108 trash
		eq.spawn2(200049,0,0,-2.79,278.72,-245.82,259.5); --  Spawn Raddi Adan but only if none of the first 4 kings are spawned.
		eq.zone_emote(MT.NPCQuestSay,"An unsettling feeling of fear passes through you as you hear the summoners finish a dark incantation then cry out saying, 'We call to you corrupted King of Lxanvom,  Raddi Adan, your master has need of you!' A wailing cry echoes through the crypt as a foul fiend of Bertoxxulous is summoned forth.");
		raddi_spawned=true;
	end

	if not AnySpawned(first_adans) and progress_dead >= 112 and not wavadozzik_spawned then --112 trash
		eq.spawn2(200048,0,0,-203.46,0.68,-275.82,128.8); -- Spawn Wavadozzik Adain, same conditions as Raddi
		eq.zone_emote(MT.NPCQuestSay,"An unsettling feeling of fear passes through you as you hear the summoners finish a dark incantation then cry out saying, 'We call to you corrupted King of Lxanvom, Wavadozzik Adan, your master has need of you!' Chittering is heard through the crypt as a foul fiend of Bertoxxulous is summoned forth.");
		wavadozzik_spawned=true;
	end

	if not AnySpawned(first_adans) and progress_dead >= 116 and not zandal_spawned then --116 trash
		eq.spawn2(200050,0,0,-1.24,-280.37,-245.82,511.2); -- Spawn Zandal Adan, same conditions as Raddi & Wavadozzik
		eq.zone_emote(MT.NPCQuestSay,"An unsettling feeling of fear passes through you as you hear the summoners finish a dark incantation then cry out saying, 'We call to you corrupted King of Lxanvom, Zandal Adan, your master has need of you!' Chittering is heard through the crypt as a foul fiend of Bertoxxulous is summoned forth.");
		zandal_spawned=true;
	end

	if not AnySpawned(first_adans) and progress_dead >= 120 and not akkapan_spawned then
		eq.spawn2(200051,0,0,203.03,1.63,-275.82,381.8); -- Spawn Akkapan Adan, same conditions as Raddi, Wavadozzik& Zandal
		eq.zone_emote(MT.NPCQuestSay,"An unsettling feeling of fear passes through you as you hear the summoners finish a dark incantation then cry out saying, 'We call to you corrupted King of Lxanvom, Akkapan Adan, your master has need of you!' A maddened whispering is heard through the crypt as a foul fiend of Bertoxxulous is summoned forth.");
		akkapan_spawned=true;
	end

	if not AnySpawned(first_adans) and not AnySpawned(second_adans) and progress_dead >=125 and not bhaly_spawned and not pzo_spawned and not qezzin_spawned and not meedo_spawned then
		StopTrashWave();
		eq.spawn2(200054,0,0,203.03,1.63,-275.82,381.8); -- Bhaly West
		eq.spawn2(200053,0,0,-2.79,278.72,-245.82,259.5); -- Meedo North Spawn the final 4 Kings all at once if at 125 kills or more & all previous kings are dead.
		eq.spawn2(200052,0,0,-203.46,0.68,-275.82,128.8); -- Qezzin East
		eq.spawn2(200022,0,0,-1.24,-280.37,-245.82,511.2); -- Pzo South
		eq.zone_emote(MT.NPCQuestSay,"An unsettling feeling of fear passes through you as you hear the summoners finish a dark incantation then cry out saying, 'We call to you the last corrupted Kings of Lxanvom. Meedo Adan! Qezzin Adan! Pzo Adan! Bhaly Adan! Your master has need of you!' Four separate howls of rage and despair echo throughout the lower depths of the crypt as four foul fiends of Bertoxxulous are summoned forth.");
		qezzin_spawned = true;
		meedo_spawned = true;
		pzo_spawned = true;
		bhaly_spawned = true;
	end

	if progress_dead >= 125 and bhaly_spawned and pzo_spawned and qezzin_spawned and meedo_spawned and not AnySpawned(final_adans) then
		SpawnBertoxxulous();
	end
end

function MaybeStartNextTrashWave()
	if not event_active or bert_spawned then
		return;
	end

	CheckProgress();

	if bert_spawned or AnySpawned(trash_ids) or AnySpawned(first_adans) or AnySpawned(second_adans) or AnySpawned(final_adans) then
		return;
	end

	if trash_dead < 125 and forced_trash_dead < 125 then
		StartTrashWave();
	end
end

function CheckTrashWatchdog()
	if not event_active or bert_spawned then
		return;
	end

	if AnySpawned(first_adans) or AnySpawned(second_adans) or AnySpawned(final_adans) then
		return;
	end

	if not AnySpawned(trash_ids) then
		trash_watchdog_dead = trash_dead;
		trash_watchdog_ticks = 0;
		MaybeStartNextTrashWave();
		return;
	end

	if trash_dead == trash_watchdog_dead then
		trash_watchdog_ticks = trash_watchdog_ticks + 1;
	else
		trash_watchdog_dead = trash_dead;
		trash_watchdog_ticks = 0;
	end

	if trash_watchdog_ticks >= 8 then
		eq.zone_emote(MT.NPCQuestSay,"The chants of decay falter, then begin anew as the failed summoning collapses into dust.");
		StopTrashWave();
		DepopTrash();
		if forced_trash_dead < 125 then
			StartTrashWave();
		end
	end
end


function Spectre_Death(e)
	trash_dead=0;
	wave_trash_dead=0;
	forced_trash_dead=0;
	event_active=true;
	trash_wave_active=true;
	trash_watchdog_dead=0;
	trash_watchdog_ticks=0;
	raddi_spawned=false;
	wavadozzik_spawned=false;
	zandal_spawned=false;
	akkapan_spawned=false;
	bhaly_spawned=false;
	pzo_spawned=false;
	meedo_spawned=false;
	qezzin_spawned=false;
	bert_spawned=false;
	darwol_spawned=false;
	feig_spawned=false;
	xhut_spawned=false;
	kavillis_spawned=false;
	eq.zone_emote(MT.NPCQuestSay,"Crazed laughter is heard as you notice a foul creature standing before you. The creature then speaks saying, 'Violaters of the depths of Lxanvom shall pay with your lives!'  The foul minion of decay then begins chanting a dark ritual.  Deeper within the depths of the crypt more chanting can be heard.");
	StartEventTimers();
	eq.signal(200056, 1); -- NPC: Summoner_of_Bertoxxulous
end


function Trash_Death(e)
	trash_dead=trash_dead+1;
	wave_trash_dead=wave_trash_dead+1;
	trash_watchdog_dead = trash_dead;
	trash_watchdog_ticks = 0;
	eq.debug("Trash Dead: " .. trash_dead);

	CheckProgress();

	if wave_trash_dead >= trash_wave_size then
		StopTrashWave();
		DepopTrash();
		MaybeStartNextTrashWave();
	elseif not AnySpawned(trash_ids) then
		StopTrashWave();
		MaybeStartNextTrashWave();
	end
end

function Trash_Credit(e)
	charm_credit.credit_once(e, Trash_Death, "codecay_bert_trash_charm_credited");
end

function Trash_Charm_End(e)
	charm_credit.depop_on_charm_end(e, "codecay_bert_trash_charm_credited");
end

function Adan_Death(e)
	MaybeStartNextTrashWave();
end


function Bert_Death(e)
	if event_active then
		event_active = false;
		eq.signal(200056, 9000); -- NPC: Summoner_of_Bertoxxulous
		StopEventTimers();
		eq.zone_emote(MT.NPCQuestSay,"A nimbus of light floods throughs the crypt in one magnificent wave as an earth shattering howl is heard.  The bringer of plagues, lord of all disease and decay, Bertoxxulous has been defeated. Suddenly an urgent whisper fills your head simply saying, 'The Torch of Lxanvom shall burn bright again.  Freedom is now ours, for that we thank you.");
		eq.depop_all(200043);
		eq.depop_all(200042);
		eq.depop_all(200062); -- Despawn all Quest NPC's that have spawned.
		eq.depop_all(200063);
		eq.depop_all(200064);
		eq.depop_all(200065);
		eq.depop_all(200046);
		eq.depop_all(200045);
		eq.depop_all(200044); -- Despawn all Kings if any spawned
		eq.depop_all(200047);
		eq.depop_all(200049);
		eq.depop_all(200048);
		eq.depop_all(200050);
		eq.depop_all(200051);
		eq.depop_all(200054);
		eq.depop_all(200053);
		eq.depop_all(200052);
		eq.depop_all(200022);
		eq.depop_all(200055);
		eq.depop_all(200056);
		eq.depop_all(200024);
		eq.spawn_condition("codecay", eq.get_zone_instance_id(), 1, 0);
		eq.spawn2(218068,0,0,-61.75,-.22,-288.5,384.8); -- NPC: A_Planar_Projection
	end
end

function HandleEventTimer(timer_name)
	if forced_progress[timer_name] then
		eq.stop_timer(timer_name);
		if event_active and not bert_spawned then
			if ShouldDelayForceTimer(timer_name) then
				eq.set_timer(timer_name, 30000);
			else
				ForceProgress(forced_progress[timer_name]);
			end
		end
	elseif timer_name == "bert_trash_watchdog" then
		CheckProgress();
		CheckTrashWatchdog();
	end
end

function event_timer(e)
	HandleEventTimer(e.timer);
end

function Summoner_Timer(e)
	HandleEventTimer(e.timer);
end




function event_encounter_load(e)
	eq.register_npc_event('Bert', Event.death_complete, 			200016, Spectre_Death);
	eq.register_npc_event('Bert', Event.timer, 					200056, Summoner_Timer);
	
	
	for _, npc_id in ipairs(trash_ids) do
		eq.register_npc_event('Bert', Event.death_complete, npc_id, Trash_Credit);
		eq.register_npc_event('Bert', Event.charm_start, npc_id, Trash_Credit);
		eq.register_npc_event('Bert', Event.charm_end, npc_id, Trash_Charm_End);
	end

	for _, npc_id in ipairs(first_adans) do
		eq.register_npc_event('Bert', Event.death_complete, npc_id, Adan_Death);
	end

	for _, npc_id in ipairs(second_adans) do
		eq.register_npc_event('Bert', Event.death_complete, npc_id, Adan_Death);
	end

	for _, npc_id in ipairs(final_adans) do
		eq.register_npc_event('Bert', Event.death_complete, npc_id, Adan_Death);
	end
	
	eq.register_npc_event('Bert', Event.death_complete, 200055, 			Bert_Death);
end
