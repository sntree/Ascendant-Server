# ascendant_encounter_scaling_pop.pl - Dynamic Encounter Scaling for PoP Zones
#
# Same architecture as Velious/LDON/Luclin scaling (profile-based, composition-scored).
# HP/melee/ATK set to 1.0 (no scaling down) — ready to tune later.
# Only resists are actively scaled (same deltas as Velious/Luclin).
#
# Called from global_npc.pl alongside the other scaling plugins.
# Zones are exclusive — an NPC in a PoP zone will never hit the other paths.
#
# Author: Straps

use POSIX qw(floor);
use List::Util qw(min max sum);

# =============================================================================
# MASTER KILL SWITCH — set to 0 to disable all PoP scaling instantly
# =============================================================================
my $POP_SCALING_ENABLED = 1;

# =============================================================================
# POP ZONE CONFIG — all PoP adventure/raid zones (by zoneidnumber)
# =============================================================================
my %POP_ZONES = (
    # Uses zoneidnumber (not zone.id) — matches $zoneid in quest scripts
    200 => 1,  # codecay - Crypt of Decay
    201 => 1,  # pojustice - Plane of Justice
    202 => 1,  # poknowledge - Plane of Knowledge
    203 => 1,  # potranquility - Plane of Tranquility
    204 => 1,  # ponightmare - Plane of Nightmares
    205 => 1,  # podisease - Plane of Disease
    206 => 1,  # poinnovation - Plane of Innovation
    207 => 1,  # potorment - Plane of Torment
    208 => 1,  # povalor - Plane of Valor
    209 => 1,  # bothunder - Bastion of Thunder
    210 => 1,  # postorms - Plane of Storms
    211 => 1,  # hohonora - Halls of Honor
    212 => 1,  # solrotower - Tower of Solusek Ro
    213 => 1,  # powar - Plane of War
    214 => 1,  # potactics - Drunder, Fortress of Zek
    215 => 1,  # poair - Eryslai, the Kingdom of Wind
    216 => 1,  # powater - Reef of Coirnav
    217 => 1,  # pofire - Doomfire, the Burning Lands
    218 => 1,  # poeartha - Vegarlson, the Earthen Badlands
    219 => 1,  # potimea - Plane of Time A
    220 => 1,  # hohonorb - Temple of Marr
    221 => 1,  # nightmareb - Lair of Terris Thule
    222 => 1,  # poearthb - Ragrax, Stronghold of the Twelve
    223 => 1,  # potimeb - Plane of Time B
);

# =============================================================================
# NPC EXCLUSIONS — specific npc_type_ids to never scale
# =============================================================================
my %POP_NPC_EXCLUDE = (
    # Add specific NPC type IDs here if needed
);

# =============================================================================
# CLASS WEIGHTS — same as Velious/LDON/Luclin (shared design)
# =============================================================================
my %POP_CLASS_WEIGHTS = (
    1  => { durability => 1.00, sustain => 0.15, damage => 0.70 },  # Warrior
    2  => { durability => 0.85, sustain => 0.90, damage => 0.30 },  # Cleric
    3  => { durability => 0.70, sustain => 0.60, damage => 0.75 },  # Paladin
    4  => { durability => 0.60, sustain => 0.50, damage => 0.70 },  # Ranger
    5  => { durability => 0.80, sustain => 0.30, damage => 0.80 },  # Shadowknight
    6  => { durability => 0.50, sustain => 0.75, damage => 0.50 },  # Druid
    7  => { durability => 0.75, sustain => 0.20, damage => 0.85 },  # Monk
    8  => { durability => 0.40, sustain => 0.15, damage => 0.65 },  # Bard
    9  => { durability => 0.55, sustain => 0.15, damage => 0.75 },  # Rogue
    10 => { durability => 0.55, sustain => 0.80, damage => 0.45 },  # Shaman
    11 => { durability => 0.30, sustain => 0.50, damage => 0.55 },  # Necromancer
    12 => { durability => 0.20, sustain => 0.10, damage => 1.00 },  # Wizard
    13 => { durability => 0.35, sustain => 0.40, damage => 0.65 },  # Magician
    14 => { durability => 0.25, sustain => 0.35, damage => 0.50 },  # Enchanter
    15 => { durability => 0.60, sustain => 0.20, damage => 0.75 },  # Beastlord
    16 => { durability => 0.45, sustain => 0.10, damage => 0.70 },  # Berserker
);

