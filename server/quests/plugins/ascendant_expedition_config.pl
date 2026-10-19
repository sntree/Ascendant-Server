# Ascendant EQ - Expedition Configuration
# Defines zone-specific settings for expeditions
# Author: Straps

use strict;
use warnings;

# Zone configuration hash
# zone_version: Zone version to use (default 0)
# default_name: Display name for the zone
# entry_override: {x, y, z, h} — custom zone-in point inside the DZ (optional)
# has_raid_tier: 1 if zone offers the "Raid Expedition" option
# raid_only: 1 if the zone offers ONLY the Raid Expedition (every boss is a raid target)
my %expedition_zones = (
    # ---- Classic ----
    'fearplane' => {
        zone_version => 0,
        default_name => 'Plane of Fear',
        has_raid_tier => 1
    },
    'hateplaneb' => {
        zone_version => 0,
        default_name => 'Plane of Hate',
        has_raid_tier => 1
    },
    'permafrost' => {
        zone_version => 0,
        default_name => 'Permafrost Keep',
        has_raid_tier => 1
    },
    'soldungb' => {
        zone_version => 0,
        default_name => "Nagafen's Lair",
        has_raid_tier => 1
    },
    'airplane' => {
        zone_version => 0,
        default_name => 'Plane of Sky',
        has_raid_tier => 1
    },
    'cazicthule' => {
        zone_version => 0,
        default_name => 'Cazic-Thule',
        has_raid_tier => 1
    },
    'hole' => {
        zone_version => 0,
        default_name => 'The Hole',
        has_raid_tier => 1
    },
    'kedge' => {
        zone_version => 0,
        default_name => 'Kedge Keep',
        has_raid_tier => 1
    },
    'hateplane' => {
        zone_version => 0,
        default_name => 'The Plane of Hate'
    },
    'mischiefplane' => {
        zone_version => 0,
        default_name => 'Plane of Mischief',
        has_raid_tier => 1
    },
    'potimeb' => {
        zone_version => 0,
        default_name => 'Plane of Time'
    },
    'crushbone' => {
        zone_version => 0,
        default_name => 'Crushbone'
    },
    'gukbottom' => {
        zone_version => 0,
        default_name => 'Lower Guk'
    },

    # ---- Kunark ----
    'citymist' => {
        zone_version => 1,
        default_name => 'City of Mist',
        has_raid_tier => 1
    },
    'sebilis' => {
        zone_version => 0,
        default_name => 'Old Sebilis',
        has_raid_tier => 1
    },
    'chardok' => {
        zone_version => 0,
        default_name => 'Chardok',
        has_raid_tier => 1,
        entry_override => { x => 911, y => -104, z => 104, h => 400 }
    },
    'charasis' => {
        zone_version => 0,
        default_name => 'Howling Stones',
        has_raid_tier => 1
    },
    'karnor' => {
        zone_version => 0,
        default_name => "Karnor's Castle",
        has_raid_tier => 1
    },
    'timorous' => {
        zone_version => 0,
        default_name => 'Timorous Deep',
        has_raid_tier => 1
    },
    'skyfire' => {
        zone_version => 0,
        default_name => 'Skyfire Mountains',
        has_raid_tier => 1
    },
    'emeraldjungle' => {
        zone_version => 0,
        default_name => 'Emerald Jungle',
        has_raid_tier => 1
    },
    'veeshan' => {
        zone_version => 0,
        default_name => "Veeshan's Peak",
        has_raid_tier => 1
    },

    # ---- Velious ----
    'iceclad' => {
        zone_version => 0,
        default_name => 'Iceclad Ocean',
        has_raid_tier => 1
    },
    'sirens' => {
        zone_version => 1,
        default_name => 'Sirens Grotto',
        has_raid_tier => 1
    },
    'velketor' => {
        zone_version => 0,
        default_name => "Velketor's Labyrinth",
        has_raid_tier => 1
    },
    'kael' => {
        zone_version => 0,
        default_name => 'Kael Drakkel',
        has_raid_tier => 1
    },
    'sleeper' => {
        zone_version => 0,
        default_name => "Sleeper's Tomb",
        has_raid_tier => 1
    },
    'templeveeshan' => {
        zone_version => 0,
        default_name => 'Temple of Veeshan',
        has_raid_tier => 1
    },
    'necropolis' => {
        zone_version => 0,
        default_name => 'Dragon Necropolis',
        has_raid_tier => 1
    },
    'growthplane' => {
        zone_version => 0,
        default_name => 'Plane of Growth',
        has_raid_tier => 1
    },
    'wakening' => {
        zone_version => 0,
        default_name => 'Wakening Land',
        has_raid_tier => 1
    },
    'skyshrine' => {
        zone_version => 0,
        default_name => 'Skyshrine',
        has_raid_tier => 1,
        raid_entry_override => { x => 1642, y => 1003, z => -25.94, h => 127.5 }
    },
    'thurgadinb' => {
        zone_version => 0,
        default_name => 'Thurgadin',
        has_raid_tier => 1
    },
    'cobaltscar' => {
        zone_version => 0,
        default_name => 'Cobalt Scar',
        has_raid_tier => 1
    },
    'westwastes' => {
        zone_version => 0,
        default_name => 'Western Wastes',
        has_raid_tier => 1
    },

    # ---- Luclin ----
    'ssratemple' => {
        zone_version => 0,
        default_name => 'Ssraeshza Temple',
        has_raid_tier => 1
    },
    'vexthal' => {
        zone_version => 0,
        default_name => 'Vex Thal',
        has_raid_tier => 1
    },
    'acrylia' => {
        zone_version => 0,
        default_name => 'Acrylia Caverns',
        has_raid_tier => 1
    },
    'akheva' => {
        zone_version => 0,
        default_name => 'Akheva Ruins',
        has_raid_tier => 1
    },
    'griegsend' => {
        zone_version => 1,
        default_name => "Grieg's End",
        has_raid_tier => 1
    },
    'thedeep' => {
        zone_version => 0,
        default_name => 'The Deep',
        has_raid_tier => 1
    },
    'sseru' => {
        zone_version => 0,
        default_name => 'Sanctus Seru',
        has_raid_tier => 1
    },
    'katta' => {
        zone_version => 0,
        default_name => 'Katta Castellum',
        has_raid_tier => 1
    },
    'umbral' => {
        zone_version => 0,
        default_name => 'Umbral Plains',
        has_raid_tier => 1
    },
    'maiden' => {
        zone_version => 0,
        default_name => "Maiden's Eye",
        has_raid_tier => 1
    },
    'anguish' => {
        zone_version => 0,
        default_name => 'Anguish, the Fallen Palace'
    },

    # ---- Overland / Outdoor ----
    'dreadlands' => {
        zone_version => 0,
        default_name => 'Dreadlands',
        has_raid_tier => 1,
        entry_override => { x => 5405, y => -841, z => 1251.1, h => 0 }
    },
    'greatdivide' => {
        zone_version => 0,
        default_name => 'Great Divide',
        has_raid_tier => 1,
        entry_override => { x => 138.19, y => -1040.24, z => 21.93, h => 250.75 }
    },
    'eastwastes' => {
        zone_version => 0,
        default_name => 'Eastern Wastes',
        has_raid_tier => 1,
        entry_override => { x => -375.17, y => -2667.32, z => 178.12, h => 140.75 }
    },

    # ---- Planes of Power ----
    # NOTE: potimea/potimeb (Plane of Time) intentionally omitted — PoTime runs its own
    # DZ/expedition + lockout system in potimeb/zone_status.lua and must not be given a raid tier.
    'pojustice' => {
        zone_version => 0,
        default_name => 'The Plane of Justice',
        has_raid_tier => 1
    },
    'podisease' => {
        zone_version => 0,
        default_name => 'The Plane of Disease',
        has_raid_tier => 1
    },
    'ponightmare' => {
        zone_version => 0,
        default_name => 'The Plane of Nightmares',
        has_raid_tier => 1
    },
    'poinnovation' => {
        zone_version => 0,
        default_name => 'The Plane of Innovation',
        has_raid_tier => 1
    },
    'potorment' => {
        zone_version => 0,
        default_name => 'Torment, the Plane of Pain',
        has_raid_tier => 1
    },
    'povalor' => {
        zone_version => 0,
        default_name => 'The Plane of Valor',
        has_raid_tier => 1
    },
    'codecay' => {
        zone_version => 0,
        default_name => 'The Crypt of Decay',
        has_raid_tier => 1
    },
    'bothunder' => {
        zone_version => 0,
        default_name => 'Bastion of Thunder',
        has_raid_tier => 1
    },
    'postorms' => {
        zone_version => 0,
        default_name => 'The Plane of Storms'
    },
    'hohonora' => {
        zone_version => 0,
        default_name => 'The Halls of Honor',
        has_raid_tier => 1
    },
    'hohonorb' => {
        zone_version => 0,
        default_name => 'The Temple of Marr',
        has_raid_tier => 1
    },
    'solrotower' => {
        zone_version => 0,
        default_name => 'The Tower of Solusek Ro',
        has_raid_tier => 1
    },
    'potactics' => {
        zone_version => 0,
        default_name => 'Drunder, the Fortress of Zek',
        has_raid_tier => 1
    },
    'poair' => {
        zone_version => 0,
        default_name => 'The Plane of Air',
        has_raid_tier => 1
    },
    'powater' => {
        zone_version => 0,
        default_name => 'The Plane of Water',
        has_raid_tier => 1
    },
    'pofire' => {
        zone_version => 0,
        default_name => 'The Plane of Fire',
        has_raid_tier => 1
    },
    'poeartha' => {
        zone_version => 0,
        default_name => 'The Plane of Earth',
        has_raid_tier => 1
    },
    'poearthb' => {
        zone_version => 0,
        default_name => 'The Plane of Earth (Ragrax)',
        has_raid_tier => 1
    },
    'nightmareb' => {
        zone_version => 0,
        default_name => 'The Lair of Terris Thule',
        has_raid_tier => 1
    },

    # ---- Legacy of Ykesha ----
    # Raid tier only where the zone has raid_target bosses:
    # nadox (Broodmother, Innoruuk), hatesfury (Captain Krasnok),
    # chardokb (Korucust). Varns (dulak) and Culthor (soldungc) are
    # rare_spawn nameds, so those zones have no raid tier.
    'nedaria' => {
        zone_version => 0,
        default_name => "Nedaria's Landing"
    },
    'gunthak' => {
        zone_version => 0,
        default_name => 'The Gulf of Gunthak'
    },
    'dulak' => {
        zone_version => 0,
        default_name => "Dulak's Harbor"
    },
    'torgiran' => {
        zone_version => 0,
        default_name => 'The Torgiran Mines'
    },
    'nadox' => {
        zone_version => 0,
        default_name => 'The Crypt of Nadox',
        has_raid_tier => 1
    },
    'hatesfury' => {
        zone_version => 0,
        default_name => "Hate's Fury, The Scorned Maiden",
        has_raid_tier => 1
    },
    'chardokb' => {
        zone_version => 0,
        default_name => 'Chardok: The Halls of Betrayal',
        has_raid_tier => 1
    },
    'soldungc' => {
        zone_version => 0,
        default_name => 'The Caverns of Exile'
    },

    # ---- Gates of Discord ----
    # Raid tier where the zone has raid_target bosses (2026_09_26_god_named_flags.sql).
    # qvic / txevu are raid_only (owner 2026-09-26). NOT here (own DZ/instance scripts):
    # uqua, ikkinz, inktuta, tacvi, snpool/snlair/snplant/sncrematory, vxed, tipt; abysmal hub.
    'natimbi' => {  # Cragbeast Queen
        zone_version => 0,
        default_name => 'Natimbi, the Broken Shores',
        has_raid_tier => 1
    },
    'qinimi' => {  # Mastruq Commander Gorlakt, Spiritlords, Pixtt Xictic Krvne, Mass of Stone, Kabeka Kret
        zone_version => 0,
        default_name => 'Qinimi, Court of Nihilia',
        has_raid_tier => 1
    },
    'riwwi' => {  # Taskmistress Krisz
        zone_version => 0,
        default_name => 'Riwwi, Coliseum of Games',
        has_raid_tier => 1
    },
    'barindu' => {  # Colossus of War, Ikaav Ixvet Pox
        zone_version => 0,
        default_name => 'Barindu, Hanging Gardens',
        has_raid_tier => 1
    },
    'ferubi' => {  # Packmaster Skoiat Pizak, Zun`Muram Votal, Smith Rondo event
        zone_version => 0,
        default_name => 'Ferubi, Forgotten Temple of Taelosia',
        has_raid_tier => 1
    },
    'kodtaz' => {  # Pixtt Nemis, Summoning Circle, Ageless Relic Protectors
        zone_version => 0,
        default_name => 'Kod\'Taz, Broken Trial Grounds',
        has_raid_tier => 1
    },
    'yxtta' => {  # Primals, Pixtt Suir Mindrider
        zone_version => 0,
        default_name => 'Yxtta, Pulpit of Exiles',
        has_raid_tier => 1
    },
    'qvic' => {  # every boss is a raid target
        zone_version => 0,
        default_name => 'Qvic, Prayer Grounds of Calling',
        has_raid_tier => 1,
        raid_only => 1
    },
    'txevu' => {  # every boss is a raid target
        zone_version => 0,
        default_name => 'Txevu, Lair of the Elite',
        has_raid_tier => 1,
        raid_only => 1
    }
);

sub GetExpeditionConfig {
    my $zone_name = shift;
    
    # Return config if exists, otherwise return default
    if (exists $expedition_zones{$zone_name}) {
        return $expedition_zones{$zone_name};
    }
    
    # Default configuration for zones not in the list
    return {
        zone_version => 0,
        default_name => quest::GetZoneLongName($zone_name)
    };
}

sub IsRaidOnly {
    my $zone_name = shift;

    my $config = GetExpeditionConfig($zone_name);
    return ($config->{raid_only} && $config->{raid_only} == 1) ? 1 : 0;
}

sub HasRaidTier {
    my $zone_name = shift;

    my $config = GetExpeditionConfig($zone_name);
    return ($config->{has_raid_tier} && $config->{has_raid_tier} == 1) ? 1 : 0;
}

return 1;
