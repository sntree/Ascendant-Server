local charm_credit = require("charm_credit")
local kill_count = 0

local function npc_death(e)
  if kill_count < 50 then
    kill_count = kill_count + 1
    eq.debug(string.format("Mob kill count: [%s]", kill_count))
    if kill_count >= 50 then
      eq.spawn2(290062, 0, 0, -1137.00, 246.00, 469.97, 405.00) -- NPC: #Stonespiritist_Ekikoa
    end
  end
end

local function npc_credit(e)
  charm_credit.credit_once(e, npc_death, "vxed_charm_credited")
end

local function npc_charm_end(e)
  charm_credit.depop_on_charm_end(e, "vxed_charm_credited")
end

local credit_npc_ids = {
  290057, -- #Chavoor_the_Trickster
  290058, -- #Ikaav_Firecaller
  290060, -- #Kyv_Hunter
  290061, -- #misguided_beludu_Creque
  290064, -- #Ukun_Bloodbeast
  290063, -- #Umbiki
  290015, -- an_aneuk_ebonspirit
  290028, -- an_aneuk_ebonspirit
  290056, -- an_aneuk_ebonspirit
  290021, -- an_aneuk_fleshweaver
  290026, -- an_aneuk_fleshweaver
  290036, -- an_aneuk_fleshweaver
  290049, -- an_arisen_beludu
  290052, -- an_arisen_beludu
  290033, -- an_ikaav_detestor
  290001, -- an_ikaav_fleshflare
  290013, -- an_ikaav_fleshflare
  290032, -- an_ikaav_fleshflare
  290037, -- an_ikaav_irelette
  290039, -- an_ikaav_irelette
  290053, -- an_ikaav_irelette
  290014, -- an_ikaav_psychagonist
  290016, -- an_ikaav_psychagonist
  290011, -- an_ukun_boneretriever
  290030, -- an_ukun_boneretriever
  290029, -- an_ukun_deathfang
  290038, -- an_ukun_deathfang
  290046, -- an_ukun_deathfang
  290012, -- a_coerced_granite_worker
  290022, -- a_coerced_granite_worker
  290008, -- a_coerced_granite_worker
  290047, -- a_feeble_beludu_shade
  290048, -- a_feeble_beludu_shade
  290051, -- a_feeble_beludu_shade
  290009, -- a_flinty_cragbeast
  290005, -- a_hynid_ravager
  290043, -- a_hynid_ravager
  290045, -- a_hynid_ravager
  290025, -- a_kyv_eyecollector
  290035, -- a_kyv_eyecollector
  290003, -- a_kyv_heartstriker
  290055, -- a_kyv_heartstriker
  290017, -- a_mastruq_decapitator
  290018, -- a_mastruq_decapitator
  290034, -- a_mastruq_decapitator
  290010, -- a_mastruq_warmedic
  290027, -- a_mastruq_warmedic
  290041, -- a_mountain_pooka
  290006, -- a_mountain_pooka
  290004, -- a_mountain_pooka
  290066, -- a_noc_darklurch
  290023, -- a_noc_havocblade
  290024, -- a_noc_havocblade
  290031, -- a_noc_havocblade
  290002, -- a_noc_sprayblood
  290007, -- a_noc_sprayblood
  290000, -- a_noc_sprayblood
  290019, -- a_ra`tuk_decimator
  290020, -- a_ra`tuk_decimator
  290054, -- a_ra`tuk_decimator
  290067, -- a_ra`tuk_screambringer
  290040, -- a_venomous_stonemite
  290042, -- a_venomous_stonemite
  290044, -- a_venomous_stonemite
  290050, -- a_weeping_beludu_mother
}

-- this is still an awful way to do this
function event_encounter_load(e)
  -- ignore in paladin epic expedition
  if eq.get_zone_instance_version() ~= 0 then
    return
  end

  for _, npc_id in ipairs(credit_npc_ids) do
    eq.register_npc_event("vxed", Event.death_complete, npc_id, npc_credit)
    eq.register_npc_event("vxed", Event.charm_start, npc_id, npc_credit)
    eq.register_npc_event("vxed", Event.charm_end, npc_id, npc_charm_end)
  end
end
