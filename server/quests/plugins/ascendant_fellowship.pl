package plugin;

use strict;
use warnings;

# ============================================================
# Ascendant Fellowship Bonus System
# ============================================================
# Rewards players for grouping with unique real people.
# Uses historical IP cross-referencing (account_ip table) to
# detect alt/multibox accounts and count only truly unique players.
#
# Spell IDs:
#   29433 = Ascendant Fellowship I   (Bronze, 2 unique)
#   29434 = Ascendant Fellowship II  (Silver, 3 unique)
#   29435 = Ascendant Fellowship III (Gold,   4+ unique)
# ============================================================

my @FELLOWSHIP_SPELLS = (29433, 29434, 29435);
my %TIER_SPELL = (
    1 => 29433,  # Bronze
    2 => 29434,  # Silver
    3 => 29435,  # Gold
);

# XP bonus spells applied alongside fellowship buff
my @XP_SPELLS = (13088, 13089);
my %TIER_XP_SPELL = (
    1 => 13088,  # Potion of Adventure I (+10% XP)
    2 => 13089,  # Potion of Adventure II (+25% XP)
    3 => 13089,  # Potion of Adventure II (+25% XP)
);

my %TIER_NAME = (
    0 => 'None',
    1 => 'Bronze',
    2 => 'Silver',
    3 => 'Gold',
);
my %TIER_UNIQUE = (
    1 => 2,  # Bronze requires 2 unique
    2 => 3,  # Silver requires 3 unique
    3 => 4,  # Gold requires 4+ unique
);

# Bonus shard drop chance per tier (1/N per kill)
my %TIER_SHARD_CHANCE = (
    1 => 60,   # 1/60  ~1.7%
    2 => 40,   # 1/40  ~2.5%
    3 => 30,   # 1/30  ~3.3%
);

# Bonus named loot pool chance per tier (percent)
my %TIER_LOOT_CHANCE = (
    1 => 3,    # 3%
    2 => 7,    # 7%
    3 => 12,   # 12%
);

# Mob strength multiplier per tier (HP, min_hit, max_hit, AC)
my %TIER_MOB_SCALE = (
    1 => 1.05,  # +5%
    2 => 1.10,  # +10%
    3 => 1.15,  # +15%
);

my $ASCENDANT_SHARD_ID = 9600;

my $CACHE_TTL     = 60;   # seconds to cache IP cross-ref result
my $DEBUG_ENABLED = 1;    # set to 0 to silence debug messages

# ============================================================
# DEBUG HELPER
# ============================================================
sub Fellowship_Debug {
    return unless $DEBUG_ENABLED;
    my ($client, $msg) = @_;
    quest::debug("[Fellowship] $msg");
}

# ============================================================
# TIER CALCULATION
# ============================================================
sub Fellowship_GetTier {
    my ($unique_count) = @_;
    return 3 if $unique_count >= 4;
    return 2 if $unique_count >= 3;
    return 1 if $unique_count >= 2;
    return 0;
}

# ============================================================
# IP CROSS-REFERENCE: Count unique players in group
# ============================================================
# Queries account_ip table for all group members' accounts.
# Uses union-find to merge accounts that share any historical IP.
# Returns the number of independent account clusters.
# ============================================================

