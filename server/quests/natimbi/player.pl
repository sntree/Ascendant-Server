sub EVENT_CLICKDOOR {
  if($doorid == 1) {
    # Ascendant (TLP-style access): completing either Vxed or Tipt opens Kod'Taz; sewers not required.
    # Also honors older temp ("T") flags from finishing either zone in someone else's expedition.
    my $cid = $client->CharacterID();
    if(defined($qglobals{god_kodtaz_access}) || defined($qglobals{god_tipt_access})
       || quest::get_data("$cid-god_vxed") ne "" || quest::get_data("$cid-god_tipt") ne "") {
      if(quest::has_zone_flag(293) != 1) {
  	    quest::set_zone_flag(293);
	  }
    }
  }
}

sub EVENT_ENTERZONE {
  # Ascendant: in a Planeshifter Tyrael instance, make the Cragbeast Queen available
  # (public natimbi keeps her 1am-3am game-time spawn condition "CBQ" / 5-day respawn).
  if ($instanceid > 0) {
    quest::spawn_condition("natimbi", $instanceid, 1, 1);
  }
#quest::delglobal("druid_epic_natimbi");
  if (defined($qglobals{druid_epic}) && $qglobals{druid_epic}==6 && !defined $qglobals{druid_epic_natimbi}) {
	quest::setglobal("druid_epic_natimbi",1,3,"H2");
	quest::unique_spawn(280090,0,0,1451.65,957.84,517.86,432); #ritual conduit
  } 
}

sub EVENT_LOOT {
  if ($class eq "Druid" && $looted_id == 62889) {  #Energized Noc Blood
	if (defined($qglobals{druid_epic}) && ($qglobals{druid_epic} > 5)) {
	  if (!defined($qglobals{druid_chest_natimbi})) {
			quest::setglobal("druid_chest_natimbi", "1", 5, "F"); 
			$x = $client->GetX();
			$y = $client->GetY();
			$z = $client->GetZ();
			quest::spawn2(283157,0,0,$x,$y,$z,0); # NPC: a_chest
		}
	  return 0;
	}
	else 
	{
		return 1;
	}
  }
}
