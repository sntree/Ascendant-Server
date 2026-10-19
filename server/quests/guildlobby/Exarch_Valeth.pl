# Exarch Valeth - Ascendant Benediction Vendor (Guild Lobby)
# Author: Straps
#
# Manages server-wide Ascendant Benedictions (haste, healing, run speed, thought).
# Players spend Marks of Ascendance to extend buff timers by 6h (max 48h) for
# ALL online players. Bulk pricing mode lets 1 Mark extend all 4 at once.
# Also sells Expedition Port Passes (account-wide unlock) and trades a
# Buff Bag Kit item (17674) for the satchel + wand pair.
# Converts Marks into Marketplace Coins (account-wide, one way, with a confirm popup).

my $MOA_ITEM_ID = 1800; # Mark of Ascendance
my $MOA_CURRENCY_ID = 3; # the same Mark in the Alt Currency window
my $COINS_PER_MARK = 100; # Marketplace Coins per Mark, one way
my @MARK_CHOICES = (1, 5, 25); # convert links offered on hail

# PRICING MODE TOGGLE
# Set to 1 for startup mode (1 Mark extends ALL 4 buffs)
# Set to 0 for normal mode (1 Mark per individual buff)
my $BULK_PRICING_MODE = 1;

# Marketplace starter errand (plugins/marketplace_starter.pl): once per account, ToB client only
sub starter_hail {
    return unless plugin::MarketplaceStarterEligible();
    my $step  = plugin::MarketplaceStarterStep();
    my $coins = plugin::MarketplaceStarterCoins();
    plugin::MarketplaceStarterEnsureTask();

    if ($step eq "") {
        plugin::Whisper("One more thing, $name. The halls' merchants have been grumbling about their stock. Would you look into it for me? I'll make it worth your while: $coins Marketplace Coins for your account. " . quest::saylink("supply errand", 1, "I'll look into it"));
    } elsif ($step eq "1") {
        plugin::Whisper("Start with Torrick Brightforge, the weapon ornamenter here in the lobby.");
    } elsif ($step eq "2") {
        plugin::Whisper("Torrick sent you on? Then see Nyra Silvermark at the Merchant Gate.");
    } elsif ($step eq "3") {
        plugin::Whisper("Kilven the Quartermaster is the last to ask.");
    } elsif ($step eq "4") {
        starter_reward();
    }
}

sub starter_accept {
    return unless plugin::MarketplaceStarterEligible();
    return unless plugin::MarketplaceStarterStep() eq "";
    plugin::MarketplaceStarterSetStep(1);
    $client->AssignTask(plugin::MarketplaceStarterTask());
    plugin::Whisper("Good. Ask Torrick Brightforge first; he fits weapon ornaments here in the lobby. Then come back to me with what they say.");
}

sub starter_reward {
    my $coins = plugin::MarketplaceStarterCoins();
    # marked done before the grant, so a second character on the account can't be paid too
    plugin::MarketplaceStarterSetStep("done");
    if (!$client->AddMarketplaceCoins($coins, "Starter errand: Exarch Valeth")) {
        plugin::MarketplaceStarterSetStep(4);
        plugin::Whisper("Your account can't hold $coins more Marketplace Coins right now. Spend some and come back to me.");
        return;
    }
    $client->UpdateTaskActivity(plugin::MarketplaceStarterTask(), 3, 1);
    plugin::Whisper("Not enough good items anywhere, then. I feared as much. The Marketplace has what the halls can't stock.");
    plugin::Whisper("Here, take these Marketplace Coins and open the Marketplace to see what it offers. They're on your account, so every character of yours can spend them.");
    $client->Message(15, "You received $coins Marketplace Coins. Your account now has " . $client->GetMarketplaceCoins() . ".");
}

