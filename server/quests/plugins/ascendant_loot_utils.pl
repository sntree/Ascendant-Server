##############################################################################
# Ascendant Loot Utils - Config + Shared Logic
##############################################################################
# Active expansion config, level block resolution, tier upgrade rolls,
# and pool merging. All functions are subs (plugin requirement).
##############################################################################

#=============================================================================
# CONFIGURATION - Active Expansions
#=============================================================================
# Add expansion names here as they unlock. Order matters for level block
# priority (later expansions override earlier ones for overlapping ranges).

sub active_expansions {
    return ('classic', 'kunark', 'velious', 'ldon', 'luclin', 'pop', 'loy', 'god');
}

#=============================================================================
# CONFIGURATION - Tier Upgrade Chances
#=============================================================================

sub rare_tier_config {
    return {
        enable    => 1,
        t3_chance => 0.06,   # 6% Ascendant
        t2_chance => 0.15,   # 15% Exalted
        t1_chance => 0.30,   # 30% Greater
        max_rare_upgrades => 1,  # Max T2/T3 per NPC
    };
}

sub raid_tier_config {
    return {
        enable    => 1,
        t3_chance => 0.10,   # 10% Ascendant
        t2_chance => 0.20,   # 20% Exalted
        t1_chance => 0.40,   # 40% Greater
        max_rare_upgrades => 1,  # Max T2/T3 per boss
    };
}

sub common_tier_config {
    return {
        enable    => 1,
        t3_chance => 0.06,   # 6% Ascendant
        t2_chance => 0.15,   # 15% Exalted
        t1_chance => 0.30,   # 30% Greater
        max_rare_upgrades => 1,
    };
}

#=============================================================================
# Dispatch helpers + zone-to-expansion resolution
#=============================================================================

sub _expansion_pool_dispatch {
    my $exp = shift;
    if ($exp eq 'classic') { return plugin::classic_rare_pools(); }
    if ($exp eq 'kunark')  { return plugin::kunark_rare_pools(); }
    if ($exp eq 'velious') { return plugin::velious_rare_pools(); }
    if ($exp eq 'ldon')    { return plugin::ldon_rare_pools(); }
    if ($exp eq 'luclin')  { return plugin::luclin_rare_pools(); }
    if ($exp eq 'pop')     { return plugin::pop_rare_pools(); }
    if ($exp eq 'loy')     { return plugin::loy_rare_pools(); }
    if ($exp eq 'god')     { return plugin::god_rare_pools(); }
    return undef;
}

sub _expansion_blocks_dispatch {
    my $exp = shift;
    if ($exp eq 'classic') { return plugin::classic_level_blocks(); }
    if ($exp eq 'kunark')  { return plugin::kunark_level_blocks(); }
    if ($exp eq 'velious') { return plugin::velious_level_blocks(); }
    if ($exp eq 'ldon')    { return plugin::ldon_level_blocks(); }
    if ($exp eq 'luclin')  { return plugin::luclin_level_blocks(); }
    if ($exp eq 'pop')     { return plugin::pop_level_blocks(); }
    if ($exp eq 'loy')     { return plugin::loy_level_blocks(); }
    if ($exp eq 'god')     { return plugin::god_level_blocks(); }
    return undef;
}

sub _expansion_raid_dispatch {
    my $exp = shift;
    if ($exp eq 'classic') { return plugin::classic_raid_config(); }
    if ($exp eq 'kunark')  { return plugin::kunark_raid_config(); }
    if ($exp eq 'luclin')  { return plugin::luclin_raid_config(); }
    return undef;
}

sub _expansion_zone_dispatch {
    my $exp = shift;
    if ($exp eq 'classic') { return plugin::classic_zone_ids(); }
    if ($exp eq 'kunark')  { return plugin::kunark_zone_ids(); }
    if ($exp eq 'velious') { return plugin::velious_zone_ids(); }
    if ($exp eq 'ldon')    { return plugin::ldon_zone_ids(); }
    if ($exp eq 'luclin')  { return plugin::luclin_zone_ids(); }
    if ($exp eq 'pop')     { return plugin::pop_zone_ids(); }
    if ($exp eq 'loy')     { return plugin::loy_zone_ids(); }
    if ($exp eq 'god')     { return plugin::god_zone_ids(); }
    return undef;
}

