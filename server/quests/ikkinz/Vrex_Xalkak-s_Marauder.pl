my $archkill;

sub EVENT_SPAWN {
    quest::settimer(2,75);
  if(!defined($qglobals{$instanceid.marakill})) {
    $archkill = int(rand(4)) + 1;
    quest::setglobal($instanceid.marakill,$archkill,3,"H6");
  }
  else {
    $archkill = $qglobals{$instanceid.marakill};
  }
}

sub EVENT_AGGRO {
  quest::settimer(1,1);
}

sub EVENT_TIMER {
  if(($timer == 1) && ($x > 720 || $x < 500 || $y < -300 || $y > -120)) {
    $npc->GMMove(531,-157,-49,252);
  }
  if($timer == 2) {
    return; # Ascendant: class rotation no longer matters (any class finishes it)
    quest::ze(15,"The creature shudders as if somehow changing.");
    if(defined($qglobals{$instanceid.marakill}) && $qglobals{$instanceid.marakill} == 1) {
      quest::setglobal($instanceid.marakill,2,3,"H6");
	  $archkill = $qglobals{$instanceid.marakill};
    }
    if(defined($qglobals{$instanceid.marakill}) && $qglobals{$instanceid.marakill} == 2) {
      quest::setglobal($instanceid.marakill,3,3,"H6");
	  $archkill = $qglobals{$instanceid.marakill};
    }
    if(defined($qglobals{$instanceid.marakill}) && $qglobals{$instanceid.marakill} == 3) {
      quest::setglobal($instanceid.marakill,4,3,"H6");
	  $archkill = $qglobals{$instanceid.marakill};
    }
    if(defined($qglobals{$instanceid.marakill}) && $qglobals{$instanceid.marakill} == 4) {
      quest::setglobal($instanceid.marakill,1,3,"H6");
	  $archkill = $qglobals{$instanceid.marakill};
    }
  }
}

sub EVENT_DEATH_COMPLETE {
  # Ascendant: any class can finish the marauder. It used to need a killing blow from a
  # rotating class group (and the rotation timer never actually changed the group),
  # so it respawned forever unless the right class was present.
  quest::spawn2(294595,0,0,480,-592,-50,128); # NPC: #Vrex_Xalkak_Nixki
  quest::delglobal($instanceid.marakill);
  quest::ze(15,"The marauder's remains crash to the ground. It is no more.");
}
