# Aurelian Stoneward - Expansion Completion Tracker (Guild Lobby)
# Author: Straps
#
# Tracks which raid bosses a character has defeated per expansion era
# (Classic, Kunark, Velious, Luclin) and awards one-time rewards when all bosses
# for an era are confirmed dead. Kill flags are stored per-account with
# character name validation; rewards are flagged per-character so alts
# must earn their own completions.

my @pop_completion_wands = (
    4039, # Polymorph Wand: Bertoxxulous
    4040, # Polymorph Wand: Saryrn
    4041, # Polymorph Wand: Vallon
);

sub GrantPoPCompletionWands {
    my ($char_id) = @_;
    return 0 if quest::get_data("pop_wands_awarded_" . $char_id);

    my $granted = 0;
    foreach my $item_id (@pop_completion_wands) {
        next if plugin::check_hasitem($client, $item_id);
        $client->SummonItem($item_id);
        $granted = 1;
    }

    quest::set_data("pop_wands_awarded_" . $char_id, $client->GetCleanName());
    return $granted;
}

# Gates of Discord finishing set (owner "accessible set", 2026-09-26). Buckets are written
# per account by global/global_npc.pl EVENT_KILLED_MERIT (%GOD_COMPLETION_BOSSES).
my @god_completion_bosses = (
    ['god_cragbeastqueen_', 'Cragbeast Queen',         'Natimbi'],
    ['god_zunmuramvotal_',  'Zun`Muram Votal',         'Ferubi'],
    ['god_vrexbarxt_',      'Vrex Barxt Qurat',        'Uqua, the Ocean God Chantry'],
    ['god_keeperaltar_',    'Keeper of the Altar',     'Ikkinz, Antechamber of Destruction'],
    ['god_iqthinxa_',       'Iqthinxa Karnkvi',        'Qvic, Prayer Grounds of Calling'],
    ['god_noqufiel_',       'Noqufiel',                "Inktu`Ta, the Unmasked Chapel"],
);

# Returns (is_complete, [ [boss_name, zone, defeated], ... ]) for the current client.
sub GetGoDProgress {
    my $account_id = $client->AccountID();
    my $char_name  = $client->GetCleanName();
    my @rows;
    my $complete = 1;
    foreach my $b (@god_completion_bosses) {
        my ($key, $boss, $zone) = @$b;
        my $who = quest::get_data($key . $account_id);
        my $done = ($who && $who eq $char_name) ? 1 : 0;
        $complete = 0 unless $done;
        push @rows, [$boss, $zone, $done];
    }
    return ($complete, \@rows);
}

sub CheckGoDComplete {
    my ($complete) = GetGoDProgress();
    return $complete;
}

sub GoDEnabled {
    return (defined &quest::is_gates_of_discord_enabled && quest::is_gates_of_discord_enabled()) ? 1 : 0;
}

sub EVENT_SAY {
    if ($text =~ /hail/i) {
        my $account_id = $client->AccountID();
        my $char_id = $client->CharacterID();
        
        # Check account flags for Classic (backward compatibility)
        my $has_classic_acct = quest::get_data("classic_complete_" . $account_id);
        # Check character flags for Kunark (new system)
        my $has_kunark_char = CheckKunarkComplete($char_id);
        # Check character flags for Velious
        my $has_velious_char = CheckVeliousComplete($char_id);
        # Check character flags for Luclin
        my $has_luclin_char = CheckLuclinComplete($char_id);
        # Check character flags for Planes of Power
        my $has_pop_char = CheckPoPComplete($char_id);
        
        # Check character awarded flags
        my $classic_awarded = quest::get_data("classic_awarded_" . $char_id);
        my $kunark_awarded = quest::get_data("kunark_awarded_" . $char_id);
        my $velious_awarded = quest::get_data("velious_awarded_" . $char_id);
        my $luclin_awarded = quest::get_data("luclin_awarded_" . $char_id);
        my $pop_awarded = quest::get_data("pop_awarded_" . $char_id);
        
        my $classic_link = quest::saylink("classic", 1, "Classic");
        my $kunark_link = quest::saylink("kunark", 1, "Kunark");
        my $velious_link = quest::saylink("velious", 1, "Velious");
        my $luclin_link = quest::saylink("luclin", 1, "Luclin");
        my $pop_link = quest::saylink("pop", 1, "Planes of Power");
        my $god_enabled = GoDEnabled();
        my $has_god_char = $god_enabled ? CheckGoDComplete() : 0;
        my $god_awarded = quest::get_data("god_awarded_" . $char_id);

        if ($god_enabled) {
            my $god_link = quest::saylink("gates of discord", 1, "Gates of Discord");
            plugin::Whisper("Greetings, $name. I am Aurelian Stoneward, keeper of legendary achievements. I can help you claim rewards for completing $classic_link, $kunark_link, $velious_link, $luclin_link, $pop_link, or $god_link content.");
        }
        else {
            plugin::Whisper("Greetings, $name. I am Aurelian Stoneward, keeper of legendary achievements. I can help you claim rewards for completing $classic_link, $kunark_link, $velious_link, $luclin_link, or $pop_link content.");
        }
        
        # Show status
        if ($has_classic_acct && !$classic_awarded) {
            plugin::Whisper("I see you have completed the Classic era! You have rewards waiting.");
        }
        if ($has_kunark_char && !$kunark_awarded) {
            plugin::Whisper("I see you have completed the Kunark era! You have rewards waiting.");
        }
        if ($has_velious_char && !$velious_awarded) {
            plugin::Whisper("I see you have completed the Velious era! You have rewards waiting.");
        }
        if ($has_luclin_char && !$luclin_awarded) {
            plugin::Whisper("I see you have completed the Luclin era! You have rewards waiting.");
        }
        if ($has_pop_char && !$pop_awarded) {
            plugin::Whisper("I see you have completed the Planes of Power era! You have rewards waiting.");
        }
        if ($has_god_char && !$god_awarded) {
            plugin::Whisper("I see you have completed the Gates of Discord era! You have rewards waiting.");
        }

        # Auto-grant era completion titlesets (idempotent). Handles both brand-new
        # completers and retroactive grants for players who finished before this existed.
        GrantCompletionTitles($char_id);
    }
    elsif ($text =~ /classic/i) {
        HandleClassicRequest();
    }
    elsif ($text =~ /kunark/i) {
        HandleKunarkRequest();
    }
    elsif ($text =~ /velious/i) {
        HandleVeliousRequest();
    }
    elsif ($text =~ /luclin/i) {
        HandleLuclinRequest();
    }
    elsif ($text =~ /pop/i) {
        HandlePoPRequest();
    }
    elsif ($text =~ /gates of discord|\bgod\b/i && GoDEnabled()) {
        HandleGoDRequest();
    }
}