sub _expansion_raid_pool_dispatch {
    my $exp = shift;
    if ($exp eq 'velious') { return plugin::velious_raid_pools(); }
    if ($exp eq 'luclin')  { return plugin::luclin_raid_pools(); }
    if ($exp eq 'pop')     { return plugin::pop_raid_pools(); }
    if ($exp eq 'loy')     { return plugin::loy_raid_pools(); }
    if ($exp eq 'god')     { return plugin::god_raid_pools(); }
    return undef;
}

sub _expansion_raid_blocks_dispatch {
    my $exp = shift;
    if ($exp eq 'velious') { return plugin::velious_raid_level_blocks(); }
    if ($exp eq 'luclin')  { return plugin::luclin_raid_level_blocks(); }
    if ($exp eq 'pop')     { return plugin::pop_raid_level_blocks(); }
    if ($exp eq 'loy')     { return plugin::loy_raid_level_blocks(); }
    if ($exp eq 'god')     { return plugin::god_raid_level_blocks(); }
    return undef;
}

sub _expansion_raid_overrides_dispatch {
    my $exp = shift;
    if ($exp eq 'pop') { return plugin::pop_raid_block_overrides(); }
    if ($exp eq 'god') { return plugin::god_raid_block_overrides(); }
    return undef;
}

#=============================================================================
# zone_to_expansion($zoneid) - Returns which expansion a zone belongs to
# Returns expansion name string or undef if zone not mapped (no bonus loot)
#=============================================================================

sub zone_to_expansion {
    my $zoneid = shift;
    return undef unless $zoneid;

    foreach my $exp (plugin::active_expansions()) {
        my $zones = _expansion_zone_dispatch($exp);
        next unless $zones;
        return $exp if exists $zones->{$zoneid};
    }

    # Unknown zone = no bonus loot
    return undef;
}

sub excluded_bonus_loot_item_ids {
    return {
        # Polluted source loottables that can leak low-era junk through
        # rare_spawn loottable rolls.
        1322   => 1, # Thaumaturgist's Robe
        6026   => 1, # Shovel

        # Epic 1.0 weapons should only come from their quest paths, never bonus loot.
        1683   => 1, # Celestial Fists
        5532   => 1, # Water Sprinkler of Nem Ankh
        8495   => 1, # Claw of the Savage Spirit
        8496   => 1, # Claw of the Savage Spirit
        10099  => 1, # Fiery Defender
        10650  => 1, # Staff of the Serpent
        10651  => 1, # Spear of Fate
        10652  => 1, # Celestial Fists
        10908  => 1, # Jagged Blade of War
        11057  => 1, # Ragebringer
        14341  => 1, # Staff of the Four
        14383  => 1, # Innoruuk's Curse
        20487  => 1, # Swiftwind
        20488  => 1, # Earthcaller
        20490  => 1, # Nature Walkers Scimitar
        20542  => 1, # Singing Short Sword
        20544  => 1, # Scythe of the Shadowed Soul
        28034  => 1, # Orb of Mastery
        36223  => 1, # Spear of Fate
        36224  => 1, # Celestial Fists
        66175  => 1, # Jagged Blade of War

        # PoP key/flag items must come from their intended source, never bonus loot.
        9433   => 1, # Symbol of Torden (Agnarr's Tower key, bothunder)

        # GoD Uqua trinkets (block the Aura of Destruction) — never bonus loot (owner 2026-09-26).
        67736  => 1, 67737  => 1, 67738  => 1, 67739  => 1,
        367736 => 1, 367737 => 1, 367738 => 1, 367739 => 1,
        567736 => 1, 567737 => 1, 567738 => 1, 567739 => 1,
        767736 => 1, 767737 => 1, 767738 => 1, 767739 => 1,

        310652 => 1, 510652 => 1, 710652 => 1,
        314383 => 1, 514383 => 1, 714383 => 1,
        320490 => 1, 520490 => 1, 720490 => 1,
        320544 => 1, 520544 => 1, 720544 => 1,
    };
}

sub is_bonus_loot_item_excluded {
    my $item_id = shift;
    return 0 unless $item_id;

    my $excluded = plugin::excluded_bonus_loot_item_ids();
    return $excluded->{$item_id} ? 1 : 0;
}

