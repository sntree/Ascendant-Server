my $namedcount = 0; # Sets the named counter for later use
my $coirnav_wave = 0; # Instance-local wave tracker (was a server-wide qglobal; not instance-safe)
my $recovery_active = 0;
my $hard_cap_started = 0;

my $RECOVERY_SECONDS = 60 * 60;  # 1 hour
my $HARD_CAP_SECONDS = 2 * 60 * 60; # 2 hours

# NPC IDs that count as "event engaged" for recovery-timer purposes
my @event_mobs = (
  216048, # fake Coirnav
  216094, # real Coirnav
  216070, # #Pwelon_of_Vapor (tough)
  216061, # #Nrinda_of_Ice (tough)
  216065, # #Vamuil_of_Water (tough)
  216109, # Pwelon_of_Vapor (weak)
  216108, # Nrinda_of_Ice (weak)
  216110, # Vamuil_of_Water (weak)
  216071, # Triloun Vaporfiend
  216076, # Hraquis Icefiend
  216060, # Regrua Waterfiend
);

# Lockout qglobal must be instance-scoped so concurrent powater instances don't collide.
# (quest_globals has no instanceid column, so we encode the instance id into the key name.)
sub _coirnav_done_key {
  return $instanceid ? "${instanceid}_coirnav_done" : "coirnav_done";
}

sub EVENT_SPAWN { #Only needed for #repop during testing
  quest::stopalltimers();
  # Fresh DZ boot: clear any stale Coirnav lockout left by a PREVIOUS expedition
  # that reused this instance id. quest_globals have no instanceid column, so the
  # lockout is encoded in the key name and would otherwise be inherited by a
  # recycled instance id -- which made #Guardian_of_Coirnav depop on spawn
  # ("Guardian despawned on engage"). The real cross-session lockout is the
  # expedition replay timer (AddReplayLockout), not this qglobal.
  quest::delglobal(_coirnav_done_key());
  $recovery_active = 0;
  $hard_cap_started = 0;
  $coirnav_wave = 0;
}

sub EVENT_SIGNAL {
  if ($signal == 1) { #signal on death of #Guardian_of_Coirnav
    SPAWN_WAVE1();
  } elsif ($signal == 2) { # Called when the weak version of the 3 named die
    if($coirnav_wave == 4) { # must be on wave 4
      my $pweloncheck = $entity_list->GetMobByNpcTypeID(216109);
      my $nrindacheck = $entity_list->GetMobByNpcTypeID(216108);
      my $vamuilcheck = $entity_list->GetMobByNpcTypeID(216110);
      if(!$pweloncheck && !$nrindacheck && !$vamuilcheck) { # You sank my battleship.
        quest::depop(216048); # Depop fake coirnav
        #Fear my corona of little bastards! Or, you could just DPS me down and this lot will depop.
        SPAWN_WAVE5();
      }
    }
  } elsif($signal == 4) {
    if ($coirnav_wave == 3) { #make sure we're on 3rd wave
      my $check_trash1 = $entity_list->GetMobByNpcTypeID(216071); # - Triloun Vapourfiend
      my $check_trash2 = $entity_list->GetMobByNpcTypeID(216076); # - Hraquis Icefiend
      my $check_trash3 = $entity_list->GetMobByNpcTypeID(216060); # - Regura Waterfiend
      if (!$check_trash1 && !$check_trash2 && !$check_trash3) { #if all trash is down, start wave 4
        quest::depop(216070); # Depop "tough" #Pwelon_of_Vapor
        quest::depop(216065); # Depop "tough" #Nrinda_of_Ice
        quest::depop(216061); # Depop "tough" #Vamuil_of_Water
        quest::spawn2(216109,0,0,$x+5,$y-20,$z+5,138); # Repop weak Pwelon_of_Vapor
        quest::spawn2(216108,0,0,$x-10,$y,$z+5,138); # Repop weak Nrinda_of_Ice
        quest::spawn2(216110,0,0,$x+5,$y+20,$z+5,138); # Repop weak Vamuil_of_Water
        $coirnav_wave = 4;
      }
    }
  } elsif($signal == 5) { #Signalled on death of the real coirnav
    quest::stopalltimers();
    quest::depopall(216074); #Depop any remaining trash creatures Triloun-057, regrua-067, hraquis-074
    quest::depopall(216067);
    quest::depopall(216057);
    $coirnav_wave = 0;
    quest::setglobal(_coirnav_done_key(), 3, 7, "D4"); # You can't attempt this again for 5 days
    quest::spawn2(216066,0,0,$x,$y,$z,138); #Spawn Essence of Water
  }
}

