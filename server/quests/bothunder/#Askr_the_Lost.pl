sub EVENT_SPAWN {
	# This quest-spawned Askr is a hail/transport NPC; keep raid AE from killing
	# or aggroing him before players can use him.
	$npc->SetSpecialAbility(19, 1); # Immune to melee
	$npc->SetSpecialAbility(20, 1); # Immune to magic
	$npc->SetSpecialAbility(24, 1); # Immune to aggro
	$npc->SetSpecialAbility(25, 1); # Immune to being aggroed
	$npc->SetSpecialAbility(35, 1); # Immune to harm from clients

	quest::say("All to me!");
	quest::settimer(1,3600);
}

sub EVENT_TIMER  {
if($timer == 1) {
quest::depop();
}
}

sub EVENT_SAY {
if($text=~/hail/i) {
$client->Message(9,"Kill the stormlord!");
quest::MovePCInstance(209,$instanceid,-727,-1662,1728); # Zone: bothunder
}
}