sub _filter_bonus_loot_pool {
    my @item_ids = @_;
    my $excluded = plugin::excluded_bonus_loot_item_ids();

    return grep { $_ && !$excluded->{$_} } @item_ids;
}

sub get_merged_pool {
    my $level  = shift;
    my $zoneid = shift;
    my @merged = ();

    # Determine which expansion this zone belongs to
    my $zone_exp = plugin::zone_to_expansion($zoneid);

    # Unknown zone = no bonus loot
    return @merged unless $zone_exp;

    # Only pull items from that expansion's pools
    my $pools  = _expansion_pool_dispatch($zone_exp);
    my $blocks = _expansion_blocks_dispatch($zone_exp);
    if ($pools && $blocks) {
        foreach my $b (@$blocks) {
            if ($level >= $b->{min} && $level <= $b->{max}) {
                my $block_id = $b->{block};
                if (exists $pools->{$block_id}) {
                    push @merged, @{$pools->{$block_id}};
                }
            }
        }
    }

    @merged = plugin::_filter_bonus_loot_pool(@merged);
    return @merged;
}

#=============================================================================
# Mischief loottable-roll expansion gate
# PoP starts the "roll another rare_spawn NPC loottable" system. Earlier
# expansions keep using the static item-pool Mischief behavior.
#=============================================================================

sub uses_rare_loottable_mischief {
    my $zoneid = shift;

    my $zone_exp = plugin::zone_to_expansion($zoneid);
    return 0 unless $zone_exp;

    my $pop_or_later = 0;
    foreach my $exp (plugin::active_expansions()) {
        $pop_or_later = 1 if $exp eq 'pop';
        return 1 if $pop_or_later && $exp eq $zone_exp;
    }

    return 0;
}

sub is_pop_mischief_debug_zone {
    my $zoneid = shift;

    my $zone_exp = plugin::zone_to_expansion($zoneid);
    return ($zone_exp && $zone_exp eq 'pop') ? 1 : 0;
}

sub pop_mischief_zone_shout {
    my $zoneid  = shift;
    my $message = shift;

    return;
    return unless plugin::is_pop_mischief_debug_zone($zoneid);
    return unless $message;

    quest::ze(15, "[PoP Mischief] " . $message);
}

sub _loot_item_counts {
    my $npc = shift;
    my %counts = ();

    return \%counts unless $npc;

    foreach my $item_id ($npc->GetLootList()) {
        next unless $item_id && $item_id > 0;
        $counts{$item_id}++;
    }

    return \%counts;
}

sub _loot_item_delta {
    my $before = shift || {};
    my $after  = shift || {};
    my @added = ();

    foreach my $item_id (sort { $a <=> $b } keys %$after) {
        my $count = ($after->{$item_id} || 0) - ($before->{$item_id} || 0);
        next unless $count > 0;
        push @added, { item_id => $item_id, count => $count };
    }

    return @added;
}

sub _item_names_for_ids {
    my @item_ids = @_;
    my %names = ();

    return \%names unless @item_ids;

    my $dbh = plugin::get_loot_dbh();
    return \%names unless $dbh;

    my $placeholders = join(',', ('?') x scalar(@item_ids));
    my $sql = "SELECT id, Name FROM items WHERE id IN ($placeholders)";
    my $sth = $dbh->prepare($sql);
    $sth->execute(@item_ids);

    while (my ($item_id, $name) = $sth->fetchrow_array()) {
        $names{$item_id} = $name if $item_id;
    }

    $sth->finish();
    return \%names;
}

sub format_bonus_roll_items {
    my $items = shift || [];
    return "no items" unless @$items;

    my @item_ids = map { $_->{item_id} } @$items;
    my $names = plugin::_item_names_for_ids(@item_ids);
    my @parts = ();
    my $shown = 0;
    my $max_items_to_show = 5;

    foreach my $item (@$items) {
        last if $shown >= $max_items_to_show;

        my $item_id = $item->{item_id};
        my $count = $item->{count} || 1;
        my $name = $names->{$item_id} || ("item " . $item_id);
        $name =~ s/\s+/ /g;

        my $part = $name . " (" . $item_id . ")";
        $part .= " x" . $count if $count > 1;
        push @parts, $part;
        $shown++;
    }

    if (scalar(@$items) > $max_items_to_show) {
        push @parts, "+" . (scalar(@$items) - $max_items_to_show) . " more";
    }

    return join(', ', @parts);
}

