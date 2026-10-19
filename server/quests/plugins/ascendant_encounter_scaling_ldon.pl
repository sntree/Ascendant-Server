# ascendant_encounter_scaling_ldon.pl - Dynamic Encounter Scaling for LDON Zones
#
# Separate tuning from Velious scaling. Same architecture (profile-based,
# composition-scored), but independently configurable resist/hp/melee deltas.
#
# Called from global_npc.pl alongside the Velious scaling plugin.
# Zones are exclusive — an NPC in an LDON zone will never hit the Velious path.
#
# Author: Straps

use POSIX qw(floor);
use List::Util qw(min max sum);

# =============================================================================
# MASTER KILL SWITCH — set to 0 to disable all LDON scaling instantly
# =============================================================================
my $LDON_SCALING_ENABLED = 1;

# =============================================================================
# LDON ZONE CONFIG — all 49 LDON instance zones
# =============================================================================
my %LDON_ZONES = (
    # Deepest Guk
    229 => 1, 234 => 1, 239 => 1, 244 => 1, 249 => 1, 254 => 1, 259 => 1, 264 => 1,
    # Rujarkian Hills
    230 => 1, 235 => 1, 240 => 1, 245 => 1, 250 => 1, 255 => 1, 260 => 1, 265 => 1, 269 => 1, 273 => 1,
    # Takish-Hiz
    231 => 1, 236 => 1, 241 => 1, 246 => 1, 251 => 1, 256 => 1, 261 => 1, 266 => 1, 270 => 1, 274 => 1,
    # Miragul's Menagerie
    232 => 1, 237 => 1, 242 => 1, 247 => 1, 252 => 1, 257 => 1, 262 => 1, 267 => 1, 271 => 1, 275 => 1,
    # Mistmoore Catacombs
    233 => 1, 238 => 1, 243 => 1, 248 => 1, 253 => 1, 258 => 1, 263 => 1, 268 => 1, 272 => 1, 276 => 1,
);

# =============================================================================
# NPC EXCLUSIONS — specific npc_type_ids to never scale
# =============================================================================
my %LDON_NPC_EXCLUDE = (
    # Add specific NPC type IDs here if needed
);