sub CheckKunarkComplete {
    my ($char_id) = @_;
    my $account_id = $client->AccountID();
    my $char_name  = $client->GetCleanName();
 
    my $has_talendor   = quest::get_data("kunark_talendor_"   . $account_id);
    my $has_severilous = quest::get_data("kunark_severilous_" . $account_id);
    my $has_trakanon   = quest::get_data("kunark_trakanon_"   . $account_id);
 
    return (
        $has_talendor   && $has_talendor   eq $char_name &&
        $has_severilous && $has_severilous eq $char_name &&
        $has_trakanon   && $has_trakanon   eq $char_name
    ) ? 1 : 0;
}

sub CheckVeliousComplete {
    my ($char_id) = @_;
    my $account_id = $client->AccountID();
    my $char_name  = $client->GetCleanName();

    my $has_wuoshi     = quest::get_data("velious_wuoshi_"          . $account_id);
    my $has_kelorekdar = quest::get_data("velious_kelorekdar_"      . $account_id);
    my $has_klandicar  = quest::get_data("velious_klandicar_"       . $account_id);
    my $has_zlandicar  = quest::get_data("velious_zlandicar_"       . $account_id);
    my $has_statue     = quest::get_data("velious_statue_rallos_zek_" . $account_id);

    return (
        $has_wuoshi     && $has_wuoshi     eq $char_name &&
        $has_kelorekdar && $has_kelorekdar eq $char_name &&
        $has_klandicar  && $has_klandicar  eq $char_name &&
        $has_zlandicar  && $has_zlandicar  eq $char_name &&
        $has_statue     && $has_statue     eq $char_name
    ) ? 1 : 0;
}

sub CheckLuclinComplete {
    my ($char_id) = @_;
    my $account_id = $client->AccountID();
    my $char_name  = $client->GetCleanName();

    my $has_grieg     = quest::get_data("luclin_griegveneficus_" . $account_id);
    my $has_seru      = quest::get_data("luclin_seru_"           . $account_id);
    my $has_overfiend = quest::get_data("luclin_thoughthorror_"  . $account_id);
    my $has_crawler   = quest::get_data("luclin_insanitycrawler_" . $account_id);
    my $has_zelnithak  = quest::get_data("luclin_zelnithak_"      . $account_id);
    my $has_highpriest = quest::get_data("luclin_highpriestssra_" . $account_id);

    return (
        $has_grieg      && $has_grieg      eq $char_name &&
        $has_seru       && $has_seru       eq $char_name &&
        $has_overfiend  && $has_overfiend  eq $char_name &&
        $has_crawler    && $has_crawler    eq $char_name &&
        $has_zelnithak  && $has_zelnithak  eq $char_name &&
        $has_highpriest && $has_highpriest eq $char_name
    ) ? 1 : 0;
}

sub CheckPoPComplete {
    my ($char_id) = @_;
    my $account_id = $client->AccountID();
    my $char_name  = $client->GetCleanName();

    my $has_grummus  = quest::get_data("pop_grummus_"          . $account_id);
    my $has_terris   = quest::get_data("pop_terristhule_"      . $account_id);
    my $has_manaetic = quest::get_data("pop_manaeticbehemoth_" . $account_id);
    my $has_aerindar = quest::get_data("pop_aerindar_"         . $account_id);
    my $has_bertox   = quest::get_data("pop_bertoxxulous_"     . $account_id);
    my $has_saryrn   = quest::get_data("pop_saryrn_"           . $account_id);

    return (
        $has_grummus  && $has_grummus  eq $char_name &&
        $has_terris   && $has_terris   eq $char_name &&
        $has_manaetic && $has_manaetic eq $char_name &&
        $has_aerindar && $has_aerindar eq $char_name &&
        $has_bertox   && $has_bertox   eq $char_name &&
        $has_saryrn   && $has_saryrn   eq $char_name
    ) ? 1 : 0;
}

# LDoN "completion" = maxed the augment power in every theme.
# Max power values: Guk 7, Miragul 7, Mistmoore 8, Rujarkian 8, Takish 8.
# These are character-wide quest globals (requires Aurelian npc_types.qglobal = 1).
sub CheckLDoNComplete {
    return (
        defined($qglobals{GUKpower}) && $qglobals{GUKpower} >= 7 &&
        defined($qglobals{MIRpower}) && $qglobals{MIRpower} >= 7 &&
        defined($qglobals{MMCpower}) && $qglobals{MMCpower} >= 8 &&
        defined($qglobals{RUJpower}) && $qglobals{RUJpower} >= 8 &&
        defined($qglobals{TAKpower}) && $qglobals{TAKpower} >= 8
    ) ? 1 : 0;
}

# Idempotently grants the completion titleset for each finished era/content.
# A per-character data flag prevents repeating the announcement on every hail;
# enabletitle() itself is already safe to call repeatedly.
sub GrantCompletionTitles {
    my ($char_id) = @_;

    if (CheckLuclinComplete($char_id) && !quest::get_data("title_luclin_419_" . $char_id)) {
        quest::enabletitle(419); # the Lunar Ascendant
        quest::set_data("title_luclin_419_" . $char_id, "1");
        $client->Message(15, "You have been awarded the title 'the Lunar Ascendant' for completing the Luclin era!");
    }

    if (CheckLDoNComplete() && !quest::get_data("title_ldon_423_" . $char_id)) {
        quest::enabletitle(423); # the Wayfarer's Champion
        quest::set_data("title_ldon_423_" . $char_id, "1");
        $client->Message(15, "You have been awarded the title 'the Wayfarer\x{2019}s Champion' for mastering the Lost Dungeons of Norrath!");
    }

    if (CheckPoPComplete($char_id) && !quest::get_data("title_pop_421_" . $char_id)) {
        quest::enabletitle(421); # the Godbreaker
        quest::set_data("title_pop_421_" . $char_id, "1");
        $client->Message(15, "You have been awarded the title 'the Godbreaker' for conquering the Planes of Power!");
    }

    if (GoDEnabled() && CheckGoDComplete() && !quest::get_data("title_god_427_" . $char_id)) {
        quest::enabletitle(427); # the Discordant (+ joke title in set 427)
        quest::set_data("title_god_427_" . $char_id, "1");
        $client->Message(15, "You have been awarded the title 'the Discordant' for conquering the Gates of Discord!");
    }
}