sub format_rare_spawn_source {
    my $source = shift;
    return "no source" unless $source;

    my $name = $source->{name} || "unknown";
    $name =~ s/^#//;
    $name =~ s/_/ /g;

    my $zone = $source->{short_name} || "unknown";
    my $level = $source->{level} || "?";
    my $loottable_id = $source->{loottable_id} || 0;
    my $id = $source->{id} || 0;

    return $name . " [npc " . $id . ", " . $zone . ", L" . $level . ", table " . $loottable_id . "]";
}

sub format_bonus_roll_attempts {
    my $source = shift;
    return "" unless $source;

    my $attempts = $source->{attempts} || 1;
    return "" unless $attempts > 1;

    return " after " . $attempts . " tries";
}

sub format_bonus_roll_fallback {
    my $source = shift;
    return "" unless $source && $source->{forced_item};

    return " with fallback item";
}

my %_common_rare_item_cache;

sub _rare_pool_zone_ids {
    my $zone_exp = shift;

    my $zones = _expansion_zone_dispatch($zone_exp);
    return () unless $zones;

    my @zone_ids = sort { $a <=> $b } keys %$zones;
    if ($zone_exp eq 'pop') {
        @zone_ids = grep { $_ != 202 && $_ != 203 } @zone_ids; # exclude PoK/PoTranq hubs
    }
    if ($zone_exp eq 'god') {
        @zone_ids = grep { $_ != 279 } @zone_ids; # exclude Abysmal Sea hub
    }

    return @zone_ids;
}

sub common_rare_bonus_item_ids {
    my $level     = shift;
    my $zoneid    = shift;
    my $threshold = shift || 5;

    my $zone_exp = plugin::zone_to_expansion($zoneid);
    return {} unless $zone_exp;

    my $block = _rare_block_for_level($zone_exp, $level);
    return {} unless $block;

    my @zone_ids = plugin::_rare_pool_zone_ids($zone_exp);
    return {} unless @zone_ids;

    my $cache_key = join(':', $zone_exp, $block->{block}, $threshold);
    return $_common_rare_item_cache{$cache_key} if exists $_common_rare_item_cache{$cache_key};

    my $placeholders = join(',', ('?') x scalar(@zone_ids));
    my @excluded_item_ids = sort { $a <=> $b } keys %{plugin::excluded_bonus_loot_item_ids()};
    my $excluded_filter_sql = '';
    if (@excluded_item_ids) {
        my $excluded_placeholders = join(',', ('?') x scalar(@excluded_item_ids));
        $excluded_filter_sql = "AND lde.item_id NOT IN ($excluded_placeholders)";
    }

    my $sql = qq{
        SELECT
            lde.item_id,
            COUNT(DISTINCT n.id) AS source_npc_count
        FROM npc_types n
        JOIN loottable_entries lte ON lte.loottable_id = n.loottable_id
        JOIN lootdrop_entries lde ON lde.lootdrop_id = lte.lootdrop_id
        JOIN spawnentry se ON se.npcid = n.id
        JOIN spawn2 s ON s.spawngroupID = se.spawngroupID
        JOIN zone z ON z.short_name = s.zone
        WHERE z.zoneidnumber IN ($placeholders)
          AND n.rare_spawn <> 0
          AND n.raid_target = 0
          AND n.loottable_id > 0
          AND n.level BETWEEN ? AND ?
          AND lde.item_id > 0
          AND lde.chance > 0
          $excluded_filter_sql
        GROUP BY lde.item_id
        HAVING COUNT(DISTINCT n.id) > ?
    };

    my %common = ();
    my $dbh = plugin::get_loot_dbh();
    if ($dbh) {
        my $sth = $dbh->prepare($sql);
        $sth->execute(@zone_ids, $block->{min}, $block->{max}, @excluded_item_ids, $threshold);
        while (my ($item_id) = $sth->fetchrow_array()) {
            $common{$item_id} = 1 if $item_id;
        }
        $sth->finish();
    }

    $_common_rare_item_cache{$cache_key} = \%common;
    return \%common;
}

