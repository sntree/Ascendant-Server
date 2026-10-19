# Instance-scoped lockout key so concurrent poearthb instances don't share the Rathe Council lockout.
sub _rathe_key {
   return $instanceid ? "${instanceid}_poeb_rathe" : "poeb_rathe";
}

sub EVENT_SPAWN { 
   quest::spawn_condition($zonesn,1,0); #Make sure we aren't up yet.
 # Fresh DZ boot: clear any stale Rathe lockout inherited from a PREVIOUS expedition
 # that reused this instance id. Otherwise a recycled id makes the controller wait
 # for the (up to 5 day) global to expire before popping the Rathe Council. The real
 # cross-session lockout is the expedition replay timer, not this qglobal.
 quest::delglobal(_rathe_key()) if $instanceid;
 my $rathe_key = _rathe_key();
 if (defined $qglobals{$rathe_key}){
   quest::settimer("rathe",1); #waiting for the global to expire to pop rathe
   }
 if (!defined $qglobals{$rathe_key}) { 
   quest::spawn_condition($zonesn,1,1); #Rathe pop 
   quest::settimer("avatar",1); 
   } 
} 

sub EVENT_SIGNAL {
  quest::settimer("rathe",1);
}

sub EVENT_TIMER { 
  my $rathe_key = _rathe_key();
  if ($timer eq "rathe" && !defined $qglobals{$rathe_key}) {
      quest::stoptimer("rathe");
      quest::spawn_condition($zonesn,1,1); #Rathe pop 
      quest::settimer("avatar",1); #waiting for rathe to go down to pop avatar
  }
  if ($timer eq "avatar" && !defined $qglobals{$rathe_key}) { 
      $boss = 0; 
      $check_boss = $entity_list->GetMobByNpcTypeID(222008); 
         if($check_boss) { 
            $boss = 1; 
         } 
      $check_boss = $entity_list->GetMobByNpcTypeID(222013); 
         if($check_boss) { 
            $boss = 1; 
         } 
      if($boss == 0) { 
         quest::stoptimer("avatar"); 
         quest::spawn_condition($zonesn,1,0); #Rathe depop 
         quest::spawn2(222014,0,0,2051.1,407.7,-219.2,0); #Avatar of Earth pop 
      } 
   } 
} 
