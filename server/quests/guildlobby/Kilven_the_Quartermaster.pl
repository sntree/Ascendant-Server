# Kilven the Quartermaster (Guild Lobby)
# Step 3 of the Marketplace starter errand (plugins/marketplace_starter.pl).

sub EVENT_SAY {
  if ($text =~ /hail/i) {
    if (plugin::MarketplaceStarterAdvance(3)) {
      plugin::Whisper("Torrick's empty and the caravans are late? Then my stores won't save you either, $name. Rations and rope, that's all I have left.");
      plugin::Whisper("Tell Exarch Valeth the halls simply don't have enough good items. He'll have to find another way to see you equipped.");
      return;
    }
    if (plugin::MarketplaceStarterEligible() && plugin::MarketplaceStarterStep() eq "4") {
      plugin::Whisper("Go on back to Exarch Valeth. There's nothing more on my shelves.");
      return;
    }
    quest::say("Greetings, $name. I am Kilven, quartermaster of this hall. I maintain supplies for adventurers. Browse my wares if you need provisions.");
  }
}

1;