sub HandleGoDRequest {
    my $char_id = $client->CharacterID();
    my ($is_complete, $rows) = GetGoDProgress();
    my $already_awarded = quest::get_data("god_awarded_" . $char_id);

    my $popup_text = "<c \"#FFD700\"><b>Gates of Discord Era Completion Status</b></c><br><br>"
                   . "<c \"#FFFFFF\">To complete the Gates of Discord era, you must defeat:</c><br><br>";
    foreach my $r (@$rows) {
        my ($boss, $zone, $done) = @$r;
        my $status = $done ? "<c \"#00FF00\">DEFEATED</c>" : "<c \"#FF0000\">Not Defeated</c>";
        $popup_text .= "<c \"#00FFFF\">$boss</c> ($zone): $status<br>";
    }
    $popup_text .= "<br>";

    if ($already_awarded) {
        my $simple_popup = "<c \"#FFD700\"><b>Gates of Discord Era Status</b></c><br><br>"
                         . "<c \"#FFFFFF\">You have already claimed your Gates of Discord rewards on this character!</c>";
        $client->Popup2("Gates of Discord Era Status", $simple_popup, 0, 0, 0, 0);
        return;
    }
    elsif ($is_complete) {
        $popup_text .= "<c \"#00FF00\"><b>Congratulations!</b></c><br>"
                    . "You have broken the Muramite advance on Taelosia!<br><br>"
                    . "<c \"#FFD700\">Rewards:</c><br>"
                    . "&bull; The title <c \"#FFD700\">the Discordant</c><br>"
                    . "&bull; Charm of the Sixth Age (charm upgrade)<br><br>"
                    . "<c \"#FFAA00\">After claiming, hand me your Charm of the Fifth Age to upgrade it to the Charm of the Sixth Age!</c><br><br>"
                    . "<c \"#FFFFFF\">Click OK to claim your rewards!</c>";
        $client->Popup2("Gates of Discord Era Completion", $popup_text, 5011, 5012, 2, 0, "Claim Rewards", "Cancel");
    }
    else {
        $popup_text .= "<c \"#FF9900\">You must defeat all six bosses to complete the Gates of Discord era.</c><br>"
                    . "Return to me when you have accomplished this feat!";
        $client->Popup2("Gates of Discord Era Completion", $popup_text, 0, 0, 0, 0);
    }
}

sub HandleLuclinRequest {
    my $char_id    = $client->CharacterID();
    my $account_id = $client->AccountID();
    my $char_name  = $client->GetCleanName();

    my $has_grieg     = quest::get_data("luclin_griegveneficus_" . $account_id);
    my $has_seru      = quest::get_data("luclin_seru_"           . $account_id);
    my $has_overfiend = quest::get_data("luclin_thoughthorror_"  . $account_id);
    my $has_crawler   = quest::get_data("luclin_insanitycrawler_" . $account_id);
    my $has_zelnithak  = quest::get_data("luclin_zelnithak_"      . $account_id);
    my $has_highpriest = quest::get_data("luclin_highpriestssra_" . $account_id);

    $has_grieg      = ($has_grieg      && $has_grieg      eq $char_name) ? 1 : 0;
    $has_seru       = ($has_seru       && $has_seru       eq $char_name) ? 1 : 0;
    $has_overfiend  = ($has_overfiend  && $has_overfiend  eq $char_name) ? 1 : 0;
    $has_crawler    = ($has_crawler    && $has_crawler    eq $char_name) ? 1 : 0;
    $has_zelnithak  = ($has_zelnithak  && $has_zelnithak  eq $char_name) ? 1 : 0;
    $has_highpriest = ($has_highpriest && $has_highpriest eq $char_name) ? 1 : 0;

    my $is_complete = ($has_grieg && $has_seru && $has_overfiend && $has_crawler && $has_zelnithak && $has_highpriest) ? 1 : 0;

    # Check if already awarded to this character
    my $already_awarded = quest::get_data("luclin_awarded_" . $char_id);

    # Build status popup
    my $grieg_status     = $has_grieg     ? "<c \"#00FF00\">DEFEATED</c>" : "<c \"#FF0000\">Not Defeated</c>";
    my $seru_status      = $has_seru      ? "<c \"#00FF00\">DEFEATED</c>" : "<c \"#FF0000\">Not Defeated</c>";
    my $overfiend_status = $has_overfiend ? "<c \"#00FF00\">DEFEATED</c>" : "<c \"#FF0000\">Not Defeated</c>";
    my $crawler_status   = $has_crawler   ? "<c \"#00FF00\">DEFEATED</c>" : "<c \"#FF0000\">Not Defeated</c>";
    my $zelnithak_status  = $has_zelnithak  ? "<c \"#00FF00\">DEFEATED</c>" : "<c \"#FF0000\">Not Defeated</c>";
    my $highpriest_status = $has_highpriest ? "<c \"#00FF00\">DEFEATED</c>" : "<c \"#FF0000\">Not Defeated</c>";

    my $popup_text = "<c \"#FFD700\"><b>Luclin Era Completion Status</b></c><br><br>"
                   . "<c \"#FFFFFF\">To complete the Luclin era, you must defeat all six bosses:</c><br><br>"
                   . "<c \"#00FFFF\">Grieg Veneficus</c> (Grieg's End): $grieg_status<br>"
                   . "<c \"#00FFFF\">Lord Inquisitor Seru</c> (Sanctus Seru): $seru_status<br>"
                   . "<c \"#00FFFF\">High Priest of Ssraeshza</c> (Ssra Temple): $highpriest_status<br>"
                   . "<c \"#00FFFF\">Thought Horror Overfiend</c> (The Deep): $overfiend_status<br>"
                   . "<c \"#00FFFF\">The Insanity Crawler</c> (Akheva Ruins): $crawler_status<br>"
                   . "<c \"#00FFFF\">Zelnithak</c> (Umbral Plains): $zelnithak_status<br><br>";

    if ($already_awarded) {
        my $simple_popup = "<c \"#FFD700\"><b>Luclin Era Status</b></c><br><br>"
                         . "<c \"#FFFFFF\">You have already claimed your Luclin rewards on this character!</c>";
        $client->Popup2("Luclin Era Status", $simple_popup, 0, 0, 0, 0);
        return;
    }
    elsif ($is_complete) {
        $popup_text .= "<c \"#00FF00\"><b>Congratulations!</b></c><br>"
                    . "You have defeated all six Luclin bosses!<br><br>"
                    . "<c \"#FFD700\">Rewards:</c><br>"
                    . "• Special reward item<br>"
                    . "• New title<br><br>"
                    . "<c \"#FFAA00\">After claiming, hand me your Charm of the Third Age to upgrade it to the Charm of the Fourth Age!</c><br><br>"
                    . "<c \"#FFFFFF\">Click OK to claim your rewards!</c>";

        $client->Popup2("Luclin Era Completion", $popup_text, 5007, 5008, 2, 0, "Claim Rewards", "Cancel");
    }
    else {
        $popup_text .= "<c \"#FF9900\">You must defeat all six bosses to complete the Luclin era.</c><br>"
                    . "Return to me when you have accomplished this feat!";

        $client->Popup2("Luclin Era Completion", $popup_text, 0, 0, 0, 0);
    }
}