my %POP_FRAGILE_CASTERS = (
    11 => 1, 12 => 1, 13 => 1, 14 => 1,
);

# =============================================================================
# SYNERGY BONUSES — same as Velious/LDON/Luclin
# =============================================================================
my @POP_SYNERGY_RULES = (
    { classes => [1, 2],  bonus => 0.10 },  # Warrior + Cleric
    { classes => [3, 2],  bonus => 0.08 },  # Paladin + Cleric
    { classes => [5, 2],  bonus => 0.08 },  # SK + Cleric
    { classes => [1, 10], bonus => 0.08 },  # Warrior + Shaman
    { classes => [7, 10], bonus => 0.08 },  # Monk + Shaman
    { classes => [13, 2],  bonus => 0.07 }, # Mage + Cleric
    { classes => [11, 10], bonus => 0.07 }, # Necro + Shaman
    { classes => [14, 2],  bonus => 0.06 }, # Enchanter + Cleric
    { classes => [14, 10], bonus => 0.06 }, # Enchanter + Shaman
);

# =============================================================================
# PROFILE TUNING TABLES — PoP-specific values
#
# HP/melee/ATK all at 1.0 (no scaling down). Only resists are active.
# To enable HP/melee/ATK scaling later, change the 1.0 values.
# Resist deltas match Velious/Luclin tuning.
# =============================================================================
my %POP_PROFILE_TUNING = (
    raid => {
        hp_solo_floor    => 1.00,
        hp_solo_ceiling  => 1.00,
        hp_group_base    => 1.00,
        hp_group_cap     => 1.00,
        melee_solo_floor => 1.00,
        melee_solo_ceil  => 1.00,
        melee_group_base => 1.00,
        melee_group_cap  => 1.00,
        atk_solo_floor   => 1.00,
        atk_group_base   => 1.00,
        atk_group_cap    => 1.00,
        resist_solo_min  => -101,
        resist_solo_max  => -32,
        resist_group_base => -23,
        resist_group_cap => -4,
    },
    named => {
        hp_solo_floor    => 1.00,
        hp_solo_ceiling  => 1.00,
        hp_group_base    => 1.00,
        hp_group_cap     => 1.00,
        melee_solo_floor => 1.00,
        melee_solo_ceil  => 1.00,
        melee_group_base => 1.00,
        melee_group_cap  => 1.00,
        atk_solo_floor   => 1.00,
        atk_group_base   => 1.00,
        atk_group_cap    => 1.00,
        resist_solo_min  => -82,
        resist_solo_max  => -17,
        resist_group_base => -14,
        resist_group_cap => -3,
    },
    trash => {
        hp_solo_floor    => 1.00,
        hp_solo_ceiling  => 1.00,
        hp_group_base    => 1.00,
        hp_group_cap     => 1.00,
        melee_solo_floor => 1.00,
        melee_solo_ceil  => 1.00,
        melee_group_base => 1.00,
        melee_group_cap  => 1.00,
        atk_solo_floor   => 1.00,
        atk_group_base   => 1.00,
        atk_group_cap    => 1.00,
        resist_solo_min  => -50,
        resist_solo_max  => -11,
        resist_group_base => -8,
        resist_group_cap => -2,
    },
);

# =============================================================================
# LEVEL-BASED MINIMUM STAT FLOORS
# =============================================================================
my %POP_STAT_FLOORS = (
    raid => {
        min_hit_per_level => 2.5,
        max_hit_per_level => 6.0,
    },
    named => {
        min_hit_per_level => 2.0,
        max_hit_per_level => 5.0,
    },
    trash => {
        min_hit_per_level => 1.5,
        max_hit_per_level => 3.5,
    },
);

my $POP_RESCAN_INTERVAL = 12;
my $POP_GROUP_CAP_START = 6;
my $POP_MAX_PARTICIPANTS = 72;

