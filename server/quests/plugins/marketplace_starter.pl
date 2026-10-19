# Marketplace starter errand: once per account, ToB client only.
# Exarch Valeth offers it; Torrick Brightforge, Nyra Silvermark and Kilven the Quartermaster
# are the steps; Valeth pays the coins. Progress is the account bucket below.
#
# Bucket values: (none) not started, 1 see Torrick, 2 see Nyra, 3 see Kilven, 4 back to Valeth, done.
# The bucket picks each NPC's lines and gates the offer and payout; task 4 ("A Shortage of Wares",
# marketplace_starter_task.sql) mirrors it in the task window, activity n = step n + 1.

sub MarketplaceStarterBucket { return "marketplace_starter"; }
sub MarketplaceStarterClient { return 8; } # EQ::versions::ClientVersion::TOB

sub MarketplaceStarterCoins { return 750; }
sub MarketplaceStarterTask { return 4; }

# true when this character is on the ToB client (the only client with the Marketplace window)
sub MarketplaceStarterEligible {
    my $client = plugin::val('$client');
    return $client && $client->GetClientVersion() >= plugin::MarketplaceStarterClient();
}

# "" (not started), 1-4, or "done"
sub MarketplaceStarterStep {
    my $client = plugin::val('$client');
    return $client->GetAccountBucket(plugin::MarketplaceStarterBucket()) // "";
}

sub MarketplaceStarterSetStep {
    my $client = plugin::val('$client');
    $client->SetAccountBucket(plugin::MarketplaceStarterBucket(), $_[0]);
}

# gives this character the task with the steps the account has already done, when it lacks it
# (accepted on another character, or the task was removed)
sub MarketplaceStarterEnsureTask {
    my $client = plugin::val('$client');
    my $step   = plugin::MarketplaceStarterStep();
    my $task   = plugin::MarketplaceStarterTask();
    return unless $step =~ /^[1-4]$/;
    return if $client->IsTaskActive($task);
    $client->AssignTask($task);
    $client->UpdateTaskActivity($task, $_, 1) for 0 .. $step - 2;
}

# moves on from step $from only; returns true if it did, so a repeat hail does nothing
sub MarketplaceStarterAdvance {
    my $from   = $_[0];
    my $client = plugin::val('$client');
    return 0 unless plugin::MarketplaceStarterEligible();
    return 0 unless plugin::MarketplaceStarterStep() eq "$from";
    plugin::MarketplaceStarterEnsureTask();
    plugin::MarketplaceStarterSetStep($from + 1);
    $client->UpdateTaskActivity(plugin::MarketplaceStarterTask(), $from - 1, 1);
    return 1;
}

1;
