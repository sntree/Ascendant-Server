# Instance-scoped qglobal keys so concurrent poair instances run the ring event independently.
sub _ring_key { return ($instanceid ? "${instanceid}_" : "") . $_[0]; }

sub EVENT_SPAWN 
{
	$counter = 0;
	# Fresh DZ boot: clear any stale ring lockout inherited from a PREVIOUS expedition
	# that reused this instance id. mist_done is set "F" (forever), so a recycled id
	# would otherwise permanently skip the Mist ring/Avatar. Cross-session lockout is
	# the expedition replay timer, not these qglobals.
	quest::delglobal(_ring_key("mist_done"));
	quest::delglobal(_ring_key("mist_event_start"));
}    

sub EVENT_SIGNAL 
{
	if ($signal == 1 && defined($qglobals{_ring_key("mist_event_start")}) && !defined($qglobals{_ring_key("mist_done")})) 
	{
     		$counter+=1;
     
     		if ($counter == 4) 
     		{
			quest::spawn2(215070,0,0,323.7,-721.3,439.3,165.8); # NPC: #Melernil_Faal`Armanna
        	}
     	}
     	elsif ($signal == 2 && defined($qglobals{_ring_key("mist_event_start")}) && !defined($qglobals{_ring_key("mist_done")})) 
     	{
        	quest::spawn2(215071,0,0,-1571.6,-570.0,343.6,381.8); # NPC: #Avatar_of_Mist
		quest::depop_withtimer(215023);
     	}
     	elsif ($signal == 3  && !defined($qglobals{_ring_key("mist_done")})) 
     	{
        	quest::setglobal(_ring_key("mist_done"),1,3,"F");
     	}
	elsif ($signal == 4 && !defined($qglobals{_ring_key("mist_event_start")}) && !defined($qglobals{_ring_key("mist_done")})) 
	{
		#A_Phoenix_Breezewing, A_Wind_Pheonix X2, Servitor_of_Xegony, Spiritfrenzied_Phoenix, Ardent_Phoenix_of_Xegony, A_Phoenix_Searedwing, Calebgrothiel
		if(!$entity_list->IsMobSpawnedByNpcTypeID(215058) && !$entity_list->IsMobSpawnedByNpcTypeID(215026) && !$entity_list->IsMobSpawnedByNpcTypeID(215025) && !$entity_list->IsMobSpawnedByNpcTypeID(215027) && !$entity_list->IsMobSpawnedByNpcTypeID(215062) && !$entity_list->IsMobSpawnedByNpcTypeID(215059) && !$entity_list->IsMobSpawnedByNpcTypeID(215028) && !$entity_list->IsMobSpawnedByNpcTypeID(215061)) 
		{
			quest::spawn2(215069,0,0,304.2,-792.2,441.6,30.4); # NPC: A_Phoenix_Windsurger
			quest::spawn2(215069,0,0,348.4,-632.2,442.3,280.2); # NPC: A_Phoenix_Windsurger
			quest::spawn2(215069,0,0,391.7,-736.2,438.1,406.8); # NPC: A_Phoenix_Windsurger
			quest::spawn2(215069,0,0,260,-699.8,444.5,151); # NPC: A_Phoenix_Windsurger
			quest::setglobal(_ring_key("mist_event_start"),1,3,"H2");
			$counter=0;
		}
	}
	elsif ($signal == 5)
	{
		quest::delglobal(_ring_key("mist_done"));
	}
}