sub _bonus_roll_has_non_common_item {
    my $items = shift || [];
    my $common_item_ids = shift || {};

    foreach my $item (@$items) {
        my $item_id = $item->{item_id};
        next unless $item_id;
        return 1 unless $common_item_ids->{$item_id};
    }

    return 0;
}

sub _remove_added_loot_items {
    my $npc = shift;
    my $items = shift || [];

    return unless $npc;

    foreach my $item (@$items) {
        my $item_id = $item->{item_id};
        my $count = $item->{count} || 1;
        next unless $item_id && $item_id > 0;

        for (my $i = 0; $i < $count; $i++) {
            $npc->RemoveItem($item_id);
        }
    }
}

sub _remove_excluded_bonus_loot_items {
    my $npc = shift;
    my $items = shift || [];

    my @kept = ();
    my @removed = ();

    foreach my $item (@$items) {
        my $item_id = $item->{item_id};
        if (plugin::is_bonus_loot_item_excluded($item_id)) {
            push @removed, $item;
        }
        else {
            push @kept, $item;
        }
    }

    plugin::_remove_added_loot_items($npc, \@removed) if @removed;
    return (\@kept, \@removed);
}

sub _add_forced_loottable_item {
    my $npc = shift;
    my $loottable_id = shift;
    my $common_item_ids = shift || {};

    return 0 unless $npc;
    return 0 unless $loottable_id;

    my $dbh = plugin::get_loot_dbh();
    return 0 unless $dbh;

    my %filtered_item_ids = map { $_ => 1 } keys %{plugin::excluded_bonus_loot_item_ids()};
    foreach my $item_id (keys %$common_item_ids) {
        $filtered_item_ids{$item_id} = 1 if $item_id;
    }

    my @filtered_item_ids = sort { $a <=> $b } keys %filtered_item_ids;
    my $item_filter_sql = '';
    if (@filtered_item_ids) {
        my $item_placeholders = join(',', ('?') x scalar(@filtered_item_ids));
        $item_filter_sql = "AND lde.item_id NOT IN ($item_placeholders)";
    }

    my $sql = q{
        SELECT
            lde.item_id,
            lde.item_charges,
            (lte.probability * lde.chance) AS weight
        FROM loottable_entries lte
        JOIN lootdrop_entries lde ON lde.lootdrop_id = lte.lootdrop_id
        WHERE lte.loottable_id = ?
          AND lde.item_id > 0
          AND lde.chance > 0
    } . "\n          $item_filter_sql" . q{
        ORDER BY lte.lootdrop_id, lde.item_id
    };

    my $sth = $dbh->prepare($sql);
    $sth->execute($loottable_id, @filtered_item_ids);

    my @items = ();
    my $total_weight = 0;
    while (my $row = $sth->fetchrow_hashref()) {
        my $weight = $row->{weight} || 0;
        next unless $weight > 0;

        $total_weight += $weight;
        push @items, {
            item_id => $row->{item_id},
            charges => $row->{item_charges} || 1,
            weight  => $weight,
        };
    }

    $sth->finish();
    return 0 unless @items && $total_weight > 0;

    my $roll = rand($total_weight);
    foreach my $item (@items) {
        $roll -= $item->{weight};
        if ($roll <= 0) {
            $npc->AddItem($item->{item_id}, $item->{charges});
            return 1;
        }
    }

    my $last_item = $items[-1];
    $npc->AddItem($last_item->{item_id}, $last_item->{charges});
    return 1;
}

#=============================================================================
# add_rare_spawn_loottable_roll($npc, $level, $zoneid)
# Rolls one natural loottable from a random non-raid rare_spawn NPC in the same
# expansion and rare level block. This is the Mischief-style named loot path.
#=============================================================================

sub _rare_block_for_level {
    my $exp   = shift;
    my $level = shift;

    my $blocks = _expansion_blocks_dispatch($exp);
    return undef unless $blocks;

    foreach my $b (@$blocks) {
        if ($level >= $b->{min} && $level <= $b->{max}) {
            return $b;
        }
    }

    return undef;
}