sub EVENT_TIMER {
  if($timer eq "hard_cap") {
    FAIL_EVENT();
    return;
  }
  if($timer eq "recovery") {
    FAIL_EVENT();
    return;
  }
  if($timer eq "recovery_check") {
    if(EVENT_IS_ENGAGED()) {
      quest::stoptimer("recovery");
      $recovery_active = 0;
    } elsif($hard_cap_started && !$recovery_active) {
      quest::settimer("recovery", $RECOVERY_SECONDS);
      $recovery_active = 1;
    }
    quest::settimer("recovery_check", 5);
    return;
  }

  if($timer == 2){ # 2nd wave of trash+named
    SPAWN_WAVE2();
  }

  if($timer == 3){ # 3nd wave of trash+named
    SPAWN_WAVE3();
  }

  if($timer == 4){ # Charging kickout (emote)
    quest::ze(0, "Coirnav the Avatar of Water is suddenly surrounded by a slight glow. A low constant humming is heard in the background.");
    quest::stoptimer(4);
  }

  if($timer == 5){ # Charging kickout 2
    quest::ze(0, "Coirnav the Avatar of Water is now glowing noticeably brighter and the constant humming is getting louder.");
    quest::stoptimer(5);
  }

  if($timer == 6){ # Last emote. You're outtatime.
    quest::ze(0, "Coirnav the Avatar of Water glows to brilliant flash of light that suddenly fades. The constant humming suddenly becomes a deafening roar that also mysteriously fades away.");
    quest::stoptimer(6);
  }

  if($timer == 7){ # kick players
    quest::stoptimer(7);
	KICK_ALL_PLAYERS();
  }
  if($timer == 8){ # Wave 4 check
    if ($coirnav_wave == 3) { #make sure we're on 3rd wave
      my $check_trash1 = $entity_list->GetMobByNpcTypeID(216071); # - Triloun Vapourfiend
      my $check_trash2 = $entity_list->GetMobByNpcTypeID(216076); # - Hraquis Icefiend
      my $check_trash3 = $entity_list->GetMobByNpcTypeID(216060); # - Regura Waterfiend
      if (!$check_trash1 && !$check_trash2 && !$check_trash3) { #if all trash is down, start wave 4
        SPAWN_WAVE4();
      }
    }
  }
  if($timer == 9) { #wave 5 check
    if($coirnav_wave == 4) { # must be on wave 4
      my $pweloncheck = $entity_list->GetMobByNpcTypeID(216109);
      my $nrindacheck = $entity_list->GetMobByNpcTypeID(216108);
      my $vamuilcheck = $entity_list->GetMobByNpcTypeID(216110);
      if(!$pweloncheck && !$nrindacheck && !$vamuilcheck) { # You sank my battleship.
        quest::depop(216048); # Depop fake coirnav
        #Fear my corona of little bastards! Or, you could just DPS me down and this lot will depop.
        SPAWN_WAVE5();
      }
    }
  }
}

sub EVENT_IS_ENGAGED {
  foreach my $npc_id (@event_mobs) {
    my $mob = $entity_list->GetMobByNpcTypeID($npc_id);
    if($mob && $mob->IsEngaged()) {
      return 1;
    }
  }
  return 0;
}

sub FAIL_EVENT {
  quest::stopalltimers();
  $recovery_active = 0;
  $hard_cap_started = 0;

  quest::depopall(216071); #Depop any remaining trash creatures
  quest::depopall(216076);
  quest::depopall(216074);
  quest::depopall(216060);
  quest::depopall(216067);
  quest::depopall(216057);

  quest::signalwith(216048,7,0); #Signal fake coirnav for banish emote
  quest::signalwith(216094,7,0); #or real coirnav for banish emote

  quest::depop(216061); # Depop any nameds up - sorry.
  quest::depop(216070);
  quest::depop(216065);
  quest::depop(216094);
  quest::depop(216108);
  quest::depop(216109);
  quest::depop(216110);
  $coirnav_wave = 0;
  quest::setglobal(_coirnav_done_key(), 3, 7, "H2"); # You can't attempt this again for 2 hours
  quest::settimer(7,45); # Kickout timer
}

sub KICK_ALL_PLAYERS {
  #zone all players back to PoKnowledge
  foreach $pc ($entity_list->GetClientList()) {
    $pc->MovePC(202, 1456, -1, -100, 0); # Zone: poknowledge
  }
}

sub SPAWN_WAVE1 {
  quest::settimer("hard_cap", $HARD_CAP_SECONDS); # 2 hour hard cap from wave 1 start
  quest::settimer("recovery_check", 5);           # heartbeat to start recovery when disengaged
  $hard_cap_started = 1;
  quest::settimer(2,180); # 3 Minute timer for second wave spawn
  quest::settimer(3,300); # 5 Minute timer for third wave
  quest::settimer(4,600); # 10 Minute timer for the emote
  quest::settimer(5,720); # 12 Minute timer for the emote
  quest::settimer(6,840); # 14 Minute timer for the emote

  quest::signalwith(216048,1,0); #Signal fake coirnav for first wave emote
  my $count = 0;
  while ($count <= 24) {
    $randX = int(rand(55));
    $randY = int(rand(75));
    $randZ = int(rand(20));
    $randH = int(rand(260));
    $randPNX = int(rand(2));
    $randPNY = int(rand(2));
    $randPNZ = int(rand(2));
    if($randPNX == 1) {
	  $randX = -$randX;
    }
    if($randPNY == 1) {
	  $randY = -$randY;
    }
    if($randPNZ == 1) {
	  $randZ = -$randZ;
    }
    quest::spawn2(216071,0,0,$x+$randX,$y+$randY,$z+$randZ,$randH); # Triloun Vaporfiend
    $count ++;
  }
  quest::spawn2(216070,0,0,$x+5,$y-20,$z+5,138); # Spawn #Pwelon_of_Vapor associated with this wave
  $coirnav_wave = 1;
}