sub EVENT_SAY {
    my $player_name = $client->GetCleanName();
    
    if ($text =~ /hail/i) {
        my $now = time();
        my %auras = (
            "Haste" => "ascendant_aura_haste_expires",
            "Healing" => "ascendant_aura_healing_expires",
            "Run Speed" => "ascendant_aura_speed_expires",
            "Thought" => "ascendant_aura_thought_expires"
        );

        my @status_lines;
        foreach my $aura_name (sort keys %auras) {
            my $value = quest::get_data($auras{$aura_name});
            my $remaining = $value ? $value - $now : 0;
            if ($remaining > 0) {
                my $hours = int($remaining / 3600);
                my $minutes = int(($remaining % 3600) / 60);
                push @status_lines, "- <c \"#00FF00\">$aura_name</c> - ${hours}h ${minutes}m left";
            } else {
                push @status_lines, "- <c \"#808080\">$aura_name - inactive</c>";
            }
        }

        my $in_bags = $client->CountItem($MOA_ITEM_ID);
        my $in_currency = $client->GetAlternateCurrencyValue($MOA_CURRENCY_ID);
        my $marks = $in_bags + $in_currency;
        my $coins = $client->GetMarketplaceCoins();
        my $has_port = quest::get_data("ascendant_expedition_port_" . $client->AccountID());
        my $gold = "<c \"#FFD700\">";
        my $head = "<c \"#FFCC00\">";
        my $grey = "<c \"#A0A0A0\">";

        my $popup_text = "${gold}Well met, $name.</c> I keep the Ascendant World Benedictions and trade in Marks of Ascendance.<br><br>";
        $popup_text .= "${head}Your Marks</c><br>";
        $popup_text .= "- Marks of Ascendance: ${gold}$marks</c> ${grey}($in_currency in currency, $in_bags in bags)</c><br>";
        $popup_text .= "- Marketplace Coins: ${gold}$coins</c><br><br>";
        $popup_text .= "${head}World Benedictions</c> ${grey}(for everyone online)</c><br>";
        $popup_text .= join("<br>", @status_lines) . "<br><br>";
        $popup_text .= "${head}What I offer</c><br>";
        if ($BULK_PRICING_MODE) {
            $popup_text .= "- All four Benedictions +6 hours ${grey}(max 48h)</c> - ${gold}1 Mark</c><br>";
        } else {
            $popup_text .= "- One Benediction +6 hours ${grey}(max 48h)</c> - ${gold}1 Mark</c><br>";
        }
        $popup_text .= "- Marketplace Coins - ${gold}1 Mark = $COINS_PER_MARK coins</c> ${grey}(one way)</c><br>";
        $popup_text .= "- Expedition Port Pass - " . ($has_port ? "${grey}owned by your account</c>" : "${gold}3 Marks</c>") . "<br><br>";
        $popup_text .= "${grey}Choose from the links in your chat window.</c>";

        $client->Popup2("Exarch Valeth - Mark of Ascendance Vendor", $popup_text, 0, 0, 0, 0);

        my @extend = $BULK_PRICING_MODE
            ? (quest::saylink("extend haste", 1, "extend all four"))
            : map { quest::saylink("extend $_", 1) } ("haste", "healing", "speed", "thought");
        plugin::Whisper("Benedictions: " . join(" | ", @extend));
        plugin::Whisper("Marketplace Coins ($COINS_PER_MARK per Mark, can't be turned back): " . join(" | ", map { quest::saylink("convert $_ mark" . ($_ == 1 ? "" : "s"), 1, "convert $_") } @MARK_CHOICES));
        plugin::Whisper($has_port ? "Your account has the Expedition Port Pass; the Guild Lobby porters can send you to your expedition." : "Expedition Port Pass (3 Marks, whole account): " . quest::saylink("expedition port pass", 1, "buy"));
        starter_hail();
    }
    elsif ($text =~ /^supply errand$/i) {
        starter_accept();
    }
    elsif ($text =~ /extend speed/i) {
        if (marks_owned() < 1) {
            plugin::Whisper("You need 1 Mark of Ascendance to extend this benediction. You currently have none.");
            return;
        }
        
        my $bucket_key = "ascendant_aura_speed_expires";
        my $value = quest::get_data($bucket_key);
        my $now = time();
        
        if ($value && $value > $now) {
            my $remaining = $value - $now;
            if ($remaining > 151200) {
                my $hours_remaining = int($remaining / 3600);
                plugin::Whisper("The Ascendant Run Speed benediction still has $hours_remaining hours remaining. Please return when it has less than 42 hours.");
                return;
            }
        }
        
        $client->Popup2(
            "Confirm Extension - Run Speed",
            "<c \"#FFCC00\">Cost:</c> 1 Mark of Ascendance<br><br>"
            . "This will extend the <c \"#00FF00\">Ascendant Run Speed</c> benediction by <c \"#00FFFF\">6 hours</c> for <c \"#FFD700\">ALL players</c> on the server.<br><br>"
            . "Do you wish to proceed?",
            1001,
            1002,
            2, 0,
            "Confirm", "Cancel"
        );
    }
    elsif ($text =~ /extend healing/i) {
        if (marks_owned() < 1) {
            plugin::Whisper("You need 1 Mark of Ascendance to extend this benediction. You currently have none.");
            return;
        }
        
        my $bucket_key = "ascendant_aura_healing_expires";
        my $value = quest::get_data($bucket_key);
        my $now = time();
        
        if ($value && $value > $now) {
            my $remaining = $value - $now;
            if ($remaining > 151200) {
                my $hours_remaining = int($remaining / 3600);
                plugin::Whisper("The Ascendant Healing benediction still has $hours_remaining hours remaining. Please return when it has less than 42 hours.");
                return;
            }
        }
        
        $client->Popup2(
            "Confirm Extension - Healing",
            "<c \"#FFCC00\">Cost:</c> 1 Mark of Ascendance<br><br>"
            . "This will extend the <c \"#00FF00\">Ascendant Healing</c> benediction by <c \"#00FFFF\">6 hours</c> for <c \"#FFD700\">ALL players</c> on the server.<br><br>"
            . "Do you wish to proceed?",
            1003,
            1004,
            2, 0,
            "Confirm", "Cancel"
        );
    }
    elsif ($text =~ /extend haste/i) {
        if (marks_owned() < 1) {
            plugin::Whisper("You need 1 Mark of Ascendance to extend this benediction. You currently have none.");
            return;
        }
        
        my $bucket_key = "ascendant_aura_haste_expires";
        my $value = quest::get_data($bucket_key);
        my $now = time();
        
        if ($value && $value > $now) {
            my $remaining = $value - $now;
            if ($remaining > 151200) {
                my $hours_remaining = int($remaining / 3600);
                plugin::Whisper("The Ascendant Haste benediction still has $hours_remaining hours remaining. Please return when it has less than 42 hours.");
                return;
            }
        }
        
        $client->Popup2(
            "Confirm Extension - Haste",
            "<c \"#FFCC00\">Cost:</c> 1 Mark of Ascendance<br><br>"
            . "This will extend the <c \"#00FF00\">Ascendant Haste</c> benediction by <c \"#00FFFF\">6 hours</c> for <c \"#FFD700\">ALL players</c> on the server.<br><br>"
            . "Do you wish to proceed?",
            1005,
            1006,
            2, 0,
            "Confirm", "Cancel"
        );
    }
    elsif ($text =~ /extend thought/i) {
        if (marks_owned() < 1) {
            plugin::Whisper("You need 1 Mark of Ascendance to extend this benediction. You currently have none.");
            return;
        }
        
        my $bucket_key = "ascendant_aura_thought_expires";
        my $value = quest::get_data($bucket_key);
        my $now = time();
        
        if ($value && $value > $now) {
            my $remaining = $value - $now;
            if ($remaining > 151200) {
                my $hours_remaining = int($remaining / 3600);
                plugin::Whisper("The Ascendant Thought benediction still has $hours_remaining hours remaining. Please return when it has less than 42 hours.");
                return;
            }
        }
        
        $client->Popup2(
            "Confirm Extension - Thought",
            "<c \"#FFCC00\">Cost:</c> 1 Mark of Ascendance<br><br>"
            . "This will extend the <c \"#00FF00\">Ascendant Thought</c> benediction by <c \"#00FFFF\">6 hours</c> for <c \"#FFD700\">ALL players</c> on the server.<br><br>"
            . "Do you wish to proceed?",
            1007,
            1008,
            2, 0,
            "Confirm", "Cancel"
        );
    }
    elsif ($text =~ /^convert (\d+)( marks?)?$/i && grep { $_ == $1 } @MARK_CHOICES) {
        my $marks = int($1);
        my $have = marks_owned();
        if ($have < $marks) {
            plugin::Whisper("You need $marks Mark" . ($marks == 1 ? "" : "s") . " of Ascendance for that; you have $have.");
            return;
        }
        if ($marks > marketplace_marks_room()) {
            plugin::Whisper("That would go over the most Marketplace Coins an account can hold (" . marketplace_max_coins() . "). Try a smaller amount.");
            return;
        }

        my $coins = $marks * $COINS_PER_MARK;
        my $balance = $client->GetMarketplaceCoins();
        $client->SetEntityVariable("marketplace_convert_marks", $marks);
        $client->Popup2(
            "Confirm Conversion - Marketplace Coins",
            "<c \"#FFCC00\">Convert:</c> $marks Mark" . ($marks == 1 ? "" : "s") . " of Ascendance<br>"
            . "<c \"#FFCC00\">You receive:</c> $coins Marketplace Coins (account-wide)<br><br>"
            . "Marketplace Coins: $balance -> " . ($balance + $coins) . "<br><br>"
            . "<c \"#FF4040\">This cannot be undone.</c> Marketplace Coins can never be turned back into Marks.",
            1011,
            1012,
            2, 0,
            "Convert", "Cancel"
        );
    }
    elsif ($text =~ /expedition port pass/i) {
        my $acct_id = $client->AccountID();
        my $has_port = quest::get_data("ascendant_expedition_port_" . $acct_id);
        
        if ($has_port) {
            plugin::Whisper("Your account already has the Expedition Port Pass.");
            return;
        }
        
        if (marks_owned() < 3) {
            plugin::Whisper("You need 3 Marks of Ascendance to purchase the Expedition Port Pass. You have " . marks_owned() . ".");
            return;
        }
        
        $client->Popup2(
            "Confirm Purchase - Expedition Port Pass",
            "<c \"#FFCC00\">Cost:</c> 3 Marks of Ascendance<br><br>"
            . "This will unlock the <c \"#00FF00\">Expedition Port Pass</c> for your <c \"#FFD700\">entire account</c>.<br><br>"
            . "Once purchased, any character on this account can teleport directly to their active expedition from the wizard or druid porters in the Guild Lobby.<br><br>"
            . "This is a <c \"#00FFFF\">permanent</c> unlock.",
            1009,
            1010,
            2, 0,
            "Purchase", "Cancel"
        );
    }
}

