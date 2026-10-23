# Randall of the Fellows (202396), Fellowship Registrar: explains fellowships and hands out the insignia
# items: 34023
my $FELLOWSHIP_INSIGNIA = 34023; # Fellowship Registration Insignia, clicks Fellowship Calling (7618)

sub EVENT_SAY {
	if (quest::get_rule("Character:EnableFellowships") ne "true") {
		if ($text=~/hail/i) {
			quest::say("Greetings, $name. The fellowship registry is closed for now. Do come back another time.");
		}
		return;
	}

	if ($text=~/hail/i) {
		quest::say("Greetings, $name! Would you like to know how to [" . quest::saylink("set up") . "] a fellowship, or would you rather hear some [" . quest::saylink("information") . "] about them first?");
	}
	elsif ($text=~/set up/i) {
		quest::say("Open your fellowship log book (Ctrl+Shift+F) and press Create, and you will lead a fellowship of your own. Target a friend and press Invite to bring them in, up to twelve of you in all. Choose them well: a fellowship is for the few you truly trust. If you need more than that, a guild may suit you better.");
	}
	elsif ($text=~/information/i) {
		quest::say("Happy to help someone new to fellowships! I can tell you [" . quest::saylink("what") . "] a fellowship is, how to [" . quest::saylink("set up") . "] one, and [" . quest::saylink("when") . "] and [" . quest::saylink("how") . "] to make a fellowship campsite.");
	}
	elsif ($text=~/what/i) {
		quest::say("A fellowship is a band of friends who adventure together, grouped or not. Its campsite is a gathering point: once it is lit, any member can travel straight to it and be ready for the next fight.");
	}
	elsif ($text=~/when/i) {
		quest::say("A campsite takes some effort, so bring at least two friends from your fellowship. Once the three of you stand together somewhere you like, any one of you can light the campfire.");
	}
	elsif ($text=~/how/i) {
		quest::say("Open your log book and go to the Camp page. Pick the Fellowship of Honor, gather two fellowship friends close by, and press Create Campsite. Take this Fellowship Registration Insignia as well. It is bound to your fellowship, and using it carries you to wherever your fellowship's campsite stands.");
		if (!plugin::check_hasitem($client, $FELLOWSHIP_INSIGNIA)) {
			quest::summonitem($FELLOWSHIP_INSIGNIA); # Item: Fellowship Registration Insignia
		}
	}
}