sub get_random_rare_spawn_loottable {
    my $level          = shift;
    my $zoneid         = shift;
    my $exclude_npc_id = shift || 0;
    my $common_item_ids = shift || {};

    my $zone_exp = plugin::zone_to_expansion($zoneid);
    return undef unless $zone_exp;
    return undef unless plugin::uses_rare_loottable_mischief($zoneid);

    my $block = _rare_block_for_level($zone_exp, $level);
    return undef unless $block;

    my @zone_ids = plugin::_rare_pool_zone_ids($zone_exp);
    return undef unless @zone_ids;

    my $placeholders = join(',', ('?') x scalar(@zone_ids));

    my @exclude_npc_ids = ();
    if (ref($exclude_npc_id) eq 'ARRAY') {
        @exclude_npc_ids = grep { $_ && $_ > 0 } @$exclude_npc_id;
    }
    elsif ($exclude_npc_id && $exclude_npc_id > 0) {
        push @exclude_npc_ids, $exclude_npc_id;
    }

    my $exclude_sql = '';
    if (@exclude_npc_ids) {
        my $exclude_placeholders = join(',', ('?') x scalar(@exclude_npc_ids));
        $exclude_sql = "AND n.id NOT IN ($exclude_placeholders)";
    }

    my %filtered_item_ids = map { $_ => 1 } keys %{plugin::excluded_bonus_loot_item_ids()};
    foreach my $item_id (keys %$common_item_ids) {
        $filtered_item_ids{$item_id} = 1 if $item_id;
    }

    my @filtered_item_ids = sort { $a <=> $b } keys %filtered_item_ids;
    my $item_filter_sql = '';
    if (@filtered_item_ids) {
        my $item_placeholders = join(',', ('?') x scalar(@filtered_item_ids));
        $item_filter_sql = "AND lde.item_id NOT IN ($item_placeholders)";
    }

    my $sql = qq{
        SELECT DISTINCT
            n.id,
            n.name,
            n.level,
            n.loottable_id,
            z.zoneidnumber,
            z.short_name
        FROM npc_types n
        JOIN loottable_entries lte ON lte.loottable_id = n.loottable_id
        JOIN lootdrop_entries lde ON lde.lootdrop_id = lte.lootdrop_id
        JOIN spawnentry se ON se.npcid = n.id
        JOIN spawn2 s ON s.spawngroupID = se.spawngroupID
        JOIN zone z ON z.short_name = s.zone
        WHERE z.zoneidnumber IN ($placeholders)
          AND n.rare_spawn <> 0
          AND n.raid_target = 0
          AND n.loottable_id > 0
          AND lde.item_id > 0
          AND lde.chance > 0
          $item_filter_sql
          AND n.level BETWEEN ? AND ?
          $exclude_sql
        ORDER BY RAND()
        LIMIT 1
    };

    my $dbh = plugin::get_loot_dbh();
    return undef unless $dbh;

    my @bind = (@zone_ids, @filtered_item_ids, $block->{min}, $block->{max});
    push @bind, @exclude_npc_ids if @exclude_npc_ids;

    my $sth = $dbh->prepare($sql);
    $sth->execute(@bind);
    my $row = $sth->fetchrow_hashref();
    $sth->finish();

    return $row;
}