sub EVENT_POPUPRESPONSE {
    my $player_name = $client->GetCleanName();
    
    my $bucket_key;
    my $aura_name;
    
    # Determine which aura based on popup ID (only confirm IDs)
    if ($popupid == 1001) {
        $bucket_key = "ascendant_aura_speed_expires";
        $aura_name = "Run Speed";
    } elsif ($popupid == 1003) {
        $bucket_key = "ascendant_aura_healing_expires";
        $aura_name = "Healing";
    } elsif ($popupid == 1005) {
        $bucket_key = "ascendant_aura_haste_expires";
        $aura_name = "Haste";
    } elsif ($popupid == 1007) {
        $bucket_key = "ascendant_aura_thought_expires";
        $aura_name = "Thought";
    } elsif ($popupid == 1002 || $popupid == 1004 || $popupid == 1006 || $popupid == 1008) {
        # Cancel buttons for benedictions
        plugin::Whisper("Extension cancelled.");
        return;
    } elsif ($popupid == 1009) {
        # Expedition Port Pass confirm
        my $acct_id = $client->AccountID();
        my $has_port = quest::get_data("ascendant_expedition_port_" . $acct_id);
        
        if ($has_port) {
            plugin::Whisper("Your account already has the Expedition Port Pass.");
            return;
        }
        if (marks_owned() < 3) {
            plugin::Whisper("You no longer have enough Marks of Ascendance.");
            return;
        }
        
        take_marks(3);
        quest::set_data("ascendant_expedition_port_" . $acct_id, "1");
        
        $client->Message(15, "You have unlocked the Expedition Port Pass for your account!");
        plugin::Whisper("The porters in the Guild Lobby can now send you directly to your active expedition.");
        return;
    } elsif ($popupid == 1011) {
        # Marks to Marketplace Coins confirm
        my $marks = int($client->GetEntityVariable("marketplace_convert_marks") || 0);
        $client->DeleteEntityVariable("marketplace_convert_marks");
        if ($marks < 1) {
            return;
        }
        if (marks_owned() < $marks) {
            plugin::Whisper("You no longer have enough Marks of Ascendance.");
            return;
        }
        if ($marks > marketplace_marks_room()) {
            plugin::Whisper("That would go over the Marketplace Coin limit. Nothing was converted.");
            return;
        }

        my $coins = $marks * $COINS_PER_MARK;
        take_marks($marks);
        $client->AddMarketplaceCoins($coins, "Exarch Valeth: converted $marks Mark" . ($marks == 1 ? "" : "s") . " of Ascendance");
        $client->Message(15, "You converted $marks Mark" . ($marks == 1 ? "" : "s") . " of Ascendance into $coins Marketplace Coins. Your account now has " . $client->GetMarketplaceCoins() . ".");
        return;
    } elsif ($popupid == 1012) {
        plugin::Whisper("Conversion cancelled. Your Marks are untouched.");
        return;
    } elsif ($popupid == 1010) {
        # Cancel button for port pass
        plugin::Whisper("Purchase cancelled.");
        return;
    } else {
        return;
    }
    
    # Verify player still has a Mark
    if (marks_owned() < 1) {
        plugin::Whisper("You no longer have a Mark of Ascendance.");
        return;
    }
    
    my $value = quest::get_data($bucket_key);
    my $now = time();
    
    # Double-check time limit
    if ($value && $value > $now) {
        my $remaining = $value - $now;
        if ($remaining > 151200) {
            my $hours_remaining = int($remaining / 3600);
            plugin::Whisper("The Ascendant $aura_name benediction still has $hours_remaining hours remaining. Please return when it has less than 42 hours.");
            return;
        }
    }
    
    # Remove the Mark
    take_marks(1);
    
    if ($BULK_PRICING_MODE) {
        # BULK MODE: Extend ALL 4 buffs
        my %all_auras = (
            "Haste" => "ascendant_aura_haste_expires",
            "Healing" => "ascendant_aura_healing_expires",
            "Run Speed" => "ascendant_aura_speed_expires",
            "Thought" => "ascendant_aura_thought_expires"
        );
        
        foreach my $aura (keys %all_auras) {
            my $key = $all_auras{$aura};
            my $val = quest::get_data($key);
            
            if ($val && $val > $now) {
                my $new_expires = $val + 21600;
                my $max_expires = $now + 172800;
                $new_expires = $max_expires if $new_expires > $max_expires;
                quest::set_data($key, $new_expires);
            } else {
                quest::set_data($key, $now + 21600);
            }
        }
        
        # World announcement for bulk mode
        quest::we(15, "Exarch Valeth proclaims: $player_name has extended ALL Ascendant Benedictions by 6 hours!");
        plugin::Whisper("All 4 Ascendant Benedictions have been extended by 6 hours for all adventurers!");
    } else {
        # NORMAL MODE: Extend single aura
        if ($value && $value > $now) {
            my $new_expires = $value + 21600;
            my $max_expires = $now + 172800;
            $new_expires = $max_expires if $new_expires > $max_expires;
            quest::set_data($bucket_key, $new_expires);
        } else {
            quest::set_data($bucket_key, $now + 21600);
        }
        
        # World announcement for single aura
        quest::we(15, "Exarch Valeth proclaims: $player_name has extended the Ascendant $aura_name benediction by 6 hours!");
        plugin::Whisper("The Ascendant $aura_name benediction has been extended by 6 hours for all adventurers!");
    }
    
    # Signal all online clients to apply auras immediately
    quest::worldwidesignalclient(999);
}