# =============================================================================
# PUBLIC API — called from global_npc.pl
# =============================================================================

sub IsPopScalingZone {
    my ($zid) = @_;
    return 0 unless $POP_SCALING_ENABLED;
    return exists $POP_ZONES{$zid} ? 1 : 0;
}

sub PopScaling_OnEngage {
    my ($npc) = @_;
    return unless $POP_SCALING_ENABLED;
    return unless $npc;

    quest::debug("[PopScaling] OnEngage called for: " . $npc->GetCleanName() . " (NPCID=" . $npc->GetNPCTypeID() . ")");

    if ($npc->IsPet()) {
        quest::debug("[PopScaling] SKIP: pet");
        return;
    }
    if ($npc->GetLevel() <= 1) {
        quest::debug("[PopScaling] SKIP: level <= 1");
        return;
    }
    if (exists $POP_NPC_EXCLUDE{$npc->GetNPCTypeID()}) {
        quest::debug("[PopScaling] SKIP: excluded NPC");
        return;
    }

    my $zoneid = plugin::val('$zoneid');
    if (!exists $POP_ZONES{$zoneid}) {
        quest::debug("[PopScaling] SKIP: zone $zoneid not in PoP zones");
        return;
    }

    my $scaled_flag = $npc->GetEntityVariable('enc_scaled');
    return if $scaled_flag && $scaled_flag eq '1';

    my $profile = _pop_get_profile($npc);

    _pop_snapshot_stats($npc);

    my $primary_client;
    my $target = $npc->GetTarget();
    if ($target && $target->IsClient()) {
        $primary_client = $target->CastToClient();
    }
    if (!$primary_client) {
        my @hate_list = $npc->GetHateList();
        foreach my $ent (@hate_list) {
            next unless $ent;
            my $h_ent = $ent->GetEnt();
            if ($h_ent && $h_ent->IsClient()) {
                $primary_client = $h_ent->CastToClient();
                last;
            }
        }
    }
    if (!$primary_client) {
        quest::debug("[PopScaling] SKIP: " . $npc->GetCleanName() . " — no client found");
        return;
    }

    my ($participant_count, $class_list_ref) = _pop_count_participants($primary_client);
    my $comp_score = _pop_score_composition($class_list_ref);
    $comp_score += _pop_check_synergies($class_list_ref);
    $comp_score = max(0.0, min(1.0, $comp_score));

    my $mults = _pop_compute_multipliers($profile, $participant_count, $comp_score);

    _pop_apply_scaling($npc, $mults, $profile);

    $npc->SetEntityVariable('enc_scaled', '1');
    $npc->SetEntityVariable('enc_profile', $profile);
    $npc->SetEntityVariable('enc_participants', "$participant_count");
    $npc->SetEntityVariable('enc_comp_score', sprintf("%.3f", $comp_score));

    quest::settimer('enc_rescan', $POP_RESCAN_INTERVAL);

    quest::debug("[PopScaling] ENGAGE: " . $npc->GetCleanName()
        . " profile=$profile participants=$participant_count"
        . " comp=" . sprintf("%.3f", $comp_score)
        . " hp_mult=" . sprintf("%.3f", $mults->{hp})
        . " melee_mult=" . sprintf("%.3f", $mults->{melee})
        . " atk_mult=" . sprintf("%.3f", $mults->{atk})
        . " resist_delta=" . sprintf("%.0f", $mults->{resist_delta})
    );
}

sub PopScaling_OnDisengage {
    my ($npc) = @_;
    return unless $npc;

    my $scaled = $npc->GetEntityVariable('enc_scaled');
    return unless $scaled && $scaled eq '1';

    quest::stoptimer('enc_rescan');
    _pop_restore_stats($npc);

    $npc->SetEntityVariable('enc_scaled', '0');
    $npc->DeleteEntityVariable('enc_profile');
    $npc->DeleteEntityVariable('enc_participants');
    $npc->DeleteEntityVariable('enc_comp_score');

    quest::debug("[PopScaling] DISENGAGE: " . $npc->GetCleanName() . " — stats restored");
}