sub Fellowship_CountUniquePlayers {
    my ($client) = @_;
    return 1 unless $client && $client->IsGrouped();

    # Gather group members
    my @members = plugin::GetGroupMembers($client);
    return 1 if scalar(@members) <= 1;

    # Collect unique account IDs
    my %acct_map;  # acct_id => client ref
    foreach my $member (@members) {
        next unless $member;
        my $acct_id = $member->AccountID();
        $acct_map{$acct_id} = $member;
    }

    my @acct_ids = keys %acct_map;
    my $acct_count = scalar(@acct_ids);
    return 1 if $acct_count <= 1;

    # Check cache: key is sorted account IDs joined
    my $cache_key = "fellowship_cache_" . join("_", sort @acct_ids);
    my $cached = quest::get_data($cache_key);
    if ($cached && $cached =~ /^\d+$/) {
        Fellowship_Debug($client, "Cache hit: $cached unique players (key=$cache_key)");
        return int($cached);
    }

    # Query account_ip for all accounts in the group
    my $dbh = plugin::LoadMysql();
    unless ($dbh) {
        Fellowship_Debug($client, "DB connection failed, falling back to account count");
        return $acct_count;
    }

    my $placeholders = join(",", map { "?" } @acct_ids);
    my $sth = $dbh->prepare(
        "SELECT accid, ip FROM account_ip WHERE accid IN ($placeholders)"
    );
    $sth->execute(@acct_ids);

    # Build IP sets per account
    my %ip_sets;  # acct_id => { ip1 => 1, ip2 => 1, ... }
    while (my $row = $sth->fetchrow_hashref()) {
        $ip_sets{$row->{accid}}{$row->{ip}} = 1;
    }
    $sth->finish();
    $dbh->disconnect();

    # Union-find: merge accounts that share any IP
    my %parent;
    foreach my $id (@acct_ids) {
        $parent{$id} = $id;
    }

    # Find with path compression
    my $find;
    $find = sub {
        my ($x) = @_;
        if ($parent{$x} != $x) {
            $parent{$x} = $find->($parent{$x});
        }
        return $parent{$x};
    };

    # Union
    my $union = sub {
        my ($a, $b) = @_;
        my $ra = $find->($a);
        my $rb = $find->($b);
        $parent{$ra} = $rb if $ra != $rb;
    };

    # Compare each pair of accounts for IP overlap
    for (my $i = 0; $i < $acct_count; $i++) {
        for (my $j = $i + 1; $j < $acct_count; $j++) {
            my $a = $acct_ids[$i];
            my $b = $acct_ids[$j];
            # Check if they share any IP
            if (exists $ip_sets{$a} && exists $ip_sets{$b}) {
                foreach my $ip (keys %{$ip_sets{$a}}) {
                    if (exists $ip_sets{$b}{$ip}) {
                        $union->($a, $b);
                        Fellowship_Debug($client, "Merged acct $a and $b (shared IP $ip)");
                        last;
                    }
                }
            }
        }
    }

    # Count unique roots
    my %roots;
    foreach my $id (@acct_ids) {
        $roots{$find->($id)} = 1;
    }
    my $unique_count = scalar(keys %roots);

    # Cache the result
    quest::set_data($cache_key, $unique_count, $CACHE_TTL);
    Fellowship_Debug($client, "Computed $unique_count unique players from $acct_count accounts (cached ${CACHE_TTL}s)");

    return $unique_count;
}

# ============================================================
# BUFF APPLICATION
# ============================================================
# Called from EVENT_GROUP_CHANGE and EVENT_ENTERZONE.
# Evaluates group composition and applies/upgrades/removes buff.
# ============================================================

sub Fellowship_ApplyBuff {
    my ($client) = @_;
    return unless $client;

    my $unique = Fellowship_CountUniquePlayers($client);
    my $new_tier = Fellowship_GetTier($unique);

    # Check current tier (stored as entity variable, resets on zone)
    my $current_tier = $client->GetEntityVariable("fellowship_tier") || 0;

    Fellowship_Debug($client, "Evaluating: unique=$unique new_tier=$new_tier current_tier=$current_tier");

    # No change needed
    if ($new_tier == $current_tier) {
        return;
    }

    # Fade all existing fellowship buffs + XP spells
    foreach my $spell_id (@FELLOWSHIP_SPELLS, @XP_SPELLS) {
        $client->BuffFadeBySpellID($spell_id);
    }

    if ($new_tier > 0) {
        # Apply new tier buff (600 ticks = 60 min)
        my $spell_id = $TIER_SPELL{$new_tier};
        $client->ApplySpell($spell_id, 600);

        # Apply XP bonus spell
        my $xp_spell = $TIER_XP_SPELL{$new_tier};
        $client->ApplySpell($xp_spell, 600) if $xp_spell;

        $client->SetEntityVariable("fellowship_tier", $new_tier);

        # Announce tier change
        my $tier_name = $TIER_NAME{$new_tier};
        $client->Message(18, "Fellowship Bonus: $tier_name! Grouped with $unique unique adventurers.");
        Fellowship_Debug($client, "Applied spell $spell_id + XP spell $xp_spell ($tier_name tier)");
    } else {
        $client->SetEntityVariable("fellowship_tier", 0);
        Fellowship_Debug($client, "No fellowship bonus (unique=$unique)");
    }
}

# ============================================================
# FADE ALL BUFFS (for leaving group / going solo)
# ============================================================
sub Fellowship_FadeAll {
    my ($client) = @_;
    return unless $client;

    foreach my $spell_id (@FELLOWSHIP_SPELLS, @XP_SPELLS) {
        $client->BuffFadeBySpellID($spell_id);
    }
    $client->SetEntityVariable("fellowship_tier", 0);
    Fellowship_Debug($client, "Faded all fellowship buffs");
}

# ============================================================
# GET CURRENT TIER (for other systems to query)
# ============================================================
sub Fellowship_GetCurrentTier {
    my ($client) = @_;
    return 0 unless $client;
    return $client->GetEntityVariable("fellowship_tier") || 0;
}

