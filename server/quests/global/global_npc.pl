# global_npc.pl - Global NPC Event Handlers
# Author: Straps
#
# Handles server-wide NPC events: spawn, combat, death, say, timers.
# Key systems wired here:
#   - Tier loot randomization (base → T1/T2/T3 at spawn via GetLootList)
#   - Mischief-style bonus loot for rares, common mobs, and raid bosses
#   - Illegible tome + ascendant gem drops on NPC death
#   - Dynamic encounter scaling for Velious+ zones (engage/disengage/rescan)
#   - Pet commands (stat display, inventory) via EVENT_SAY
#   - Halloween cosmetic overrides

use strict;
use warnings;

our (
  $npc,
  $client,
  $zoneid,
  $instanceid,
  $text,
  $status,
  $entity_list
);

my $ASCENDANT_SHARD_ID = 9600;  # Shard of Ascendant Power

# Ascendant Gems - Base IDs 17656-17661
my @AscendantGems = (17656, 17657, 17658, 17659, 17660, 17661);

# Illegible Tome Arrays - IDs 121571-121618
# Tier 1 (GREATER) - Icon 3370
my @IllegalibleTomesTier1 = (
  121571, 121574, 121577, 121580, 121583, 121586, 121589, 121592,
  121595, 121598, 121601, 121604, 121607, 121610, 121613, 121616
);

# Tier 2 (EXALTED) - Icon 3290
my @IllegalibleTomesTier2 = (
  121572, 121575, 121578, 121581, 121584, 121587, 121590, 121593,
  121596, 121599, 121602, 121605, 121608, 121611, 121614, 121617
);

# Tier 3 (ASCENDANT) - Icon 7384
my @IllegalibleTomesTier3 = (
  121573, 121576, 121579, 121582, 121585, 121588, 121591, 121594,
  121597, 121600, 121603, 121606, 121609, 121612, 121615, 121618
);

sub is_illegible_tome {
  my ($item_id) = @_;
  foreach my $t (
    @IllegalibleTomesTier1,
    @IllegalibleTomesTier2,
    @IllegalibleTomesTier3
  ) {
    return 1 if $item_id == $t;
  }
  return 0;
}

sub is_anti_farm_item {
  my ($item_id) = @_;
  return 1 if is_illegible_tome($item_id);
  return 1 if $item_id == $ASCENDANT_SHARD_ID;
  
  # Check if it's an Ascendant Gem
  foreach my $gem (@AscendantGems) {
    return 1 if $item_id == $gem;
  }
  
  return 0;
}

sub _get_dbh {
  return plugin::LoadMysql() if defined &plugin::LoadMysql;
  return undef;
}

sub is_named_or_rare {
    my ($npc) = @_;
    return 0 unless $npc;
    return 1 if $npc->IsRareSpawn();
    return 0;
}

sub _is_ldon_chest {
    my ($npc, $zid) = @_;
    return 0 unless $npc && $zid;
    my $ldon_zones = plugin::ldon_zone_ids();
    return 0 unless exists $ldon_zones->{$zid};
    return ($npc->GetCleanName() =~ /chest/i) ? 1 : 0;
}


# -----------------------------------------------------------------------------
# DZ Mode Detection — derives mode from expedition name, cached per instance
# Used by EVENT_DEATH_COMPLETE to prevent respawns in raid-mode DZs
# -----------------------------------------------------------------------------
my %_dz_mode_cache;

sub _get_dz_mode {
    return '' unless $instanceid && $instanceid > 0;
    return $_dz_mode_cache{$instanceid} if exists $_dz_mode_cache{$instanceid};

    my $dz = quest::get_expedition();
    return '' unless $dz;

    my $name = $dz->GetName();
    my $mode = ($name =~ /:\s*Raid$/i) ? 'raid' : 'normal';
    $_dz_mode_cache{$instanceid} = $mode;
    return $mode;
}