sub add_rare_spawn_loottable_roll {
    my $npc    = shift;
    my $level  = shift;
    my $zoneid = shift;
    my $require_item = shift || 0;
    my $max_attempts = shift || 1;
    my $common_item_threshold = shift || 0;

    return undef unless $npc;

    $max_attempts = 1 unless $require_item;
    $max_attempts = 1 unless $max_attempts && $max_attempts > 0;

    my @exclude_npc_ids = ($npc->GetNPCTypeID());
    my $last_source;
    my $common_item_ids = {};
    if ($require_item && $common_item_threshold && $common_item_threshold > 0) {
        $common_item_ids = plugin::common_rare_bonus_item_ids($level, $zoneid, $common_item_threshold);
    }

    my $use_common_gate = (%$common_item_ids) ? 1 : 0;

    for (my $attempt = 1; $attempt <= $max_attempts; $attempt++) {
        my $source = plugin::get_random_rare_spawn_loottable($level, $zoneid, \@exclude_npc_ids, $common_item_ids);
        last unless $source && $source->{loottable_id};

        push @exclude_npc_ids, $source->{id} if $source->{id};

        my $before_loot = plugin::_loot_item_counts($npc);
        my $copper      = $npc->GetCopper();
        my $silver      = $npc->GetSilver();
        my $gold        = $npc->GetGold();
        my $platinum    = $npc->GetPlatinum();

        $npc->AddLootTable($source->{loottable_id});

        # AddLootTable(id) uses the normal NPC path and rerolls coin; bonus rolls
        # should only add items, so restore the corpse's original coin.
        $npc->SetCopper($copper);
        $npc->SetSilver($silver);
        $npc->SetGold($gold);
        $npc->SetPlatinum($platinum);

        my $after_loot = plugin::_loot_item_counts($npc);
        my @added_items = plugin::_loot_item_delta($before_loot, $after_loot);
        my ($kept_items, $removed_items) = plugin::_remove_excluded_bonus_loot_items($npc, \@added_items);
        @added_items = @$kept_items;
        $source->{added_items} = \@added_items;
        $source->{removed_excluded_items} = $removed_items if @$removed_items;
        $source->{attempts} = $attempt;

        return $source if !$require_item;

        if (@added_items) {
            if (!$use_common_gate || plugin::_bonus_roll_has_non_common_item(\@added_items, $common_item_ids)) {
                return $source;
            }

            plugin::_remove_added_loot_items($npc, \@added_items);
            $source->{added_items} = [];
            $source->{common_only_rejected} = 1;
        }

        $last_source = $source;
    }

    if ($last_source) {
        my $before_loot = plugin::_loot_item_counts($npc);
        if (plugin::_add_forced_loottable_item($npc, $last_source->{loottable_id}, $common_item_ids)) {
            my $after_loot = plugin::_loot_item_counts($npc);
            my @added_items = plugin::_loot_item_delta($before_loot, $after_loot);
            my ($kept_items, $removed_items) = plugin::_remove_excluded_bonus_loot_items($npc, \@added_items);
            @added_items = @$kept_items;

            if (@added_items) {
                $last_source->{added_items} = \@added_items;
                $last_source->{removed_excluded_items} = $removed_items if @$removed_items;
                $last_source->{forced_item} = 1;
                return $last_source;
            }
        }

        $last_source->{failed_to_add_item} = 1;
        return $last_source;
    }

    return undef;
}

#=============================================================================
# get_merged_raid_pool($level, $zoneid) - Level-banded raid pool (Velious)
#=============================================================================

sub get_merged_raid_pool {
    my $level  = shift;
    my $zoneid = shift;
    my $npc_id = shift;   # optional: enables per-NPC block overrides
    my @merged = ();

    my $zone_exp = plugin::zone_to_expansion($zoneid);
    return @merged unless $zone_exp;

    my $pools  = _expansion_raid_pool_dispatch($zone_exp);
    my $blocks = _expansion_raid_blocks_dispatch($zone_exp);
    return @merged unless ($pools && $blocks);

    # Per-NPC override takes precedence over level banding
    my $overrides = _expansion_raid_overrides_dispatch($zone_exp);
    if ($npc_id && $overrides && exists $overrides->{$npc_id}) {
        my $block_id = $overrides->{$npc_id};
        if (exists $pools->{$block_id}) {
            push @merged, @{$pools->{$block_id}};
        }
        return @merged;
    }

    foreach my $b (@$blocks) {
        if ($level >= $b->{min} && $level <= $b->{max}) {
            my $block_id = $b->{block};
            if (exists $pools->{$block_id}) {
                push @merged, @{$pools->{$block_id}};
            }
        }
    }

    return @merged;
}

#=============================================================================
# get_merged_raid_config() - Merge raid configs from all active expansions
#=============================================================================

sub get_merged_raid_config {
    my %merged_zones;
    my @merged_boss_names;
    my %merged_groups;
    my %merged_boss_groups;
    my $group_offset = 0;

    foreach my $exp (plugin::active_expansions()) {
        my $config = _expansion_raid_dispatch($exp);
        next unless $config;

        # Merge zones
        if ($config->{zones}) {
            foreach my $z (keys %{$config->{zones}}) {
                $merged_zones{$z} = 1;
            }
        }

        # Merge boss names
        if ($config->{boss_names}) {
            push @merged_boss_names, @{$config->{boss_names}};
        }

        # Merge groups (offset group IDs to avoid collisions)
        if ($config->{groups}) {
            foreach my $gid (keys %{$config->{groups}}) {
                my $new_gid = $gid + $group_offset;
                $merged_groups{$new_gid} = $config->{groups}{$gid};
            }
        }

        # Merge boss_groups mapping
        if ($config->{boss_groups}) {
            foreach my $bname (keys %{$config->{boss_groups}}) {
                my $new_gid = $config->{boss_groups}{$bname} + $group_offset;
                $merged_boss_groups{$bname} = $new_gid;
            }
        }

        # Offset for next expansion (use 100 per expansion to avoid collisions)
        $group_offset += 100;
    }

    return {
        zones       => \%merged_zones,
        boss_names  => \@merged_boss_names,
        groups      => \%merged_groups,
        boss_groups => \%merged_boss_groups,
    };
}

