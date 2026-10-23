# items: 60173
sub EVENT_ITEM {
  if (plugin::check_handin(\%itemcount, 60173 =>1)) {
    quest::emote("motions for you to enter the altar through the entrance behind him.");
    if(defined($qglobals{ikky}) && ($qglobals{ikky} == 14)) {
      # Ascendant: solo-friendly. Raid cap applies only to a raid; raid, group or solo can enter.
      $raid = $client->GetRaid();
      if (!$raid || $raid->RaidCount() <= 54) {
        if(!defined($qglobals{ikkylockout6})) {
          $InInstanceIkky7 = quest::GetInstanceID("ikkinz",6);
          if($InInstanceIkky7 == 0){
            $Instance = PARTY_IKKY_INSTANCE(6);
            if ($Instance) {
              quest::AssignToInstance($Instance);
            } else {
              $Instance = quest::CreateInstance("ikkinz", 6, 21600);
              ASSIGN_IKKY_INSTANCE($Instance);
            }
            quest::say("Instance added.");
            $client->MarkCompassLoc(1860, 660, -447);
          } else {
            $client->Message(13, "You are already in an instance!");
          }
        } else {
          $client->Message(13,"You have recently completed a raid.");
        }
      } else {
        $client->Message(13, "You have ".$raid->RaidCount()." players in raid.  Only 54 allowed");
      }
    }
    quest::summonitem(60173); # Item: Icon of the Altar
  }
  plugin::return_items(\%itemcount);
}

# Ascendant: assign the whole raid, the whole group, or just the client
sub ASSIGN_IKKY_INSTANCE {
  my $instance_id = shift;
  my $raid = $client->GetRaid();
  my $group = $client->GetGroup();
  if ($raid) {
    quest::AssignRaidToInstance($instance_id);
  } elsif ($group) {
    quest::AssignGroupToInstance($instance_id);
  } else {
    quest::AssignToInstance($instance_id);
  }
}

# Ascendant: an existing ikkinz instance of this version held by a raid or group member, or 0
sub PARTY_IKKY_INSTANCE {
  my $version = shift;
  my $raid = $client->GetRaid();
  my $group = $client->GetGroup();
  my @members = ();
  if ($raid) {
    for (my $i = 0; $i < 72; $i++) {
      my $member = $raid->GetMember($i);
      push(@members, $member) if ($member && $member->IsClient());
    }
  } elsif ($group) {
    for (my $i = 0; $i < 6; $i++) {
      my $member = $group->GetMember($i);
      push(@members, $member) if ($member && $member->IsClient());
    }
  }
  foreach my $member (@members) {
    next if ($member->CharacterID() == $client->CharacterID());
    my $instance_id = quest::GetInstanceIDByCharID("ikkinz", $version, $member->CharacterID());
    return $instance_id if ($instance_id);
  }
  return 0;
}