sub PopScaling_Rescan {
    my ($npc) = @_;
    return unless $npc;
    return unless $POP_SCALING_ENABLED;

    my $scaled = $npc->GetEntityVariable('enc_scaled');
    return unless $scaled && $scaled eq '1';

    my $primary_client;
    my @hate_list = $npc->GetHateList();
    foreach my $ent (@hate_list) {
        next unless $ent;
        my $h_ent = $ent->GetEnt();
        if ($h_ent && $h_ent->IsClient()) {
            $primary_client = $h_ent->CastToClient();
            last;
        }
    }
    return unless $primary_client;
    my ($new_count, $class_list_ref) = _pop_count_participants($primary_client);

    my $old_count = int($npc->GetEntityVariable('enc_participants') || 1);

    if ($new_count > $old_count) {
        my $profile = $npc->GetEntityVariable('enc_profile') || 'trash';
        my $comp_score = _pop_score_composition($class_list_ref);
        $comp_score += _pop_check_synergies($class_list_ref);
        $comp_score = max(0.0, min(1.0, $comp_score));

        my $mults = _pop_compute_multipliers($profile, $new_count, $comp_score);

        _pop_restore_stats($npc);
        _pop_apply_scaling($npc, $mults, $profile);

        $npc->SetEntityVariable('enc_participants', "$new_count");
        $npc->SetEntityVariable('enc_comp_score', sprintf("%.3f", $comp_score));

        quest::debug("[PopScaling] RESCAN UP: " . $npc->GetCleanName()
            . " $old_count -> $new_count participants"
            . " hp_mult=" . sprintf("%.3f", $mults->{hp})
        );
    }
}

# =============================================================================
# INTERNAL FUNCTIONS
# =============================================================================

sub _pop_get_profile {
    my ($npc) = @_;
    if ($npc->IsRaidTarget()) { return 'raid'; }
    elsif ($npc->IsRareSpawn()) { return 'named'; }
    return 'trash';
}

sub _pop_snapshot_stats {
    my ($npc) = @_;
    $npc->SetEntityVariable('enc_orig_max_hp',  $npc->GetMaxHP());
    $npc->SetEntityVariable('enc_orig_min_hit', $npc->GetMinDMG());
    $npc->SetEntityVariable('enc_orig_max_hit', $npc->GetMaxDMG());
    $npc->SetEntityVariable('enc_orig_atk',     $npc->GetATK());
    $npc->SetEntityVariable('enc_orig_mr',      $npc->GetMR());
    $npc->SetEntityVariable('enc_orig_fr',      $npc->GetFR());
    $npc->SetEntityVariable('enc_orig_cr',      $npc->GetCR());
    $npc->SetEntityVariable('enc_orig_pr',      $npc->GetPR());
    $npc->SetEntityVariable('enc_orig_dr',      $npc->GetDR());
    $npc->SetEntityVariable('enc_orig_heal_scale', '100');
}

sub _pop_restore_stats {
    my ($npc) = @_;
    my $orig_hp  = $npc->GetEntityVariable('enc_orig_max_hp');
    return unless defined $orig_hp && $orig_hp > 0;

    my $hp_ratio = $npc->GetHPRatio();

    my $orig_min = $npc->GetEntityVariable('enc_orig_min_hit');
    my $orig_max = $npc->GetEntityVariable('enc_orig_max_hit');
    my $orig_atk = $npc->GetEntityVariable('enc_orig_atk');
    my $orig_mr  = $npc->GetEntityVariable('enc_orig_mr');
    my $orig_fr  = $npc->GetEntityVariable('enc_orig_fr');
    my $orig_cr  = $npc->GetEntityVariable('enc_orig_cr');
    my $orig_pr  = $npc->GetEntityVariable('enc_orig_pr');
    my $orig_dr  = $npc->GetEntityVariable('enc_orig_dr');

    $npc->ModifyNPCStat("max_hp",  "$orig_hp");
    $npc->ModifyNPCStat("min_hit", "$orig_min");
    $npc->ModifyNPCStat("max_hit", "$orig_max");
    $npc->ModifyNPCStat("atk",     "$orig_atk");
    $npc->ModifyNPCStat("mr",      "$orig_mr");
    $npc->ModifyNPCStat("fr",      "$orig_fr");
    $npc->ModifyNPCStat("cr",      "$orig_cr");
    $npc->ModifyNPCStat("pr",      "$orig_pr");
    $npc->ModifyNPCStat("dr",      "$orig_dr");
    $npc->ModifyNPCStat("heal_scale", "100");

    # Keep damage already dealt. Full-healing here reset bosses to 100% on every
    # aggro drop (feign death, mem blur, rescan). Recovery is handled by the
    # NPC:OOCRegen rule instead.
    my $restored_hp = int($npc->GetMaxHP() * $hp_ratio / 100);
    $restored_hp = 1 if $restored_hp < 1;
    $npc->SetHP($restored_hp);
}