# =============================================================================
# CLASS WEIGHTS — same as Velious (shared design)
# =============================================================================
my %LDON_CLASS_WEIGHTS = (
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

my %LDON_FRAGILE_CASTERS = (
    11 => 1, 12 => 1, 13 => 1, 14 => 1,
);

# =============================================================================
# SYNERGY BONUSES — same as Velious
# =============================================================================
my @LDON_SYNERGY_RULES = (
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
# PROFILE TUNING TABLES — LDON-specific values
#
# LDON base resists are lower than Velious (avg 42-48 vs 73-141), so resist
# deltas can be less aggressive. Players are level 60 in Velious gear.
#
# hp_solo_floor:    HP multiplier for weakest solo composition
# hp_solo_ceiling:  HP multiplier for strongest solo composition
# hp_group_cap:     HP multiplier cap for large groups
# melee_solo_floor: min_hit/max_hit multiplier for weakest solo
# melee_solo_ceil:  min_hit/max_hit multiplier for strongest solo
# melee_group_cap:  melee multiplier cap for large groups
# atk_solo_floor:   ATK multiplier for weakest solo
# atk_group_cap:    ATK multiplier cap for large groups
# resist_solo_min:  resist delta for weakest solo (negative = easier)
# resist_solo_max:  resist delta for strongest solo
# resist_group_cap: resist delta cap for large groups
# =============================================================================
my %LDON_PROFILE_TUNING = (
    raid => {
        hp_solo_floor    => 0.31,
        hp_solo_ceiling  => 0.60,
        hp_group_base    => 0.63,
        hp_group_cap     => 0.81,
        melee_solo_floor => 0.77,
        melee_solo_ceil  => 1.00,
        melee_group_base => 1.00,
        melee_group_cap  => 1.20,
        atk_solo_floor   => 0.66,
        atk_group_base   => 0.91,
        atk_group_cap    => 1.15,
        resist_solo_min  => -101,
        resist_solo_max  => -32,
        resist_group_base => -23,
        resist_group_cap => -4,
    },
    named => {
        hp_solo_floor    => 0.37,
        hp_solo_ceiling  => 0.61,
        hp_group_base    => 0.68,
        hp_group_cap     => 0.81,
        melee_solo_floor => 0.81,
        melee_solo_ceil  => 1.00,
        melee_group_base => 1.00,
        melee_group_cap  => 1.15,
        atk_solo_floor   => 0.75,
        atk_group_base   => 0.99,
        atk_group_cap    => 1.10,
        resist_solo_min  => -82,
        resist_solo_max  => -17,
        resist_group_base => -14,
        resist_group_cap => -3,
    },
    trash => {
        hp_solo_floor    => 0.51,
        hp_solo_ceiling  => 0.77,
        hp_group_base    => 0.80,
        hp_group_cap     => 0.81,
        melee_solo_floor => 0.96,
        melee_solo_ceil  => 1.00,
        melee_group_base => 1.00,
        melee_group_cap  => 1.10,
        atk_solo_floor   => 0.91,
        atk_group_base   => 1.00,
        atk_group_cap    => 1.05,
        resist_solo_min  => -50,
        resist_solo_max  => -11,
        resist_group_base => -8,
        resist_group_cap => -2,
    },
);

# =============================================================================
# LEVEL-BASED MINIMUM STAT FLOORS
# =============================================================================
my %LDON_STAT_FLOORS = (
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

my $LDON_RESCAN_INTERVAL = 12;
my $LDON_GROUP_CAP_START = 6;
my $LDON_MAX_PARTICIPANTS = 6;

# =============================================================================
# PUBLIC API — called from global_npc.pl
# =============================================================================

sub IsLdonScalingZone {
    my ($zid) = @_;
    return 0 unless $LDON_SCALING_ENABLED;
    return exists $LDON_ZONES{$zid} ? 1 : 0;
}

sub LdonScaling_OnEngage {
    my ($npc) = @_;
    return unless $LDON_SCALING_ENABLED;
    return unless $npc;

    quest::debug("[LdonScaling] OnEngage called for: " . $npc->GetCleanName() . " (NPCID=" . $npc->GetNPCTypeID() . ")");

    if ($npc->IsPet()) {
        quest::debug("[LdonScaling] SKIP: pet");
        return;
    }
    if ($npc->GetLevel() <= 1) {
        quest::debug("[LdonScaling] SKIP: level <= 1");
        return;
    }
    if (exists $LDON_NPC_EXCLUDE{$npc->GetNPCTypeID()}) {
        quest::debug("[LdonScaling] SKIP: excluded NPC");
        return;
    }

    my $zoneid = plugin::val('$zoneid');
    if (!exists $LDON_ZONES{$zoneid}) {
        quest::debug("[LdonScaling] SKIP: zone $zoneid not in LDON zones");
        return;
    }

    my $scaled_flag = $npc->GetEntityVariable('enc_scaled');
    return if $scaled_flag && $scaled_flag eq '1';

    my $profile = _ldon_get_profile($npc);

    _ldon_snapshot_stats($npc);

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
        quest::debug("[LdonScaling] SKIP: " . $npc->GetCleanName() . " — no client found");
        return;
    }

    my ($participant_count, $class_list_ref) = _ldon_count_participants($primary_client);
    my $comp_score = _ldon_score_composition($class_list_ref);
    $comp_score += _ldon_check_synergies($class_list_ref);
    $comp_score = max(0.0, min(1.0, $comp_score));

    my $mults = _ldon_compute_multipliers($profile, $participant_count, $comp_score);

    _ldon_apply_scaling($npc, $mults, $profile);

    $npc->SetEntityVariable('enc_scaled', '1');
    $npc->SetEntityVariable('enc_profile', $profile);
    $npc->SetEntityVariable('enc_participants', "$participant_count");
    $npc->SetEntityVariable('enc_comp_score', sprintf("%.3f", $comp_score));

    quest::settimer('enc_rescan', $LDON_RESCAN_INTERVAL);

    quest::debug("[LdonScaling] ENGAGE: " . $npc->GetCleanName()
        . " profile=$profile participants=$participant_count"
        . " comp=" . sprintf("%.3f", $comp_score)
        . " hp_mult=" . sprintf("%.3f", $mults->{hp})
        . " melee_mult=" . sprintf("%.3f", $mults->{melee})
        . " atk_mult=" . sprintf("%.3f", $mults->{atk})
        . " resist_delta=" . sprintf("%.0f", $mults->{resist_delta})
    );
}

sub LdonScaling_OnDisengage {
    my ($npc) = @_;
    return unless $npc;

    my $scaled = $npc->GetEntityVariable('enc_scaled');
    return unless $scaled && $scaled eq '1';

    quest::stoptimer('enc_rescan');
    _ldon_restore_stats($npc);

    $npc->SetEntityVariable('enc_scaled', '0');
    $npc->DeleteEntityVariable('enc_profile');
    $npc->DeleteEntityVariable('enc_participants');
    $npc->DeleteEntityVariable('enc_comp_score');

    quest::debug("[LdonScaling] DISENGAGE: " . $npc->GetCleanName() . " — stats restored");
}

sub LdonScaling_Rescan {
    my ($npc) = @_;
    return unless $npc;
    return unless $LDON_SCALING_ENABLED;

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
    my ($new_count, $class_list_ref) = _ldon_count_participants($primary_client);

    my $old_count = int($npc->GetEntityVariable('enc_participants') || 1);

    if ($new_count > $old_count) {
        my $profile = $npc->GetEntityVariable('enc_profile') || 'trash';
        my $comp_score = _ldon_score_composition($class_list_ref);
        $comp_score += _ldon_check_synergies($class_list_ref);
        $comp_score = max(0.0, min(1.0, $comp_score));

        my $mults = _ldon_compute_multipliers($profile, $new_count, $comp_score);

        _ldon_restore_stats($npc);
        _ldon_apply_scaling($npc, $mults, $profile);

        $npc->SetEntityVariable('enc_participants', "$new_count");
        $npc->SetEntityVariable('enc_comp_score', sprintf("%.3f", $comp_score));

        quest::debug("[LdonScaling] RESCAN UP: " . $npc->GetCleanName()
            . " $old_count -> $new_count participants"
            . " hp_mult=" . sprintf("%.3f", $mults->{hp})
        );
    }
}

# =============================================================================
# INTERNAL FUNCTIONS
# =============================================================================

sub _ldon_get_profile {
    my ($npc) = @_;
    if ($npc->IsRaidTarget()) { return 'raid'; }
    elsif ($npc->IsRareSpawn()) { return 'named'; }
    return 'trash';
}

sub _ldon_snapshot_stats {
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

sub _ldon_restore_stats {
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

sub _ldon_count_participants {
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

sub _ldon_score_composition {
    my ($class_list_ref) = @_;
    my $count = scalar(@{$class_list_ref}) || 1;

    my ($total_dur, $total_sus, $total_dmg) = (0, 0, 0);
    foreach my $cid (@{$class_list_ref}) {
        my $w = $LDON_CLASS_WEIGHTS{$cid};
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

sub _ldon_check_synergies {
    my ($class_list_ref) = @_;
    my %present;
    foreach my $cid (@{$class_list_ref}) { $present{$cid} = 1; }

    my $bonus = 0;
    foreach my $rule (@LDON_SYNERGY_RULES) {
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

sub _ldon_compute_multipliers {
    my ($profile, $count, $comp_score) = @_;
    my $t = $LDON_PROFILE_TUNING{$profile} || $LDON_PROFILE_TUNING{trash};

    $count = max(1, min($LDON_MAX_PARTICIPANTS, $count));

    my ($hp_mult, $melee_mult, $atk_mult, $resist_delta);

    if ($count <= 2) {
        my $duo_factor = ($count == 2) ? 0.15 : 0.0;

        $hp_mult    = _ldon_lerp($t->{hp_solo_floor}, $t->{hp_solo_ceiling},
                            $comp_score + $duo_factor);
        $melee_mult = _ldon_lerp($t->{melee_solo_floor}, $t->{melee_solo_ceil},
                            $comp_score + $duo_factor);
        $atk_mult   = _ldon_lerp($t->{atk_solo_floor}, 1.0,
                            $comp_score + $duo_factor);
        $resist_delta = _ldon_lerp($t->{resist_solo_min}, $t->{resist_solo_max},
                              $comp_score + $duo_factor);
    } else {
        my $group_progress = _ldon_group_curve($count);

        $hp_mult    = _ldon_lerp($t->{hp_group_base}, $t->{hp_group_cap}, $group_progress);
        $melee_mult = _ldon_lerp($t->{melee_group_base}, $t->{melee_group_cap}, $group_progress);
        $atk_mult   = _ldon_lerp($t->{atk_group_base}, $t->{atk_group_cap}, $group_progress);
        $resist_delta = _ldon_lerp($t->{resist_group_base}, $t->{resist_group_cap}, $group_progress);
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

sub _ldon_group_curve {
    my ($count) = @_;
    return 0.0 if $count <= 2;
    my $x = $count - 2;
    my $max_x = $LDON_MAX_PARTICIPANTS - 2;
    my $progress = log(1 + $x) / log(1 + $max_x);
    return max(0.0, min(1.0, $progress));
}

sub _ldon_lerp {
    my ($a, $b, $t) = @_;
    $t = max(0.0, min(1.0, $t));
    return $a + ($b - $a) * $t;
}

sub _ldon_apply_scaling {
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
    my $floors = $LDON_STAT_FLOORS{$profile} || $LDON_STAT_FLOORS{trash};
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
