#Guardian_of_Coirnav
#Signals coirnav_controller with the Event start

sub EVENT_SPAWN {
  # Lockout key is instance-scoped (see #coirnav_controller.pl) so concurrent powater instances don't collide.
  my $done_key = $instanceid ? "${instanceid}_coirnav_done" : "coirnav_done";
  if(defined $qglobals{$done_key} && $qglobals{$done_key} == 3) {
    quest::settimer(1,3);
  }
}

sub EVENT_AGGRO {
  quest::say("We are the protectors and guardians of this domain, death is all you will find here.");
}

sub EVENT_DEATH_COMPLETE {
  quest::say("Even now Coirnav awaits to deal swift death to you. Flee, weaklings.");
  quest::signalwith(216107,1,0); # NPC: #coirnav_controller
}

sub EVENT_TIMER {
  quest::stoptimer(1);
  # Re-verify the lockout right before depopping. #coirnav_controller clears a
  # stale lockout at fresh-instance boot; re-checking here avoids a boot-order
  # race where the Guardian read the stale value before the controller cleared it.
  my $done_key = $instanceid ? "${instanceid}_coirnav_done" : "coirnav_done";
  if(defined $qglobals{$done_key} && $qglobals{$done_key} == 3) {
    quest::depop_withtimer();
  }
}