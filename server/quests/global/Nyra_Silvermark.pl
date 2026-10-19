# Nyra Silvermark - Bazaar/Lobby Transport NPC (Global)
# Author: Straps
#
# Provides direct transport between Guild Lobby and Bazaar.
# Context-aware: offers different destinations based on current zone.
# Also step 2 of the Marketplace starter errand in the Guild Lobby.

sub EVENT_SAY {
    my $zoneid = $client->GetZoneID();
    
    if ($text =~ /hail/i) {
        # step 2 of the Marketplace starter errand (plugins/marketplace_starter.pl)
        if ($zoneid == 344 && plugin::MarketplaceStarterAdvance(2)) {
            plugin::Whisper("Caravans? $name, the roads have been thin for weeks. What reaches the Bazaar is picked over before it's unloaded.");
            plugin::Whisper("Check with Kilven the Quartermaster, but I wouldn't hope for much.");
            return;
        }
        if ($zoneid == 344 && plugin::MarketplaceStarterEligible() && plugin::MarketplaceStarterStep() eq "3") {
            plugin::Whisper("Kilven the Quartermaster keeps the hall's stores. Try him.");
            return;
        }
        if ($zoneid == 344) {
            plugin::Whisper("Greetings, $name. I am Nyra Silvermark, Steward of the Merchant Gate.");
            plugin::Whisper("I can transport you to the ".quest::saylink("bazaar", 1)." if you wish to conduct business with the merchants.");
        } elsif ($zoneid == 151) {
            plugin::Whisper("Greetings, $name. I am Nyra Silvermark, Steward of the Merchant Gate.");
            plugin::Whisper("I can transport you back to the ".quest::saylink("guild lobby", 1)." when you are finished with your business.");
        } else {
            plugin::Whisper("I am Nyra Silvermark, Steward of the Merchant Gate. I only operate between the Guild Lobby and the Bazaar.");
        }
    }
    elsif ($text =~ /bazaar/i) {
        if ($zoneid == 344) {
            plugin::Whisper("Transporting you to the Bazaar...");
            #quest::movepc(151, -18, 55, 2.4, 497);  new bazaar
            quest::movepc(151, -9, -810, 3.75, 251);
        } else {
            plugin::Whisper("I can only transport you to the Bazaar from the Guild Lobby.");
        }
    }
    elsif ($text =~ /guild lobby/i) {
        if ($zoneid == 151) {
            plugin::Whisper("Transporting you to the Guild Lobby...");
            quest::movepc(344, 0.8, 121.35, 1.75, 255);
        } else {
            plugin::Whisper("I can only transport you to the Guild Lobby from the Bazaar.");
        }
    }
}

1;
