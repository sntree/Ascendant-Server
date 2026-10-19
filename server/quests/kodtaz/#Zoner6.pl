#Ikkinz Raid #3: Chambers of Transcendence 
sub EVENT_SPAWN {
  	$x = $npc->GetX();
   	$y = $npc->GetY();
	quest::set_proximity($x - 25, $x + 25, $y - 25, $y + 25); # Ascendant: was +-10, hard to find
}

sub EVENT_ENTER {
  # Ascendant: ignore players who just zoned in here (exit from the trial lands on this trigger)
  my $zonein = $client->GetEntityVariable("kt_zonein");
  if ($zonein ne "" && time() - $zonein < 10) {
    $client->Message(15, "Step off and back onto the entrance to re-enter the temple.");
    return;
  }
	$InInstanceIkky6 = quest::GetInstanceID("ikkinz",5);
	
    if($InInstanceIkky6 > 0){
		quest::MovePCInstance(294,$InInstanceIkky6,-157,27,-2); # Zone: ikkinz
	}
        else {
		$client->Message(13, "You are not a part of an instance!");
	}
    
}
