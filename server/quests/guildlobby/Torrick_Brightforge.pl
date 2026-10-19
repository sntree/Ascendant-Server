# Torrick Brightforge - Weapon Ornamentation merchant (Guild Lobby)
# Step 1 of the Marketplace starter errand (plugins/marketplace_starter.pl).

sub EVENT_SAY {
    if ($text =~ /hail/i) {
        if (plugin::MarketplaceStarterAdvance(1)) {
            plugin::Whisper("Valeth sent you about stock? Ha. Look at these racks, $name. Half the ornaments I used to carry never came back from the forges this season.");
            plugin::Whisper("Go ask Nyra Silvermark at the Merchant Gate. If the Bazaar caravans can't bring more, nobody can.");
            return;
        }
        if (plugin::MarketplaceStarterEligible() && plugin::MarketplaceStarterStep() eq "2") {
            plugin::Whisper("Nyra Silvermark, at the Merchant Gate. She'll know about the caravans.");
            return;
        }
        quest::say("Well met, $name. I fit weapons with ornamentation. Have a look at my wares.");
    }
}

1;