sub SPAWN_WAVE2 {
  quest::signalwith(216048,2,0); #Signal fake coirnav for second wave emote
  my $count = 0;
  while ($count <= 24) {
    $randX = int(rand(55));
    $randY = int(rand(75));
    $randZ = int(rand(20));
    $randH = int(rand(260));
    $randPNX = int(rand(2));
    $randPNY = int(rand(2));
    $randPNZ = int(rand(2));
    if($randPNX == 1) {
	  $randX = -$randX;
    }
    if($randPNY == 1) {
	  $randY = -$randY;
    }
    if($randPNZ == 1) {
	  $randZ = -$randZ;
    }
    quest::spawn2(216076,0,0,$x+$randX,$y+$randY,$z+$randZ,$randH); #Hraquis icefiend
    $count ++;
  }
  quest::spawn2(216061,0,0,$x-10,$y,$z+5,138); # Spawn #Nrinda_of_Ice associated with this wave
  $coirnav_wave = 2;
  quest::stoptimer(2); # Stop timer
}

sub SPAWN_WAVE3 {
  quest::settimer(8,1); # 1 second timer to trigger 4th wave (safety check for failed signals)
  quest::signalwith(216048,3,0); #Signal fake coirnav for third wave emote
  my $count = 0;
  while ($count <= 24) {
    $randX = int(rand(55));
    $randY = int(rand(75));
    $randZ = int(rand(20));
    $randH = int(rand(260));
    $randPNX = int(rand(2));
    $randPNY = int(rand(2));
    $randPNZ = int(rand(2));
    if($randPNX == 1) {
	  $randX = -$randX;
    }
    if($randPNY == 1) {
	  $randY = -$randY;
    }
    if($randPNZ == 1) {
	  $randZ = -$randZ;
    }
    quest::spawn2(216060,0,0,$x+$randX,$y+$randY,$z+$randZ,$randH); # Regrua waterfiend
    $count ++;
  }
  quest::spawn2(216065,0,0,$x+5,$y+20,$z+5,138); # Spawn #Vamuil_of_Water associated with this wave
  $coirnav_wave = 3;
  quest::stoptimer(3); # Stop timer
}

sub SPAWN_WAVE4 {
  quest::depop(216070); # Depop "tough" #Pwelon_of_Vapor
  quest::depop(216065); # Depop "tough" #Nrinda_of_Ice
  quest::depop(216061); # Depop "tough" #Vamuil_of_Water
  quest::spawn2(216109,0,0,$x+5,$y-20,$z+5,138); # Repop weak Pwelon_of_Vapor
  quest::spawn2(216108,0,0,$x-10,$y,$z+5,138); # Repop weak Nrinda_of_Ice
  quest::spawn2(216110,0,0,$x+5,$y+20,$z+5,138); # Repop weak Vamuil_of_Water
  $coirnav_wave = 4;
  quest::stoptimer(8);
  quest::settimer(9,1); #timer to check for wave 4 to be dead and spawn wave 5. (safety check for failed signal)
}

sub SPAWN_WAVE5 {
  my $count = 0;
  while ($count <= 26) {
    $randX = int(rand(55));
    $randY = int(rand(75));
    $randZ = int(rand(20));
    $randH = int(rand(260));
    $randPNX = int(rand(2));
    $randPNY = int(rand(2));
    $randPNZ = int(rand(2));
    if($randPNX == 1) {
	  $randX = -$randX;
    }
    if($randPNY == 1) {
	  $randY = -$randY;
    }
    if($randPNZ == 1) {
	  $randZ = -$randZ;
    }
    if ($count <= 9) {
      quest::spawn2(216057,0,0,$x+$randX,$y+$randY,$z+$randZ,$randH); #Triloun Vaporling
    } elsif (($count >= 10) && ($count <=18)) {
      quest::spawn2(216067,0,0,$x+$randX,$y+$randY,$z+$randZ,$randH); #Regrua Waterling
    } elsif (($count >= 19) && ($count <=26)) {
      quest::spawn2(216074,0,0,$x+$randX,$y+$randY,$z+$randZ,$randH); #Hraquis Iceling
    }
    $count ++;
  }
  quest::spawn2(216094,0,0,$x,$y,$z-10,138); # The Real Corinav
  $coirnav_wave = 5;
  quest::stoptimer(9);
}