sub HandlePoPRequest {
    my $char_id    = $client->CharacterID();
    my $account_id = $client->AccountID();
    my $char_name  = $client->GetCleanName();

    my $has_grummus  = quest::get_data("pop_grummus_"          . $account_id);
    my $has_terris   = quest::get_data("pop_terristhule_"      . $account_id);
    my $has_manaetic = quest::get_data("pop_manaeticbehemoth_" . $account_id);
    my $has_aerindar = quest::get_data("pop_aerindar_"         . $account_id);
    my $has_bertox   = quest::get_data("pop_bertoxxulous_"     . $account_id);
    my $has_saryrn   = quest::get_data("pop_saryrn_"           . $account_id);

    $has_grummus  = ($has_grummus  && $has_grummus  eq $char_name) ? 1 : 0;
    $has_terris   = ($has_terris   && $has_terris   eq $char_name) ? 1 : 0;
    $has_manaetic = ($has_manaetic && $has_manaetic eq $char_name) ? 1 : 0;
    $has_aerindar = ($has_aerindar && $has_aerindar eq $char_name) ? 1 : 0;
    $has_bertox   = ($has_bertox   && $has_bertox   eq $char_name) ? 1 : 0;
    $has_saryrn   = ($has_saryrn   && $has_saryrn   eq $char_name) ? 1 : 0;

    my $is_complete = ($has_grummus && $has_terris && $has_manaetic && $has_aerindar && $has_bertox && $has_saryrn) ? 1 : 0;

    my $already_awarded = quest::get_data("pop_awarded_" . $char_id);

    my $grummus_status  = $has_grummus  ? "<c \"#00FF00\">DEFEATED</c>" : "<c \"#FF0000\">Not Defeated</c>";
    my $terris_status   = $has_terris   ? "<c \"#00FF00\">DEFEATED</c>" : "<c \"#FF0000\">Not Defeated</c>";
    my $manaetic_status = $has_manaetic ? "<c \"#00FF00\">DEFEATED</c>" : "<c \"#FF0000\">Not Defeated</c>";
    my $aerindar_status = $has_aerindar ? "<c \"#00FF00\">DEFEATED</c>" : "<c \"#FF0000\">Not Defeated</c>";
    my $bertox_status   = $has_bertox   ? "<c \"#00FF00\">DEFEATED</c>" : "<c \"#FF0000\">Not Defeated</c>";
    my $saryrn_status   = $has_saryrn   ? "<c \"#00FF00\">DEFEATED</c>" : "<c \"#FF0000\">Not Defeated</c>";

    my $popup_text = "<c \"#FFD700\"><b>Planes of Power Era Completion Status</b></c><br><br>"
                   . "<c \"#FFFFFF\">To complete the Planes of Power era, you must defeat the Tier 1 and Tier 2 gods:</c><br><br>"
                   . "<c \"#00FFFF\">Grummus</c> (Plane of Disease): $grummus_status<br>"
                   . "<c \"#00FFFF\">Terris-Thule</c> (Plane of Nightmare): $terris_status<br>"
                   . "<c \"#00FFFF\">Manaetic Behemoth</c> (Plane of Innovation): $manaetic_status<br>"
                   . "<c \"#00FFFF\">Aerin`Dar</c> (Plane of Valor): $aerindar_status<br>"
                   . "<c \"#00FFFF\">Bertoxxulous</c> (Crypt of Decay): $bertox_status<br>"
                   . "<c \"#00FFFF\">Saryrn</c> (Plane of Torment): $saryrn_status<br><br>";

    if ($already_awarded) {
        my $wands_granted = GrantPoPCompletionWands($char_id);
        if ($wands_granted) {
            plugin::Whisper("Your Planes of Power polymorph wand rewards were missing from the old claim. I have granted them now.");
            return;
        }

        my $simple_popup = "<c \"#FFD700\"><b>Planes of Power Era Status</b></c><br><br>"
                         . "<c \"#FFFFFF\">You have already claimed your Planes of Power rewards on this character!</c>";
        $client->Popup2("Planes of Power Era Status", $simple_popup, 0, 0, 0, 0);
        return;
    }
    elsif ($is_complete) {
        $popup_text .= "<c \"#00FF00\"><b>Congratulations!</b></c><br>"
                    . "You have cast down the gods of the Planes of Power!<br><br>"
                    . "<c \"#FFD700\">Rewards:</c><br>"
                    . "&bull; The title <c \"#FFD700\">the Godbreaker</c><br>"
                    . "&bull; Polymorph Wand: Bertoxxulous<br>"
                    . "&bull; Polymorph Wand: Saryrn<br>"
                    . "&bull; Polymorph Wand: Vallon<br>"
                    . "&bull; Charm of the Fifth Age (charm upgrade)<br><br>"
                    . "<c \"#FFAA00\">After claiming, hand me your Charm of the Fourth Age to upgrade it to the Charm of the Fifth Age!</c><br><br>"
                    . "<c \"#FFFFFF\">Click OK to claim your rewards!</c>";

        $client->Popup2("Planes of Power Era Completion", $popup_text, 5009, 5010, 2, 0, "Claim Rewards", "Cancel");
    }
    else {
        $popup_text .= "<c \"#FF9900\">You must defeat all six Tier 1 and Tier 2 bosses to complete the Planes of Power era.</c><br>"
                    . "Return to me when you have accomplished this feat!";

        $client->Popup2("Planes of Power Era Completion", $popup_text, 0, 0, 0, 0);
    }
}