# ============================================================
# MOB STRENGTH SCALING — called from EVENT_COMBAT after engage
# Applies +5/10/15% to HP, min_hit, max_hit, AC based on tier
# ============================================================
sub Fellowship_ScaleMob {
    my ($npc, $tier) = @_;
    return unless $npc && $tier && $tier > 0;
    return if $npc->GetEntityVariable('fellowship_scaled');
    return if $npc->IsRaidTarget();

    my $mult = $TIER_MOB_SCALE{$tier};
    return unless $mult && $mult > 1.0;

    # Snapshot current stats (after encounter scaling if applicable)
    my $hp      = $npc->GetMaxHP();
    my $min_hit = $npc->GetMinDMG();
    my $max_hit = $npc->GetMaxDMG();
    my $ac      = $npc->GetAC();

    $npc->SetEntityVariable('fellowship_pre_hp',      $hp);
    $npc->SetEntityVariable('fellowship_pre_min_hit',  $min_hit);
    $npc->SetEntityVariable('fellowship_pre_max_hit',  $max_hit);
    $npc->SetEntityVariable('fellowship_pre_ac',       $ac);

    # Apply multiplier
    my $new_hp      = int($hp * $mult);
    my $new_min_hit = int($min_hit * $mult);
    my $new_max_hit = int($max_hit * $mult);
    my $new_ac      = int($ac * $mult);

    my $hp_ratio = $npc->GetHPRatio();
    $npc->ModifyNPCStat('max_hp',  "$new_hp");
    my $scaled_hp = int($npc->GetMaxHP() * $hp_ratio / 100);
    $scaled_hp = 1 if $scaled_hp < 1;
    $npc->SetHP($scaled_hp);
    $npc->ModifyNPCStat('min_hit', "$new_min_hit");
    $npc->ModifyNPCStat('max_hit', "$new_max_hit");
    $npc->ModifyNPCStat('ac',      "$new_ac");

    $npc->SetEntityVariable('fellowship_scaled', '1');
    $npc->SetEntityVariable('fellowship_scale_tier', "$tier");

    quest::debug("[Fellowship] ScaleMob: " . $npc->GetCleanName()
        . " tier=$tier mult=$mult HP=$hp->$new_hp melee=$min_hit-$max_hit->$new_min_hit-$new_max_hit AC=$ac->$new_ac");
}

# ============================================================
# MOB RESTORE — called from EVENT_COMBAT on disengage
# ============================================================
sub Fellowship_RestoreMob {
    my ($npc) = @_;
    return unless $npc;
    return unless $npc->GetEntityVariable('fellowship_scaled');

    my $hp      = $npc->GetEntityVariable('fellowship_pre_hp');
    my $min_hit = $npc->GetEntityVariable('fellowship_pre_min_hit');
    my $max_hit = $npc->GetEntityVariable('fellowship_pre_max_hit');
    my $ac      = $npc->GetEntityVariable('fellowship_pre_ac');

    $npc->ModifyNPCStat('max_hp',  "$hp")      if $hp;
    $npc->ModifyNPCStat('min_hit', "$min_hit")  if $min_hit;
    $npc->ModifyNPCStat('max_hit', "$max_hit")  if $max_hit;
    $npc->ModifyNPCStat('ac',      "$ac")       if $ac;

    $npc->DeleteEntityVariable('fellowship_scaled');
    $npc->DeleteEntityVariable('fellowship_scale_tier');
    $npc->DeleteEntityVariable('fellowship_pre_hp');
    $npc->DeleteEntityVariable('fellowship_pre_min_hit');
    $npc->DeleteEntityVariable('fellowship_pre_max_hit');
    $npc->DeleteEntityVariable('fellowship_pre_ac');

    quest::debug("[Fellowship] RestoreMob: " . $npc->GetCleanName() . " — stats restored");
}

# ============================================================
# LOOT BONUSES — called from EVENT_DEATH
# Rolls bonus shard and named loot pool item based on tier
# ============================================================
sub Fellowship_BonusLoot {
    my ($npc, $client, $tier) = @_;
    return unless $npc && $client && $tier && $tier > 0;

    my $npc_lvl = $npc->GetLevel();
    my $zoneid  = $npc->GetZoneID();

    # Bonus shard roll
    my $shard_denom = $TIER_SHARD_CHANCE{$tier};
    if ($shard_denom && int(rand($shard_denom)) == 0) {
        $npc->AddItem($ASCENDANT_SHARD_ID, 1);
        quest::debug("[Fellowship] Bonus shard! tier=$tier (1/$shard_denom) npc=" . $npc->GetCleanName());
    }

    # Bonus named loot pool roll
    my $loot_pct = $TIER_LOOT_CHANCE{$tier};
    if ($loot_pct && int(rand(100)) < $loot_pct) {
        my @item_pool = plugin::get_merged_pool($npc_lvl, $zoneid);
        if (scalar(@item_pool) > 0) {
            my $bonus_item = $item_pool[int(rand(scalar(@item_pool)))];
            $npc->AddItem($bonus_item, 1);
            quest::debug("[Fellowship] Bonus named loot! item=$bonus_item tier=$tier ($loot_pct%) npc=" . $npc->GetCleanName());
        }
    }
}

1;