sub _pop_count_participants {
    my ($client) = @_;
    my $entity_list = plugin::val('$entity_list');
    my @class_ids;

    my $raid = $client->GetRaid();
    if ($raid) {
        for (my $i = 0; $i < 72; $i++) {
            my $cid = $raid->GetMember($i);
            next unless $cid;
            my $m = $entity_list->GetClientByCharID($cid);
            if ($m) { push @class_ids, $m->GetClass(); }
        }
        my $count = scalar(@class_ids) || 1;
        return ($count, \@class_ids);
    }

    my $group = $client->GetGroup();
    if ($group) {
        for (my $i = 0; $i < 6; $i++) {
            my $m = $group->GetMember($i);
            if ($m) { push @class_ids, $m->GetClass(); }
        }
        my $count = scalar(@class_ids) || 1;
        return ($count, \@class_ids);
    }

    push @class_ids, $client->GetClass();
    return (1, \@class_ids);
}

sub _pop_score_composition {
    my ($class_list_ref) = @_;
    my $count = scalar(@{$class_list_ref}) || 1;

    my ($total_dur, $total_sus, $total_dmg) = (0, 0, 0);
    foreach my $cid (@{$class_list_ref}) {
        my $w = $POP_CLASS_WEIGHTS{$cid};
        next unless $w;
        $total_dur += $w->{durability};
        $total_sus += $w->{sustain};
        $total_dmg += $w->{damage};
    }

    my $avg_dur = $total_dur / $count;
    my $avg_sus = $total_sus / $count;
    my $avg_dmg = $total_dmg / $count;

    my $score = ($avg_dur * 0.40) + ($avg_sus * 0.35) + ($avg_dmg * 0.25);
    return max(0.0, min(1.0, $score));
}

sub _pop_check_synergies {
    my ($class_list_ref) = @_;
    my %present;
    foreach my $cid (@{$class_list_ref}) { $present{$cid} = 1; }

    my $bonus = 0;
    foreach my $rule (@POP_SYNERGY_RULES) {
        my $all_present = 1;
        foreach my $req_class (@{$rule->{classes}}) {
            unless (exists $present{$req_class}) {
                $all_present = 0;
                last;
            }
        }
        $bonus += $rule->{bonus} if $all_present;
    }
    return $bonus;
}

sub _pop_compute_multipliers {
    my ($profile, $count, $comp_score) = @_;
    my $t = $POP_PROFILE_TUNING{$profile} || $POP_PROFILE_TUNING{trash};

    $count = max(1, min($POP_MAX_PARTICIPANTS, $count));

    my ($hp_mult, $melee_mult, $atk_mult, $resist_delta);

    if ($count <= 2) {
        my $duo_factor = ($count == 2) ? 0.15 : 0.0;

        $hp_mult    = _pop_lerp($t->{hp_solo_floor}, $t->{hp_solo_ceiling},
                            $comp_score + $duo_factor);
        $melee_mult = _pop_lerp($t->{melee_solo_floor}, $t->{melee_solo_ceil},
                            $comp_score + $duo_factor);
        $atk_mult   = _pop_lerp($t->{atk_solo_floor}, 1.0,
                            $comp_score + $duo_factor);
        $resist_delta = _pop_lerp($t->{resist_solo_min}, $t->{resist_solo_max},
                              $comp_score + $duo_factor);
    } else {
        my $group_progress = _pop_group_curve($count);

        $hp_mult    = _pop_lerp($t->{hp_group_base}, $t->{hp_group_cap}, $group_progress);
        $melee_mult = _pop_lerp($t->{melee_group_base}, $t->{melee_group_cap}, $group_progress);
        $atk_mult   = _pop_lerp($t->{atk_group_base}, $t->{atk_group_cap}, $group_progress);
        $resist_delta = _pop_lerp($t->{resist_group_base}, $t->{resist_group_cap}, $group_progress);
    }

    $hp_mult    = max(0.20, min($t->{hp_group_cap}, $hp_mult));
    $melee_mult = max(0.20, min($t->{melee_group_cap}, $melee_mult));
    $atk_mult   = max(0.30, min($t->{atk_group_cap}, $atk_mult));

    return {
        hp           => $hp_mult,
        melee        => $melee_mult,
        atk          => $atk_mult,
        resist_delta => $resist_delta,
    };
}