# Marks in the Alt Currency window plus Mark items in the bags
sub marks_owned {
    return $client->CountItem($MOA_ITEM_ID) + $client->GetAlternateCurrencyValue($MOA_CURRENCY_ID);
}

# spends from the Alt Currency window first, then Mark items; callers check marks_owned() first
sub take_marks {
    my $count = shift;
    my $from_currency = $client->GetAlternateCurrencyValue($MOA_CURRENCY_ID);
    $from_currency = $count if $from_currency > $count;
    if ($from_currency > 0) {
        $client->RemoveAlternateCurrencyValue($MOA_CURRENCY_ID, $from_currency);
    }
    if ($count > $from_currency) {
        $client->RemoveItem($MOA_ITEM_ID, $count - $from_currency);
    }
}

# Marketplace Coins an account may hold (server rule, 100000 by default)
sub marketplace_max_coins {
    my $max = quest::get_rule("Character:MarketplaceMaxCoins");
    return ($max && $max > 0) ? $max : 100000;
}

# how many Marks still fit under the coin limit
sub marketplace_marks_room {
    my $room = marketplace_max_coins() - $client->GetMarketplaceCoins();
    return $room > 0 ? int($room / $COINS_PER_MARK) : 0;
}

sub EVENT_ITEM {
    if (quest::handin({17674 => 1})) {
        $client->SummonItem(17672);
        $client->SummonItem(17673);
        plugin::Whisper("Take this satchel and wand. Place your buff scrolls within the satchel, then use the wand to unleash their power.");
        $client->Message(18, "You receive an Ascendant Buff Satchel and an Ascendant Buff Wand!");
    } else {
        plugin::Whisper("I have no use for this.");
        plugin::return_items(\%itemcount);
    }
}

1;