sub HandleVeliousRequest {
    my $char_id    = $client->CharacterID();
    my $account_id = $client->AccountID();
    my $char_name  = $client->GetCleanName();

    my $has_wuoshi     = quest::get_data("velious_wuoshi_"          . $account_id);
    my $has_kelorekdar = quest::get_data("velious_kelorekdar_"      . $account_id);
    my $has_klandicar  = quest::get_data("velious_klandicar_"       . $account_id);
    my $has_zlandicar  = quest::get_data("velious_zlandicar_"       . $account_id);
    my $has_statue     = quest::get_data("velious_statue_rallos_zek_" . $account_id);

    $has_wuoshi     = ($has_wuoshi     && $has_wuoshi     eq $char_name) ? 1 : 0;
    $has_kelorekdar = ($has_kelorekdar && $has_kelorekdar eq $char_name) ? 1 : 0;
    $has_klandicar  = ($has_klandicar  && $has_klandicar  eq $char_name) ? 1 : 0;
    $has_zlandicar  = ($has_zlandicar  && $has_zlandicar  eq $char_name) ? 1 : 0;
    $has_statue     = ($has_statue     && $has_statue     eq $char_name) ? 1 : 0;

    my $is_complete = ($has_wuoshi && $has_kelorekdar && $has_klandicar && $has_zlandicar && $has_statue) ? 1 : 0;

    # Check if already awarded to this character
    my $already_awarded = quest::get_data("velious_awarded_" . $char_id);

    # Build status popup
    my $wuoshi_status     = $has_wuoshi     ? "<c \"#00FF00\">DEFEATED</c>" : "<c \"#FF0000\">Not Defeated</c>";
    my $kelorekdar_status = $has_kelorekdar ? "<c \"#00FF00\">DEFEATED</c>" : "<c \"#FF0000\">Not Defeated</c>";
    my $klandicar_status  = $has_klandicar  ? "<c \"#00FF00\">DEFEATED</c>" : "<c \"#FF0000\">Not Defeated</c>";
    my $zlandicar_status  = $has_zlandicar  ? "<c \"#00FF00\">DEFEATED</c>" : "<c \"#FF0000\">Not Defeated</c>";
    my $statue_status     = $has_statue     ? "<c \"#00FF00\">DEFEATED</c>" : "<c \"#FF0000\">Not Defeated</c>";

    my $popup_text = "<c \"#FFD700\"><b>Velious Era Completion Status</b></c><br><br>"
                   . "<c \"#FFFFFF\">To complete the Velious era, you must defeat all five raid bosses:</c><br><br>"
                   . "<c \"#00FFFF\">Wuoshi</c> (Wakening Lands): $wuoshi_status<br>"
                   . "<c \"#00FFFF\">Kelorek`Dar</c> (Cobalt Scar): $kelorekdar_status<br>"
                   . "<c \"#00FFFF\">Klandicar</c> (Western Wastes): $klandicar_status<br>"
                   . "<c \"#00FFFF\">Zlandicar</c> (Dragon Necropolis): $zlandicar_status<br>"
                   . "<c \"#00FFFF\">The Statue of Rallos Zek</c> (Kael Drakkel): $statue_status<br><br>";

    if ($already_awarded) {
        my $simple_popup = "<c \"#FFD700\"><b>Velious Era Status</b></c><br><br>"
                         . "<c \"#FFFFFF\">You have already claimed your Velious rewards on this character!</c>";
        $client->Popup2("Velious Era Status", $simple_popup, 0, 0, 0, 0);
        return;
    }
    elsif ($is_complete) {
        $popup_text .= "<c \"#00FF00\"><b>Congratulations!</b></c><br>"
                    . "You have defeated all five Velious raid bosses!<br><br>"
                    . "<c \"#FFD700\">Rewards:</c><br>"
                    . "• Special reward item<br>"
                    . "• New title<br><br>"
                    . "<c \"#FFAA00\">After claiming, hand me your Charm of the Second Age to upgrade it to the Charm of the Third Age!</c><br><br>"
                    . "<c \"#FFFFFF\">Click OK to claim your rewards!</c>";

        $client->Popup2("Velious Era Completion", $popup_text, 5005, 5006, 2, 0, "Claim Rewards", "Cancel");
    }
    else {
        $popup_text .= "<c \"#FF9900\">You must defeat all five bosses to complete the Velious era.</c><br>"
                    . "Return to me when you have accomplished this feat!";

        $client->Popup2("Velious Era Completion", $popup_text, 0, 0, 0, 0);
    }
}

sub HandleClassicRequest {
    my $account_id = $client->AccountID();
    my $char_id = $client->CharacterID();
    
    # Check account completion
    my $has_nagafen = quest::get_data("classic_nagafen_" . $account_id);
    my $has_vox = quest::get_data("classic_vox_" . $account_id);
    my $is_complete = ($has_nagafen && $has_vox) ? 1 : 0;
    
    # Check if already awarded to this character
    my $already_awarded = quest::get_data("classic_awarded_" . $char_id);
    
    # Build status popup
    my $nagafen_status = $has_nagafen ? "<c \"#00FF00\">DEFEATED</c>" : "<c \"#FF0000\">Not Defeated</c>";
    my $vox_status     = $has_vox     ? "<c \"#00FF00\">DEFEATED</c>" : "<c \"#FF0000\">Not Defeated</c>";
    
    my $popup_text = "<c \"#FFD700\"><b>Classic Era Completion Status</b></c><br><br>"
                   . "<c \"#FFFFFF\">To complete the Classic era, you must defeat both ancient dragons:</c><br><br>"
                   . "<c \"#00FFFF\">Lord Nagafen</c> (Solusek's Eye): $nagafen_status<br>"
                   . "<c \"#00FFFF\">Lady Vox</c> (Permafrost): $vox_status<br><br>";
    
    if ($already_awarded) {
        my $simple_popup = "<c \"#FFD700\"><b>Classic Era Status</b></c><br><br>"
                         . "<c \"#FFFFFF\">You have already claimed your Classic rewards on this character!</c>";
        $client->Popup2("Classic Era Status", $simple_popup, 0, 0, 0, 0);
        return;
    }
    elsif ($is_complete) {
        $popup_text .= "<c \"#00FF00\"><b>Congratulations!</b></c><br>"
                    . "You have defeated both ancient dragons!<br><br>"
                    . "<c \"#FFD700\">Rewards:</c><br>"
                    . "• Charm of the First Age<br>"
                    . "• Special reward item<br>"
                    . "• Three new titles<br><br>"
                    . "<c \"#FFFFFF\">Click OK to claim your rewards!</c>";
        
        $client->Popup2("Classic Era Completion", $popup_text, 5001, 5002, 2, 0, "Claim Rewards", "Cancel");
    }
    else {
        $popup_text .= "<c \"#FF9900\">You must defeat both dragons to complete the Classic era.</c><br>"
                    . "Return to me when you have accomplished this feat!";
        
        $client->Popup2("Classic Era Completion", $popup_text, 0, 0, 0, 0);
    }
}