sub _pop_group_curve {
    my ($count) = @_;
    return 0.0 if $count <= 2;
    my $x = $count - 2;
    my $max_x = $POP_MAX_PARTICIPANTS - 2;
    my $progress = log(1 + $x) / log(1 + $max_x);
    return max(0.0, min(1.0, $progress));
}

sub _pop_lerp {
    my ($a, $b, $t) = @_;
    $t = max(0.0, min(1.0, $t));
    return $a + ($b - $a) * $t;
}

sub _pop_apply_scaling {
    my ($npc, $mults, $profile) = @_;

    my $orig_hp  = int($npc->GetEntityVariable('enc_orig_max_hp')  || $npc->GetMaxHP());
    my $orig_min = int($npc->GetEntityVariable('enc_orig_min_hit') || $npc->GetMinDMG());
    my $orig_max = int($npc->GetEntityVariable('enc_orig_max_hit') || $npc->GetMaxDMG());
    my $orig_atk = int($npc->GetEntityVariable('enc_orig_atk')     || $npc->GetATK());

    # HP — keep the same damage taken percentage across the rescale
    my $hp_ratio = $npc->GetHPRatio();
    my $new_hp = max(1, int($orig_hp * $mults->{hp}));
    $npc->ModifyNPCStat("max_hp", "$new_hp");
    my $scaled_hp = int($npc->GetMaxHP() * $hp_ratio / 100);
    $scaled_hp = 1 if $scaled_hp < 1;
    $npc->SetHP($scaled_hp);

    # Melee damage with level-based floors
    my $level = $npc->GetLevel();
    my $floors = $POP_STAT_FLOORS{$profile} || $POP_STAT_FLOORS{trash};
    my $comp = $npc->GetEntityVariable('enc_comp_score') || 0.5;
    my $floor_scale = 0.5 + (0.5 * $comp);

    my $min_floor = int($level * $floors->{min_hit_per_level} * $floor_scale);
    my $max_floor = int($level * $floors->{max_hit_per_level} * $floor_scale);

    my $new_min = max($min_floor, int($orig_min * $mults->{melee}));
    my $new_max = max($max_floor, int($orig_max * $mults->{melee}));
    $new_max = max($new_min, $new_max);
    $npc->ModifyNPCStat("min_hit", "$new_min");
    $npc->ModifyNPCStat("max_hit", "$new_max");

    # ATK
    my $new_atk = max(1, int($orig_atk * $mults->{atk}));
    $npc->ModifyNPCStat("atk", "$new_atk");

    # Heal Scale
    my $new_heal_scale = max(10, int(100 * $mults->{hp}));
    $npc->ModifyNPCStat("heal_scale", "$new_heal_scale");

    # Resists — additive delta from original values
    my $delta = int($mults->{resist_delta});
    if ($delta != 0) {
        foreach my $resist_type ('mr', 'fr', 'cr', 'pr', 'dr') {
            my $orig_key = 'enc_orig_' . $resist_type;
            my $orig_val = int($npc->GetEntityVariable($orig_key) || 0);
            my $new_val  = max(0, $orig_val + $delta);
            $npc->ModifyNPCStat($resist_type, "$new_val");
        }
    }
}

1;