sub EVENT_SPAWN {

  # -----------------------------
  # NORMAL DZ: Depop raid targets (pure script, no DB migration needed)
  # In normal-mode expedition instances, raid targets are excluded.
  # Only applies in zones that offer a raid tier — otherwise let everything spawn.
  # -----------------------------
  if ($instanceid && $instanceid > 0 && $npc->IsRaidTarget()) {
      my $npc_id = $npc->GetNPCTypeID();
      # Exception: Narandi the Wretched (118145) must stay in normal instances for Ring War
      # Exception: Ageless Relic Protector split forms (293222/293223) - Kod'Taz Pit of the Lost step
      my %raid_depop_exceptions = (118145 => 1, 293222 => 1, 293223 => 1);
      if (!$raid_depop_exceptions{$npc_id}) {
          my $zone_short = quest::GetZoneShortName($zoneid);
          if (plugin::HasRaidTier($zone_short)) {
              my $dz_mode = _get_dz_mode($instanceid);
              if ($dz_mode eq 'normal') {
                  quest::debug("NORMAL DZ: Depop raid target " . $npc->GetCleanName());
                  $npc->Depop();
                  return;
              }
          }
      }
  }

  # -----------------------------
  # GLOBAL NPC COMBAT SCALING (SOLO/DUO FRIENDLY)
  # -----------------------------

  # Cache level once (used everywhere)
  my $lvl = $npc->GetLevel();

  # Skip combat scaling for pets, but allow other pet logic
  my $is_pet = $npc->IsPet();

  if (!$is_pet && !plugin::IsScalingZone($zoneid) && !plugin::IsLdonScalingZone($zoneid) && !plugin::IsLuclinScalingZone($zoneid) && !plugin::IsPopScalingZone($zoneid) && !plugin::IsLoyScalingZone($zoneid) && !(defined &plugin::IsGodScalingZone && plugin::IsGodScalingZone($zoneid))) {


    # ---------- DAMAGE ----------
    my $dmg_mult = 1.0;

    if ($lvl >= 15 && $lvl <= 30) {
        $dmg_mult = 0.95;   # -10%
    }
    elsif ($lvl >= 31 && $lvl <= 40) {
        $dmg_mult = 0.90;   # -78%
    }
    elsif ($lvl >= 41) {
        $dmg_mult = 0.90;   # -30%
    }

    if ($dmg_mult < 1.0) {
        my $min = $npc->GetMinDMG();
        my $max = $npc->GetMaxDMG();
        my $new_min = int($min * $dmg_mult);
        $new_min = 1 if $new_min < 1;

        my $new_max = int($max * $dmg_mult);
        $new_max = $new_min if $new_max < $new_min;

        $npc->ModifyNPCStat("min_hit", $new_min);
        $npc->ModifyNPCStat("max_hit", $new_max);

    }

    # ---------- HP ----------
    my $hp_mult = 1.0;

    if ($lvl >= 35 && $lvl <= 40) {
        $hp_mult = 0.95;   # -10%
    }
    elsif ($lvl >= 41) {
        $hp_mult = 0.95;   # -18%
    }

    if ($hp_mult < 1.0) {
        my $max_hp = $npc->GetMaxHP();
        my $new_hp = int($max_hp * $hp_mult);

        $npc->ModifyNPCStat("max_hp", $new_hp);
        $npc->SetHP($npc->GetMaxHP());
    }

    # ---------- ATK ----------
    if ($lvl >= 35) {
        my $atk = $npc->GetATK();
        $npc->ModifyNPCStat("atk", int($atk * 0.95));  # -10%
    }
  }


  # Check if this is a player pet and equip from pet bag
  if ($npc->IsPet() && $npc->GetOwnerID() > 0) {
      my $owner = $entity_list->GetClientByID($npc->GetOwnerID());

    # ---- PET RUNSPEED SET ----
    $npc->ModifyNPCStat("runspeed", 1.8);
    
      if ($owner && $owner->IsClient()) {
          # Equip pet from owner's pet bag
          plugin::EquipPetFromBag($npc, $owner);
      }
  }




  # -----------------------------
  # Halloween event handling
  # -----------------------------
  if (defined &quest::is_content_flag_enabled && quest::is_content_flag_enabled("peq_halloween")) {

    my $clean_name = $npc->GetCleanName();

    my $is_pet = 0;
    $is_pet = $npc->IsPet() if ($npc && $npc->can('IsPet'));

    unless ($clean_name =~ /mount/i || $is_pet) {

      if ($clean_name =~ /soulbinder/i || $clean_name =~ /priest of discord/i) {
        my @races = (14, 60, 82, 85);
        $npc->ChangeRace($races[int(rand(@races))]);
        $npc->ChangeSize(6);
        $npc->ChangeTexture(1);
        $npc->ChangeGender(2);
      }

      my %halloween_zones       = (202 => 1, 150 => 1, 151 => 1, 344 => 1);
      my %not_allowed_bodytypes = (11  => 1, 60  => 1, 66  => 1, 67  => 1);

      if (exists $halloween_zones{$zoneid} && !exists $not_allowed_bodytypes{$npc->GetBodyType()}) {
        my @races = (14, 60, 82, 85);
        $npc->ChangeRace($races[int(rand(@races))]);
        $npc->ChangeSize(6);
        $npc->ChangeTexture(1);
        $npc->ChangeGender(2);
      }
    }
  }

  # -----------------------------
  # April Fools Global Drops (1 in 10 kills)
  # -----------------------------
  if (plugin::AprilFools_Enabled()) {
      my @AprilFoolsItems = (29781, 42983, 55938, 64044, 64046);
      plugin::AddLoot(1, 10, @AprilFoolsItems);
  }

  # -----------------------------
  # LDON Relic Global Drop (community unlock event)
  # -----------------------------
  if (defined &quest::is_content_flag_enabled && !quest::is_content_flag_enabled("ldon")) {
      plugin::AddLoot(1, 800, 9544);  # ~0.02% — Lost Dungeon Relic
  }

  # -----------------------------
  # Ascendant Gems Global Drops
  # -----------------------------
  plugin::AddLoot(1, 450, @AscendantGems);  # ~0.33% per gem, ~2% for any gem

  # -----------------------------
  # ASCENDANT SHARD - LEVEL SCALED
  # -----------------------------

  if ($lvl <= 20) {
      plugin::AddLoot(1, 100, $ASCENDANT_SHARD_ID);   # ~0.66%
  }
  elsif ($lvl <= 30) {
      plugin::AddLoot(1, 90, $ASCENDANT_SHARD_ID);   # ~0.83%
  }
  elsif ($lvl <= 40) {
      plugin::AddLoot(1, 80,  $ASCENDANT_SHARD_ID);   # ~1.25%
  }
  else { # 41+
      plugin::AddLoot(1, 70,  $ASCENDANT_SHARD_ID);   # ~2%
  }



  # -----------------------------
  # Illegible Tome Global Drops (Level-Based)
  # -----------------------------
  my $mlevel = $npc->GetLevel();

  if ($mlevel >= 1 && $mlevel <= 10) {
    # slight tilt to T1
    plugin::AddLoot(1, 165, @IllegalibleTomesTier1);
    plugin::AddLoot(1, 175, @IllegalibleTomesTier2);
    plugin::AddLoot(1, 185, @IllegalibleTomesTier3);

  } elsif ($mlevel >= 11 && $mlevel <= 20) {
    plugin::AddLoot(1, 155, @IllegalibleTomesTier1);
    plugin::AddLoot(1, 165, @IllegalibleTomesTier2);
    plugin::AddLoot(1, 175, @IllegalibleTomesTier3);

  } elsif ($mlevel >= 21 && $mlevel <= 30) {
    # nearly even
    plugin::AddLoot(1, 150, @IllegalibleTomesTier1);
    plugin::AddLoot(1, 155, @IllegalibleTomesTier2);
    plugin::AddLoot(1, 160, @IllegalibleTomesTier3);

  } elsif ($mlevel >= 31 && $mlevel <= 40) {
    # slight tilt to T3
    plugin::AddLoot(1, 150, @IllegalibleTomesTier1);
    plugin::AddLoot(1, 145, @IllegalibleTomesTier2);
    plugin::AddLoot(1, 140, @IllegalibleTomesTier3);

  } else { # 41+
    # slight tilt to T3
    plugin::AddLoot(1, 145, @IllegalibleTomesTier1);
    plugin::AddLoot(1, 140, @IllegalibleTomesTier2);
    plugin::AddLoot(1, 135, @IllegalibleTomesTier3);
  }

  # -----------------------------
  # NAMED / RARE MOB GUARANTEED PROGRESSION
  # -----------------------------
  my $npc_level = $npc->GetLevel();

  # Velious/PoP/LoY raid targets: level-banded raid loot (2-3 items), skips rare pool
  my $is_velious_raid = 0;
  # Ascendant: event phases/adds that respawn on reset must not roll raid bonus loot (farmable).
  # Tacvi: fake Tunat`Muram Cuu Vauax (298014), Zun`Muram Mordl Delt split adds (298050-298053)
  my %no_raid_bonus_loot = map { $_ => 1 } (298014, 298050, 298051, 298052, 298053);
  if ($npc->IsRaidTarget() && !$no_raid_bonus_loot{$npc->GetNPCTypeID()}) {
      my $zone_exp = plugin::zone_to_expansion($zoneid);
      if ($zone_exp && ($zone_exp eq 'velious' || $zone_exp eq 'pop' || $zone_exp eq 'loy' || $zone_exp eq 'god')) {
          $is_velious_raid = 1;
          plugin::raid_levelblock_loot($npc, $npc_level, $zoneid);
      }
  }

  # For rare spawns - guaranteed extra rare_spawn loottable roll, with 10% chance for a 2nd roll
  if (!$is_velious_raid && $npc->IsRareSpawn()) {

    plugin::rare_levelblock_loot($npc, $npc_level, $zoneid);
    plugin::kunark_spell_bonus_loot($npc, $npc_level, $zoneid);

    # Combine all tome tiers into one pool
    my @AllIllegalibleTomes = (
        @IllegalibleTomesTier1,
        @IllegalibleTomesTier2,
        @IllegalibleTomesTier3
    );

    # Shard drop chance
    if ($zoneid == 39) {
        # The Hole: 1 in 12
        plugin::AddLoot(1, 12, $ASCENDANT_SHARD_ID);
        plugin::AddLoot(1, 6, @AllIllegalibleTomes);
    } else {
        # Normal: 1 in 6
        plugin::AddLoot(1, 6, $ASCENDANT_SHARD_ID);
        plugin::AddLoot(1, 3, @AllIllegalibleTomes);
    }
}
  # LDON chests — boosted tomes/shards (same rates as named mobs)
  elsif (!$is_velious_raid && _is_ldon_chest($npc, $zoneid)) {

    my @AllIllegalibleTomes = (
        @IllegalibleTomesTier1,
        @IllegalibleTomesTier2,
        @IllegalibleTomesTier3
    );

    # Same rates as named: shard 1/6, tomes 1/3
    plugin::AddLoot(1, 6, $ASCENDANT_SHARD_ID);
    plugin::AddLoot(1, 3, @AllIllegalibleTomes);

    # Also give mischief-style bonus loot like nameds
    plugin::rare_levelblock_loot($npc, $npc_level, $zoneid);
  }
  # For all other mobs - 0.33% chance for a rare_spawn loottable bonus roll
  elsif (!$is_velious_raid) {
      plugin::common_mob_bonus_loot($npc, $npc_level, $zoneid);
  }


  # -----------------------------
  # Tier loot randomization - Process at spawn
  # -----------------------------

  return unless $npc;

  my $dbh = _get_dbh();
  return unless $dbh;

  # Get the NPC's loot list
  my @loot_list = $npc->GetLootList();
  return unless @loot_list;

  #quest::debug("TierLoot: NPC " . $npc->GetCleanName() . " has " . scalar(@loot_list) . " items in loot list");

  my $rare_item_upgraded = 0;  # Track if we've upgraded a rare item (limit 1 per NPC)

  # Iterate through each item in loot
  foreach my $item_id (@loot_list) {
    next unless $item_id && $item_id > 0;

    # Query for tier variants of this item
    my $sql = q{
      SELECT
        tier_code,
        variant_item_id
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

    next unless %tier_variants;

    # Roll for tier upgrade
    my $selected_tier = 0;
    my $selected_item_id = $item_id;

    # Roll for T3 first (Ascendant) - 3%
    if (exists $tier_variants{3} && rand() < 0.06) {
      # Limit rare items (T2 and T3) to 1 per NPC
      if (!$rare_item_upgraded) {
        $selected_tier = 3;
        $selected_item_id = $tier_variants{3};
        $rare_item_upgraded = 1;
      }
    }
    # Roll for T2 (Exalted) - adjusted to land ~10%
    elsif (exists $tier_variants{2} && rand() < 0.15) {

      if (!$rare_item_upgraded) {
        $selected_tier = 2;
        $selected_item_id = $tier_variants{2};
        $rare_item_upgraded = 1;
      }
    }
    # Roll for T1 (Greater) - adjusted to land ~30%
    elsif (exists $tier_variants{1} && rand() < 0.30) {

      # T1 is not limited, can have multiple
      $selected_tier = 1;
      $selected_item_id = $tier_variants{1};
    }

    # Replace item if tier was selected
    if ($selected_tier > 0 && $selected_item_id != $item_id) {
      # Remove the base item
      $npc->RemoveItem($item_id);
      
      # Add the tier item
      $npc->AddItem($selected_item_id, 1);
      
      #quest::debug("TierLoot: NPC " . $npc->GetCleanName() . " replaced base=$item_id with T$selected_tier item=$selected_item_id");
    }
  }
  plugin::raid_boss_bonus_loot($npc, $zoneid);
}


# Pet Commands — enables pet stat and inventory display via EVENT_SAY
# Author: Straps
sub EVENT_SAY {
    # Your existing EVENT_SAY code...
    
    # Pet commands - only respond if this is a pet
    if ($npc->IsPet() && $npc->GetOwnerID() > 0) {
        my $owner_id = $npc->GetOwnerID();
        my $owner = $entity_list->GetClientByID($owner_id);
        
        # Only respond to owner
        if ($owner && $client->CharacterID() == $owner->CharacterID()) {
            if ($text =~ /hail/i) {
                # Whisper (color 6) to owner
                $client->Message(18, $npc->GetCleanName() . " 'Greetings, Master! I am ready to serve.'");
                $client->Message(18, $npc->GetCleanName() . " 'Say " . quest::saylink("my stats", 1, "my stats") . " to see my combat statistics.'");
                $client->Message(18, $npc->GetCleanName() . " 'Say " . quest::saylink("equip", 1, "equip") . " to equip items from your Pet Bag.'");
            }
            elsif ($text =~ /my stats/i) {
                plugin::ShowPetStats($npc, $client);
            }
            elsif ($text =~ /equip/i) {
                # Check cooldown to prevent spam equipping
                my $char_id = $client->CharacterID();
                my $cooldown_key = "pet_equip_cooldown_" . $char_id;
                my $last_equip = quest::get_data($cooldown_key);
                
                if ($last_equip && (time() - $last_equip) < 30) {
                    my $remaining = 30 - (time() - $last_equip);
                    $client->Message(13, $npc->GetCleanName() . " 'Please wait " . $remaining . " seconds before I can equip again, Master.'");
                    return;
                }
                
                my $equipped_count = plugin::EquipPetFromBag($npc, $owner);
                if (defined $equipped_count && $equipped_count > 0) {
                    quest::set_data($cooldown_key, time());
                    $client->Message(18, $npc->GetCleanName() . " 'I have equipped " . $equipped_count . " item(s) from your Pet Bag, Master!'");

                } elsif (!defined $equipped_count) {
                    $client->Message(13, $npc->GetCleanName() . " 'I could not find a Pet Bag in your inventory or bank, Master.'");
                } else {
                    $client->Message(18, $npc->GetCleanName() . " 'I found no items to equip in your Pet Bag, Master.'");
                }
            }
        }
    }
    
    # Rest of your EVENT_SAY code...
}


sub EVENT_COMBAT {
    my $combat_state = plugin::val('$combat_state');
    if ($combat_state == 1) {
        plugin::EncounterScaling_OnEngage($npc);
        plugin::LdonScaling_OnEngage($npc);
        plugin::LuclinScaling_OnEngage($npc);
        plugin::PopScaling_OnEngage($npc);
        plugin::LoyScaling_OnEngage($npc);
        plugin::GodScaling_OnEngage($npc) if defined &plugin::GodScaling_OnEngage;
        plugin::AprilFools_OnEngage($npc);

        # Fellowship mob strength scaling (after encounter scaling)
        if (!$npc->IsPet() && $npc->GetLevel() > 1) {
            my $target = $npc->GetTarget();
            if ($target && $target->IsClient()) {
                my $tier = plugin::Fellowship_GetCurrentTier($target->CastToClient());
                plugin::Fellowship_ScaleMob($npc, $tier) if $tier > 0;
            }
        }
    } else {
        plugin::EncounterScaling_OnDisengage($npc);
        plugin::LdonScaling_OnDisengage($npc);
        plugin::LuclinScaling_OnDisengage($npc);
        plugin::PopScaling_OnDisengage($npc);
        plugin::LoyScaling_OnDisengage($npc);
        plugin::GodScaling_OnDisengage($npc) if defined &plugin::GodScaling_OnDisengage;
        plugin::Fellowship_RestoreMob($npc);
    }
}


sub EVENT_SPAWN_ZONE {
  our $spawned;
  return unless $spawned && $spawned->IsPet();
  return unless $spawned->GetOwner();
  return unless $spawned->GetOwner()->IsClient();
  quest::settimer("pet_aura_apply_" . $spawned->GetID(), 3);
}

sub EVENT_TIMER {
  our $timer;
  if ($timer eq 'enc_rescan') {
    plugin::EncounterScaling_Rescan($npc);
    plugin::LdonScaling_Rescan($npc);
    plugin::LuclinScaling_Rescan($npc);
    plugin::PopScaling_Rescan($npc);
    plugin::LoyScaling_Rescan($npc);
    plugin::GodScaling_Rescan($npc) if defined &plugin::GodScaling_Rescan;
    return;
  }

  if ($timer =~ /^pet_aura_apply_(\d+)$/) {
    my $pet_id = $1;
    quest::stoptimer($timer);
    my $pet = $entity_list->GetMobByID($pet_id);
    return unless $pet && $pet->IsPet();
    return unless $pet->GetOwner() && $pet->GetOwner()->IsClient();
    my %auras = (
      ascendant_aura_speed_expires   => 25543,
      ascendant_aura_healing_expires => 25544,
      ascendant_aura_thought_expires => 25545,
      ascendant_aura_haste_expires   => 25546,
    );
    my $now = time();
    foreach my $bucket (keys %auras) {
      my $spell  = $auras{$bucket};
      my $expire = quest::get_data($bucket);
      next unless $expire;
      my $remaining = $expire - $now;
      next if $remaining <= 0;
      my $ticks = int($remaining / 6);
      $pet->BuffFadeBySpellID($spell);
      $pet->ApplySpellBuff($spell, $ticks, 255);
    }
  }
}


sub EVENT_CHARM_START {
  # Store original loot list before equipping pet bag items
  # This preserves the NPC's natural loot drops
  my @original_loot = $npc->GetLootList();
  if (@original_loot && scalar(@original_loot) > 0) {
    my $loot_string = join(',', @original_loot);
    $npc->SetEntityVariable('original_loot', $loot_string);
    quest::debug("PetBag: Stored original loot for charmed NPC: $loot_string");
  }

  # Charm ownership is established before EVENT_CHARM_START fires, so this equips
  # the owner's pet bag onto charmed pets the same way summoned pets are equipped.
  my $owner = undef;
  my $owner_id = $npc->GetOwnerID();
  $owner = $entity_list->GetClientByID($owner_id) if $owner_id;
  if ($owner && $owner->IsClient()) {
    plugin::EquipPetFromBag($npc, $owner);
  }

  # Apply active Ascendant Auras to the newly charmed mob
  my %auras = (
    ascendant_aura_speed_expires   => 25543,
    ascendant_aura_healing_expires => 25544,
    ascendant_aura_thought_expires => 25545,
    ascendant_aura_haste_expires   => 25546,
  );
  my $now = time();
  foreach my $bucket (keys %auras) {
    my $spell  = $auras{$bucket};
    my $expire = quest::get_data($bucket);
    next unless $expire;
    my $remaining = $expire - $now;
    next if $remaining <= 0;
    my $ticks = int($remaining / 6);
    $npc->BuffFadeBySpellID($spell);
    $npc->ApplySpellBuff($spell, $ticks, 255);
  }
}


sub EVENT_CHARM_END {
  # Wipe Ascendant Auras from the mob when charm breaks
  my @aura_spells = (25543, 25544, 25545, 25546);
  foreach my $spell (@aura_spells) {
    $npc->BuffFadeBySpellID($spell);
  }

  my $original_loot_string = $npc->GetEntityVariable('original_loot');
  my @original_loot = ();
  if ($original_loot_string) {
    @original_loot = split(',', $original_loot_string);
    quest::debug("PetBag: Retrieved original loot: $original_loot_string");
  }
  
  # Remove pet-bag equipped items before restoring the NPC's natural loot.
  $npc->ClearEquippedItems();

  # Aggressively clear ALL loot - loop until nothing remains
  my $safety = 0;
  while ($safety < 100) {
    my @current_loot = $npc->GetLootList();
    last unless @current_loot;
    foreach my $item_id (@current_loot) {
      $npc->RemoveItem($item_id) if $item_id && $item_id > 0;
    }
    $safety++;
  }
  
  # Restore original loot
  foreach my $item_id (@original_loot) {
    if ($item_id && $item_id > 0) {
      $npc->AddItem($item_id);
      quest::debug("PetBag: Restored original item $item_id to NPC");
    }
  }
  
  $npc->DeleteEntityVariable('original_loot');
}

sub EVENT_DEATH {
 
  plugin::AprilFools_OnDeath($npc);

  quest::debug("ANTI-FARM: EVENT_DEATH fired");
 
  my $killer_id = plugin::val('$killer_id');
  my $killer_mob = $entity_list->GetMobByID($killer_id);
  return unless $killer_mob;
 
  # Resolve pet kills
  if ($killer_mob->IsPet()) {
    my $owner = $entity_list->GetMobByID($killer_mob->GetOwnerID());
    $killer_mob = $owner if $owner;
  }
 
  return unless $killer_mob && $killer_mob->IsClient();
  my $client = $killer_mob->CastToClient();

  # -----------------------------
  # FELLOWSHIP BONUS LOOT (tier-based)
  # -----------------------------
  my $fellowship_tier = plugin::Fellowship_GetCurrentTier($client);
  if ($fellowship_tier > 0) {
    plugin::Fellowship_BonusLoot($npc, $client, $fellowship_tier);
  }

  # -----------------------------
  # SHARD RATE LIMIT (1 per 7 min)
  # -----------------------------
  my $char_id = $client->CharacterID();
  my $cd_key  = "shard_cd_${char_id}";
  my @loot_check = $npc->GetLootList();

  foreach my $item_id (@loot_check) {
    next unless $item_id == $ASCENDANT_SHARD_ID;

    if (quest::get_data($cd_key)) {
      $npc->RemoveItem($ASCENDANT_SHARD_ID);
      quest::debug("SHARD RATE LIMIT: Removed shard for char=$char_id (cooldown active)");
    } else {
      quest::set_data($cd_key, "1", 420);
      quest::debug("SHARD RATE LIMIT: Shard allowed for char=$char_id (7m cooldown set)");
    }
    last;
  }

  my $npc_level    = $npc->GetLevel();
  my $client_level = $client->GetLevel();
  my $delta        = $client_level - $npc_level;
 
  # Calculate max level difference that still gives exp (EQ green con range)
  my $exp_range;
  if    ($client_level <= 7)  { $exp_range = 3;  }
  elsif ($client_level <= 24) { $exp_range = 5;  }
  elsif ($client_level <= 34) { $exp_range = 7;  }
  elsif ($client_level <= 44) { $exp_range = 10; }
  elsif ($client_level <= 50) { $exp_range = 13; }
  else                        { $exp_range = 16; }
 
  quest::debug("ANTI-FARM: client=$client_level npc=$npc_level delta=$delta exp_range=$exp_range");
 
  # If mob gives exp, no penalty
  return if $delta <= $exp_range;
 
  my @loot = $npc->GetLootList();
  quest::debug("ANTI-FARM: NPC has " . scalar(@loot) . " loot entries");
 
  # How far past the exp range are we?
  my $over = $delta - $exp_range;
 
  foreach my $item_id (@loot) {
    next unless is_anti_farm_item($item_id);
 
    if ($over > 5) {
      # Way past exp range - guaranteed removal
      quest::debug("ANTI-FARM: HARD REMOVE item=$item_id (over=$over)");
      $npc->RemoveItem($item_id);
    }
    else {
      # Just past exp range - scaling chance (20% per level over)
      my $remove_chance = $over * 0.20;
      if (rand() < $remove_chance) {
        quest::debug("ANTI-FARM: SOFT REMOVE item=$item_id (over=$over chance=$remove_chance)");
        $npc->RemoveItem($item_id);
      }
    }
  }
}

# -----------------------------------------------------------------------------
# EVENT_KILLED_MERIT — Gates of Discord kill tracking + SERVER FIRSTs
# Fires once per raid/group member with kill credit. For every GoD raid encounter:
#   * per-account bucket  god_<key>_<account_id> = char name   (Aurelian reads the 6
#     finishing keys: cragbeastqueen, zunmuramvotal, vrexbarxt, keeperaltar, iqthinxa, noqufiel)
#   * server first        first_kill_<key> = "Name|Guild"  (same key convention as the
#     Classic-PoP boss scripts; website-readable) + world emote.
# Lives here (not in PEQ boss scripts) so it survives script refreshes. Multi-NPC
# encounters (twins, alt ids) credit whichever listed NPC dies first.
# -----------------------------------------------------------------------------
my %GOD_KILL_TRACKING = (
    280080 => ['cragbeastqueen', "the Cragbeast Queen"],  # Natimbi
    281089 => ['gorlakt', "Mastruq Commander Gorlakt"],  # Qinimi
    281090 => ['spiritlords', "the Spiritlords"],  # Qinimi
    281091 => ['spiritlords', "the Spiritlords"],  # Qinimi
    281126 => ['xictickrvne', "Pixtt Xictic Krvne"],  # Qinimi
    281030 => ['massofstone', "the Mass of Stone"],  # Qinimi
    281125 => ['kabekakret', "Kabeka Kret"],  # Qinimi
    282105 => ['krisz', "Taskmistress Krisz"],  # Riwwi
    283049 => ['colossusofwar', "the Colossus of War"],  # Barindu
    283050 => ['ikaavixvetpox', "Ikaav Ixvet Pox"],  # Barindu
    283162 => ['ikaavixvetpox', "Ikaav Ixvet Pox"],  # Barindu
    284067 => ['skoiatpizak', "Packmaster Skoiat Pizak"],  # Ferubi
    284080 => ['zunmuramvotal', "Zun`Muram Votal"],  # Ferubi
    284097 => ['smithrondo', "Smith Rondo"],  # Ferubi
    291102 => ['primalreptile', "the Primal Reptile"],  # Yxtta
    291115 => ['primalarachnid', "the Primal Arachnid"],  # Yxtta
    291117 => ['primalinsect', "the Primal Insect"],  # Yxtta
    291111 => ['mindrider', "Pixtt Suir Mindrider"],  # Yxtta
    293044 => ['pixttnemis', "Pixtt Nemis"],  # Kod'Taz
    293214 => ['chaosprovoker', "the Hexxt Chaos Provoker"],  # Kod'Taz
    292021 => ['tqivtwins', "the Tqiv twins"],  # Uqua
    292022 => ['tqivtwins', "the Tqiv twins"],  # Uqua
    292065 => ['devastatingconstruct', "the Devastating Construct"],  # Uqua
    292069 => ['vrexbarxt', "Vrex Barxt Qurat"],  # Uqua
    294577 => ['tuniktamuik', "the Spirit of Tunik Tamuik"],  # Ikkinz
    294471 => ['guardianglorification', "the Guardian of Glorification"],  # Ikkinz
    294582 => ['guardianglorification', "the Guardian of Glorification"],  # Ikkinz
    294595 => ['xalkaknixki', "Vrex Xalkak Nixki"],  # Ikkinz
    294620 => ['keeperaltar', "the Keeper of the Altar"],  # Ikkinz
    295140 => ['cynosure', "Cynosure Kvanjji"],  # Qvic
    295149 => ['cynosure', "Cynosure Kvanjji"],  # Qvic
    295001 => ['mnatmsha', "Mnat M`sha"],  # Qvic
    295087 => ['dvoinmsha', "Dvoin M`sha"],  # Qvic
    295134 => ['ptavmsha', "Ptav M`sha"],  # Qvic
    295130 => ['iqthinxa', "Iqthinxa Karnkvi"],  # Qvic
    295150 => ['iqthinxa', "Iqthinxa Karnkvi"],  # Qvic
    296013 => ['nijacka', "Nijacka the Scourge"],  # Inktu`Ta
    296024 => ['kelekdrix', "Kelekdrix, Herald of Trushar"],  # Inktu`Ta
    296027 => ['mimezpo', "Mimezpo the Oracle"],  # Inktu`Ta
    296065 => ['noqufiel', "Noqufiel"],  # Inktu`Ta
    297056 => ['cragbeastmatriarch', "the Ancient Cragbeast Matriarch"],  # Txevu
    297023 => ['aneuktwins', "the Aneuk twins"],  # Txevu
    297025 => ['aneuktwins', "the Aneuk twins"],  # Txevu
    297218 => ['nkosibakari', "High Priest Nkosi Bakari"],  # Txevu
    297090 => ['ikaavnysflleiv', "Ikaav Nysf Lleiv"],  # Txevu
    297032 => ['ixttwins', "Ixt Kial Ivesi and Ixt Quall Nalnn"],  # Txevu
    297043 => ['ixttwins', "Ixt Kial Ivesi and Ixt Quall Nalnn"],  # Txevu
    297044 => ['ixttwins', "Ixt Kial Ivesi and Ixt Quall Nalnn"],  # Txevu
    297049 => ['ixttwins', "Ixt Kial Ivesi and Ixt Quall Nalnn"],  # Txevu
    297074 => ['ixttwins', "Ixt Kial Ivesi and Ixt Quall Nalnn"],  # Txevu
    297091 => ['ixttwins', "Ixt Kial Ivesi and Ixt Quall Nalnn"],  # Txevu
    297211 => ['ixthseksyat', "Ixt Hsek Syat"],  # Txevu
    297082 => ['ukunbloodfeaster', "Ukun Bloodfeaster"],  # Txevu
    297150 => ['tkarishzyk', "Zun`Muram Tkarish Zyk"],  # Txevu
    298039 => ['xxerickex', "Pixtt Xxeric Kex"],  # Tacvi
    298201 => ['kretvkrakxt', "Pixtt Kretv Krakxt"],  # Tacvi
    298032 => ['rieltavas', "Pixtt Riel Tavas"],  # Tacvi
    298018 => ['shaldnboc', "Zun`Muram Shaldn Boc"],  # Tacvi
    298020 => ['mordldelt', "Zun`Muram Mordl Delt"],  # Tacvi
    298023 => ['yihstvor', "Zun`Muram Yihst Vor"],  # Tacvi
    298029 => ['kvxepirik', "Zun`Muram Kvxe Pirik"],  # Tacvi
    298055 => ['tunatmuram', "Tunat`Muram Cuu Vauax"],  # Tacvi
);

sub EVENT_KILLED_MERIT {
    return unless $npc && $client;
    my $boss = $GOD_KILL_TRACKING{$npc->GetNPCTypeID()};
    return unless $boss;
    my ($key, $boss_name) = @$boss;
    my $char_name = $client->GetCleanName();
    quest::set_data('god_' . $key . '_' . $client->AccountID(), $char_name);

    my $first_key = 'first_kill_' . $key;
    return if quest::get_data($first_key) || $client->GetGM();
    my $guild = $client->GuildID() > 0 ? quest::getguildnamebyid($client->GuildID()) : '';
    $guild = '' unless defined $guild;
    quest::set_data($first_key, $char_name . '|' . $guild);
    my $who = $guild ne '' ? "$char_name <$guild>" : $char_name;
    quest::we(15, "SERVER FIRST! $who and their group have slain $boss_name for the first time on this server!");
}

# -----------------------------------------------------------------------------
# EVENT_DEATH_COMPLETE — Raid DZ no-respawn
# Disables spawn points after NPC death in raid-mode expedition instances.
# Only fires in instanced zones with an active expedition named "...: Raid"
# -----------------------------------------------------------------------------
sub EVENT_DEATH_COMPLETE {
    return unless $instanceid && $instanceid > 0;
    my $dz_mode = _get_dz_mode($instanceid);
    if ($dz_mode eq 'raid') {
        my $sp_id = $npc->GetSpawnPointID();
        if ($sp_id) {
            quest::disable_spawn2($sp_id);
            quest::debug("RAID DZ: Disabled spawn2 $sp_id for " . $npc->GetCleanName());
        }
    }
}

1;