sub HandleKunarkRequest {
    my $char_id    = $client->CharacterID();
    my $account_id = $client->AccountID();
    my $char_name  = $client->GetCleanName();

    my $has_talendor   = quest::get_data("kunark_talendor_"   . $account_id);
    my $has_severilous = quest::get_data("kunark_severilous_" . $account_id);
    my $has_trakanon   = quest::get_data("kunark_trakanon_"   . $account_id);

    $has_talendor   = ($has_talendor   && $has_talendor   eq $char_name) ? 1 : 0;
    $has_severilous = ($has_severilous && $has_severilous eq $char_name) ? 1 : 0;
    $has_trakanon   = ($has_trakanon   && $has_trakanon   eq $char_name) ? 1 : 0;

    my $is_complete = ($has_talendor && $has_severilous && $has_trakanon) ? 1 : 0;

    # Check if already awarded to this character
    my $already_awarded = quest::get_data("kunark_awarded_" . $char_id);
    
    # Build status popup
    my $talendor_status   = $has_talendor   ? "<c \"#00FF00\">DEFEATED</c>"     : "<c \"#FF0000\">Not Defeated</c>";
    my $severilous_status = $has_severilous ? "<c \"#00FF00\">DEFEATED</c>"     : "<c \"#FF0000\">Not Defeated</c>";
    my $trakanon_status   = $has_trakanon   ? "<c \"#00FF00\">DEFEATED</c>"     : "<c \"#FF0000\">Not Defeated</c>";
    
    my $popup_text = "<c \"#FFD700\"><b>Kunark Era Completion Status</b></c><br><br>"
                   . "<c \"#FFFFFF\">To complete the Kunark era, you must defeat all three ancient dragons:</c><br><br>"
                   . "<c \"#00FFFF\">Talendor</c>: $talendor_status<br>"
                   . "<c \"#00FFFF\">Severilous</c>: $severilous_status<br>"
                   . "<c \"#00FFFF\">Trakanon</c>: $trakanon_status<br><br>";
    
    if ($already_awarded) {
        my $simple_popup = "<c \"#FFD700\"><b>Kunark Era Status</b></c><br><br>"
                         . "<c \"#FFFFFF\">You have already claimed your Kunark rewards on this character!</c>";
        $client->Popup2("Kunark Era Status", $simple_popup, 0, 0, 0, 0);
        return;
    }
    elsif ($is_complete) {
        $popup_text .= "<c \"#00FF00\"><b>Congratulations!</b></c><br>"
                    . "You have defeated all three Kunark dragons!<br><br>"
                    . "<c \"#FFD700\">Rewards:</c><br>"
                    . "• Special reward item<br>"
                    . "• Two new titles<br><br>"
                    . "<c \"#FFAA00\">After claiming, hand me your Charm of the First Age to upgrade it to the Charm of the Second Age!</c><br><br>"
                    . "<c \"#FFFFFF\">Click OK to claim your rewards!</c>";
        
        $client->Popup2("Kunark Era Completion", $popup_text, 5003, 5004, 2, 0, "Claim Rewards", "Cancel");
    }
    else {
        $popup_text .= "<c \"#FF9900\">You must defeat all three dragons to complete the Kunark era.</c><br>"
                    . "Return to me when you have accomplished this feat!";
        
        $client->Popup2("Kunark Era Completion", $popup_text, 0, 0, 0, 0);
    }
}

sub EVENT_ITEM {
    # Reject charm turn-ins that have augments — player must remove augs first
    my @charm_ids = (2827, 2855, 2854, 2829, 4038);
    for my $slot (1..4) {
        my $inst = plugin::val("item${slot}_inst");
        next unless $inst;
        my $traded_id = plugin::val("item${slot}");
        next unless $traded_id && grep { $_ == $traded_id } @charm_ids;
        for my $aug (0..5) {
            if ($inst->GetAugmentItemID($aug) && $inst->GetAugmentItemID($aug) > 0) {
                plugin::Whisper("I cannot accept a charm that has augments in it! Please remove all augments before handing me the charm.");
                $client->SummonItem($traded_id);
                return;
            }
        }
    }

    if (plugin::check_handin(\%itemcount, 4038 => 1)) {
        my $char_id = $client->CharacterID();
        my $god_awarded = quest::get_data("god_awarded_" . $char_id);

        if ($god_awarded) {
            # They've completed Gates of Discord, upgrade their charm
            $client->SummonItem(4042);
            plugin::Whisper("Your charm has been upgraded! This Charm of the Sixth Age bears the scars of the war against Mata Muram.");
            $client->Message(15, "You received: Charm of the Sixth Age");
        }
        else {
            plugin::Whisper("You have not yet claimed your Gates of Discord rewards. Please click 'Claim Rewards' first, then hand me the charm.");
            $client->SummonItem(4038);
        }
    }
    elsif (plugin::check_handin(\%itemcount, 2829 => 1)) {
        my $char_id = $client->CharacterID();
        my $pop_awarded = quest::get_data("pop_awarded_" . $char_id);

        if ($pop_awarded) {
            # They've completed Planes of Power, upgrade their charm
            $client->SummonItem(4038);
            plugin::Whisper("Your charm has been upgraded! This transcendent Charm of the Fifth Age reflects your conquest of the Planes of Power.");
            $client->Message(15, "You received: Charm of the Fifth Age");
        }
        else {
            # They haven't completed Planes of Power yet — return the charm directly
            plugin::Whisper("You have not yet claimed your Planes of Power rewards. Please click 'Claim Rewards' first, then hand me the charm.");
            $client->SummonItem(2829);
        }
    }
    elsif (plugin::check_handin(\%itemcount, 2827 => 1)) {
        my $char_id = $client->CharacterID();
        my $luclin_awarded = quest::get_data("luclin_awarded_" . $char_id);
        
        if ($luclin_awarded) {
            # They've completed Luclin, upgrade their charm
            $client->SummonItem(2829);
            plugin::Whisper("Your charm has been upgraded! This legendary Charm of the Fourth Age reflects your mastery over the shadows of Luclin.");
            $client->Message(15, "You received: Charm of the Fourth Age");
        }
        else {
            # They haven't completed Luclin yet — return the charm directly
            plugin::Whisper("You have not yet claimed your Luclin rewards. Please click 'Claim Rewards' first, then hand me the charm.");
            $client->SummonItem(2827);
        }
    }
    elsif (plugin::check_handin(\%itemcount, 2855 => 1)) {
        my $char_id = $client->CharacterID();
        my $velious_awarded = quest::get_data("velious_awarded_" . $char_id);
        
        if ($velious_awarded) {
            # They've completed Velious, upgrade their charm
            $client->SummonItem(2827);
            plugin::Whisper("Your charm has been upgraded! This mighty Charm of the Third Age reflects your mastery over the frozen continent.");
            $client->Message(15, "You received: Charm of the Third Age");
        }
        else {
            # They haven't completed Velious yet — return the charm directly
            # (check_handin already consumed it from %itemcount so return_items won't work)
            plugin::Whisper("You have not yet claimed your Velious rewards. Please click 'Claim Rewards' first, then hand me the charm.");
            $client->SummonItem(2855);
        }
    }
    elsif (plugin::check_handin(\%itemcount, 2854 => 1)) {
        my $char_id = $client->CharacterID();
        my $classic_awarded = quest::get_data("classic_awarded_" . $char_id);
        
        if ($classic_awarded) {
            # They've completed Classic, upgrade their charm
            $client->SummonItem(2855);
            plugin::Whisper("Your charm has been upgraded! This enhanced Charm of the Second Age reflects your mastery over the Classic dragons.");
            $client->Message(15, "You received: Charm of the Second Age");
        }
        else {
            # They haven't completed Classic yet — return the charm directly
            plugin::Whisper("You have not yet claimed your Classic rewards. Please click 'Claim Rewards' first, then hand me the charm.");
            $client->SummonItem(2854);
        }
    }
    else {
        plugin::return_items(\%itemcount);
    }
}