#=============================================================================
# roll_tier_upgrade($item_id, $tier_cfg, $rare_count_ref, $dbh)
# Returns: upgraded item ID (or original if no upgrade)
#=============================================================================

sub roll_tier_upgrade {
    my $item_id        = shift;
    my $tier_cfg       = shift;
    my $rare_count_ref = shift;  # reference to scalar tracking rare upgrades
    my $dbh            = shift;

    return $item_id unless $tier_cfg->{enable} && $dbh;

    my $sql = q{
        SELECT tier_code, variant_item_id
        FROM item_tier_map
        WHERE base_item_id = ?
        ORDER BY tier_code ASC
    };

    my $sth = $dbh->prepare($sql);
    $sth->execute($item_id);

    my %tier_variants;
    while (my ($tier_code, $variant_id) = $sth->fetchrow_array()) {
        $tier_variants{$tier_code} = $variant_id if $tier_code && $variant_id;
    }
    $sth->finish();

    return $item_id unless %tier_variants;

    my $max_rare = $tier_cfg->{max_rare_upgrades} || 1;

    # Roll for T3 (Ascendant)
    if (exists $tier_variants{3} && rand() < $tier_cfg->{t3_chance}) {
        if ($$rare_count_ref < $max_rare) {
            $$rare_count_ref++;
            quest::debug("Tier upgrade: T3 (Ascendant) -> item $tier_variants{3}");
            return $tier_variants{3};
        }
    }
    # Roll for T2 (Exalted)
    elsif (exists $tier_variants{2} && rand() < $tier_cfg->{t2_chance}) {
        if ($$rare_count_ref < $max_rare) {
            $$rare_count_ref++;
            quest::debug("Tier upgrade: T2 (Exalted) -> item $tier_variants{2}");
            return $tier_variants{2};
        }
    }
    # Roll for T1 (Greater)
    elsif (exists $tier_variants{1} && rand() < $tier_cfg->{t1_chance}) {
        quest::debug("Tier upgrade: T1 (Greater) -> item $tier_variants{1}");
        return $tier_variants{1};
    }

    return $item_id;
}

#=============================================================================
# get_loot_dbh() - Get database handle for tier lookups
#=============================================================================

sub get_loot_dbh {
    return plugin::LoadMysql() if defined &plugin::LoadMysql;
    return undef;
}

#=============================================================================
# velious_armor_tier_reward($base_id)
# Called from Velious armor quest turn-in scripts instead of quest::summonitem.
# Uses same chances as rare_tier_config(). Offsets are fixed:
#   T1 (Enhanced)  = base + 300000
#   T2 (Exalted)   = base + 500000
#   T3 (Ascendant) = base + 700000
#=============================================================================

sub velious_armor_tier_reward {
    my $base_id = shift;
    my $cfg = plugin::rare_tier_config();
    my $reward_id = $base_id;

    if ($cfg->{enable}) {
        my $roll = rand();
        if ($roll < $cfg->{t3_chance}) {
            $reward_id = $base_id + 700000;
            quest::debug("Velious armor tier: Ascendant (T3) -> $reward_id");
        }
        elsif ($roll < $cfg->{t3_chance} + $cfg->{t2_chance}) {
            $reward_id = $base_id + 500000;
            quest::debug("Velious armor tier: Exalted (T2) -> $reward_id");
        }
        elsif ($roll < $cfg->{t3_chance} + $cfg->{t2_chance} + $cfg->{t1_chance}) {
            $reward_id = $base_id + 300000;
            quest::debug("Velious armor tier: Enhanced (T1) -> $reward_id");
        }
    }

    quest::summonitem($reward_id);
}

return 1;