sub EVENT_POPUPRESPONSE {
    if ($popupid == 5001) {
        # Classic rewards claim
        my $account_id = $client->AccountID();
        my $char_id = $client->CharacterID();
        
        # Check if already awarded
        my $already_awarded = quest::get_data("classic_awarded_" . $char_id);
        if ($already_awarded) {
            plugin::Whisper("You have already claimed your Classic rewards!");
            return;
        }
        
        # Verify completion
        my $has_nagafen = quest::get_data("classic_nagafen_" . $account_id);
        my $has_vox = quest::get_data("classic_vox_" . $account_id);
        
        if ($has_nagafen && $has_vox) {
            # Award items and titles
            $client->SummonItem(2854);
            $client->SummonItem(17662);
            
            # Grant titles
            quest::enabletitle(398);
            quest::enabletitle(399);
            quest::enabletitle(400);
            
            # Mark as awarded (permanent)
            quest::set_data("classic_awarded_" . $char_id, $client->GetCleanName());
            
            # World announcement
            my $char_name = $client->GetCleanName();
            quest::we(15, "$char_name has claimed their Classic era rewards for defeating Lord Nagafen and Lady Vox! Congratulations, champion!");
            
            # Messages
            $client->Message(15, "═══════════════════════════════════════════════════");
            $client->Message(15, "Classic Era Rewards Claimed!");
            $client->Message(15, "You received the Charm of the First Age, a special reward, and unlocked three new titles!");
            $client->Message(15, "═══════════════════════════════════════════════════");
            
            plugin::Whisper("Congratulations, $name! Your Classic era achievements have been rewarded. Wear them with pride!");
        }
        else {
            plugin::Whisper("I cannot grant you the rewards. You must defeat both Lord Nagafen and Lady Vox first.");
        }
    }
    elsif ($popupid == 5002) {
        plugin::Whisper("Very well. Return when you are ready to claim your rewards.");
    }
    elsif ($popupid == 5003) {
        # Kunark rewards claim
        my $char_id = $client->CharacterID();
        
        # Check if already awarded
        my $already_awarded = quest::get_data("kunark_awarded_" . $char_id);
        if ($already_awarded) {
            plugin::Whisper("You have already claimed your Kunark rewards!");
            return;
        }
        
        # Verify completion (Kunark uses account_id key, char_name value)
        my $account_id = $client->AccountID();
        my $char_name  = $client->GetCleanName();
        my $has_talendor   = quest::get_data("kunark_talendor_"   . $account_id);
        my $has_severilous = quest::get_data("kunark_severilous_" . $account_id);
        my $has_trakanon   = quest::get_data("kunark_trakanon_"   . $account_id);

        $has_talendor   = ($has_talendor   && $has_talendor   eq $char_name) ? 1 : 0;
        $has_severilous = ($has_severilous && $has_severilous eq $char_name) ? 1 : 0;
        $has_trakanon   = ($has_trakanon   && $has_trakanon   eq $char_name) ? 1 : 0;

        if ($has_talendor && $has_severilous && $has_trakanon) {
            # Award item and titles (no charm yet)
            $client->SummonItem(17663);
            
            # Grant titles
            quest::enabletitle(401);
            quest::enabletitle(402);
            
            # Mark as awarded (permanent)
            quest::set_data("kunark_awarded_" . $char_id, $client->GetCleanName());
            
            # World announcement
            quest::we(15, "$char_name has claimed their Kunark era rewards for defeating Talendor, Severilous, and Trakanon! Congratulations, champion!");
            
            # Messages
            $client->Message(15, "═══════════════════════════════════════════════════");
            $client->Message(15, "Kunark Era Rewards Claimed!");
            $client->Message(15, "You received a special reward and unlocked two new titles!");
            $client->Message(15, "═══════════════════════════════════════════════════");
            
            plugin::Whisper("Congratulations, $name! Your Kunark era achievements have been rewarded. Now hand me your Charm of the First Age and I will upgrade it to the Charm of the Second Age!");
        }
        else {
            plugin::Whisper("I cannot grant you the rewards. You must defeat Talendor, Severilous, and Trakanon first.");
        }
    }
    elsif ($popupid == 5004) {
        plugin::Whisper("Very well. Return when you are ready to claim your rewards.");
    }
    elsif ($popupid == 5005) {
        # Velious rewards claim
        my $char_id = $client->CharacterID();

        # Check if already awarded
        my $already_awarded = quest::get_data("velious_awarded_" . $char_id);
        if ($already_awarded) {
            plugin::Whisper("You have already claimed your Velious rewards!");
            return;
        }

        # Verify completion
        my $account_id = $client->AccountID();
        my $char_name  = $client->GetCleanName();
        my $has_wuoshi     = quest::get_data("velious_wuoshi_"          . $account_id);
        my $has_kelorekdar = quest::get_data("velious_kelorekdar_"      . $account_id);
        my $has_klandicar  = quest::get_data("velious_klandicar_"       . $account_id);
        my $has_zlandicar  = quest::get_data("velious_zlandicar_"       . $account_id);
        my $has_statue     = quest::get_data("velious_statue_rallos_zek_" . $account_id);

        $has_wuoshi     = ($has_wuoshi     && $has_wuoshi     eq $char_name) ? 1 : 0;
        $has_kelorekdar = ($has_kelorekdar && $has_kelorekdar eq $char_name) ? 1 : 0;
        $has_klandicar  = ($has_klandicar  && $has_klandicar  eq $char_name) ? 1 : 0;
        $has_zlandicar  = ($has_zlandicar  && $has_zlandicar  eq $char_name) ? 1 : 0;
        $has_statue     = ($has_statue     && $has_statue     eq $char_name) ? 1 : 0;

        if ($has_wuoshi && $has_kelorekdar && $has_klandicar && $has_zlandicar && $has_statue) {
            # Award items and titles
            $client->SummonItem(17675);

            # Grant titles
            quest::enabletitle(411);

            # Mark as awarded (permanent)
            quest::set_data("velious_awarded_" . $char_id, $client->GetCleanName());

            # World announcement
            quest::we(15, "$char_name has claimed their Velious era rewards for conquering the frozen continent! Congratulations, champion!");

            # Messages
            $client->Message(15, "═══════════════════════════════════════════════════");
            $client->Message(15, "Velious Era Rewards Claimed!");
            $client->Message(15, "You received a special reward and unlocked a new title!");
            $client->Message(15, "═══════════════════════════════════════════════════");

            plugin::Whisper("Congratulations, $name! Your Velious era achievements have been rewarded. Now hand me your Charm of the Second Age and I will upgrade it to the Charm of the Third Age!");
        }
        else {
            plugin::Whisper("I cannot grant you the rewards. You must defeat all five Velious bosses first.");
        }
    }
    elsif ($popupid == 5006) {
        plugin::Whisper("Very well. Return when you are ready to claim your rewards.");
    }
    elsif ($popupid == 5007) {
        # Luclin rewards claim
        my $char_id = $client->CharacterID();

        # Check if already awarded
        my $already_awarded = quest::get_data("luclin_awarded_" . $char_id);
        if ($already_awarded) {
            plugin::Whisper("You have already claimed your Luclin rewards!");
            return;
        }

        # Verify completion
        my $account_id = $client->AccountID();
        my $char_name  = $client->GetCleanName();
        my $has_grieg     = quest::get_data("luclin_griegveneficus_" . $account_id);
        my $has_seru      = quest::get_data("luclin_seru_"           . $account_id);
        my $has_overfiend = quest::get_data("luclin_thoughthorror_"  . $account_id);
        my $has_crawler   = quest::get_data("luclin_insanitycrawler_" . $account_id);
        my $has_zelnithak  = quest::get_data("luclin_zelnithak_"      . $account_id);
        my $has_highpriest = quest::get_data("luclin_highpriestssra_" . $account_id);

        $has_grieg      = ($has_grieg      && $has_grieg      eq $char_name) ? 1 : 0;
        $has_seru       = ($has_seru       && $has_seru       eq $char_name) ? 1 : 0;
        $has_overfiend  = ($has_overfiend  && $has_overfiend  eq $char_name) ? 1 : 0;
        $has_crawler    = ($has_crawler    && $has_crawler    eq $char_name) ? 1 : 0;
        $has_zelnithak  = ($has_zelnithak  && $has_zelnithak  eq $char_name) ? 1 : 0;
        $has_highpriest = ($has_highpriest && $has_highpriest eq $char_name) ? 1 : 0;

        if ($has_grieg && $has_seru && $has_overfiend && $has_crawler && $has_zelnithak && $has_highpriest) {
            # Award items and titles
            $client->SummonItem(2830);

            # Grant titles
            quest::enabletitle(419); # the Lunar Ascendant (Luclin era)
            quest::set_data("title_luclin_419_" . $char_id, "1");

            # Mark as awarded (permanent)
            quest::set_data("luclin_awarded_" . $char_id, $client->GetCleanName());

            # World announcement
            quest::we(15, "$char_name has claimed their Luclin era rewards for conquering the shadows of Luclin! Congratulations, champion!");

            # Messages
            $client->Message(15, "═══════════════════════════════════════════════════");
            $client->Message(15, "Luclin Era Rewards Claimed!");
            $client->Message(15, "You received a special reward and unlocked a new title!");
            $client->Message(15, "═══════════════════════════════════════════════════");

            plugin::Whisper("Congratulations, $name! Your Luclin era achievements have been rewarded. Now hand me your Charm of the Third Age and I will upgrade it to the Charm of the Fourth Age!");
        }
        else {
            plugin::Whisper("I cannot grant you the rewards. You must defeat all six Luclin bosses first.");
        }
    }
    elsif ($popupid == 5008) {
        plugin::Whisper("Very well. Return when you are ready to claim your rewards.");
    }
    elsif ($popupid == 5009) {
        # Planes of Power rewards claim
        my $char_id = $client->CharacterID();

        # Check if already awarded
        my $already_awarded = quest::get_data("pop_awarded_" . $char_id);
        if ($already_awarded) {
            plugin::Whisper("You have already claimed your Planes of Power rewards!");
            return;
        }

        # Verify completion
        my $account_id = $client->AccountID();
        my $char_name  = $client->GetCleanName();
        my $has_grummus  = quest::get_data("pop_grummus_"          . $account_id);
        my $has_terris   = quest::get_data("pop_terristhule_"      . $account_id);
        my $has_manaetic = quest::get_data("pop_manaeticbehemoth_" . $account_id);
        my $has_aerindar = quest::get_data("pop_aerindar_"         . $account_id);
        my $has_bertox   = quest::get_data("pop_bertoxxulous_"     . $account_id);
        my $has_saryrn   = quest::get_data("pop_saryrn_"           . $account_id);

        $has_grummus  = ($has_grummus  && $has_grummus  eq $char_name) ? 1 : 0;
        $has_terris   = ($has_terris   && $has_terris   eq $char_name) ? 1 : 0;
        $has_manaetic = ($has_manaetic && $has_manaetic eq $char_name) ? 1 : 0;
        $has_aerindar = ($has_aerindar && $has_aerindar eq $char_name) ? 1 : 0;
        $has_bertox   = ($has_bertox   && $has_bertox   eq $char_name) ? 1 : 0;
        $has_saryrn   = ($has_saryrn   && $has_saryrn   eq $char_name) ? 1 : 0;

        if ($has_grummus && $has_terris && $has_manaetic && $has_aerindar && $has_bertox && $has_saryrn) {
            # Award completion items
            GrantPoPCompletionWands($char_id);

            # Grant title: the Godbreaker
            quest::enabletitle(421);
            quest::set_data("title_pop_421_" . $char_id, "1");

            # Mark as awarded (permanent)
            quest::set_data("pop_awarded_" . $char_id, $client->GetCleanName());

            # World announcement
            quest::we(15, "$char_name has cast down the gods of the Planes of Power and earned the title of Godbreaker! Congratulations, champion!");

            # Messages
            $client->Message(15, "\x{2550}" x 51);
            $client->Message(15, "Planes of Power Era Rewards Claimed!");
            $client->Message(15, "You received the Planes of Power polymorph wands and unlocked the title 'the Godbreaker'!");
            $client->Message(15, "\x{2550}" x 51);

            plugin::Whisper("Congratulations, $name! You have proven yourself a Godbreaker. Now hand me your Charm of the Fourth Age and I will upgrade it to the Charm of the Fifth Age!");
        }
        else {
            plugin::Whisper("I cannot grant you the rewards. You must defeat all six Tier 1 and Tier 2 Planes of Power bosses first.");
        }
    }
    elsif ($popupid == 5010) {
        plugin::Whisper("Very well. Return when you are ready to claim your rewards.");
    }
    elsif ($popupid == 5011) {
        # Gates of Discord rewards claim
        my $char_id = $client->CharacterID();
        my $char_name = $client->GetCleanName();

        if (quest::get_data("god_awarded_" . $char_id)) {
            plugin::Whisper("You have already claimed your Gates of Discord rewards!");
            return;
        }

        if (GoDEnabled() && CheckGoDComplete()) {
            quest::enabletitle(427); # the Discordant
            quest::set_data("title_god_427_" . $char_id, "1");
            quest::set_data("god_awarded_" . $char_id, $char_name);

            quest::we(15, "$char_name has broken the Muramite war machine and earned the title of the Discordant! Congratulations, champion!");

            $client->Message(15, "\x{2550}" x 51);
            $client->Message(15, "Gates of Discord Era Rewards Claimed!");
            $client->Message(15, "You unlocked the title 'the Discordant'!");
            $client->Message(15, "\x{2550}" x 51);

            plugin::Whisper("Congratulations, $name! Now hand me your Charm of the Fifth Age and I will upgrade it to the Charm of the Sixth Age!");
        }
        else {
            plugin::Whisper("I cannot grant you the rewards. You must defeat all six Gates of Discord bosses first.");
        }
    }
    elsif ($popupid == 5012) {
        plugin::Whisper("Very well. Return when you are ready to claim your rewards.");
    }
}

1;
