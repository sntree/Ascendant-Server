# Ascendant: plugin::ClassType was called by global_player.pl (BiC gemstone combines 10334/10346/10903)
# but never defined, so the script died and players got no gemstone or Gemcutter Tools back.
# Returns 'melee', 'hybrid', 'priest' or 'caster'. Accepts a class name ($class) or class id.
# Groups match the class masks on the gemstone rewards (67653-67668):
#   melee 33089 = WAR MNK ROG BER, hybrid 16540 = PAL RNG SHD BRD BST,
#   priest 546 = CLR DRU SHM, caster 15360 = NEC WIZ MAG ENC

sub ClassType {
    my $class = shift;

    my %by_id = (
        1  => 'melee',  7  => 'melee',  9  => 'melee',  16 => 'melee',
        3  => 'hybrid', 4  => 'hybrid', 5  => 'hybrid', 8  => 'hybrid', 15 => 'hybrid',
        2  => 'priest', 6  => 'priest', 10 => 'priest',
        11 => 'caster', 12 => 'caster', 13 => 'caster', 14 => 'caster',
    );

    my %by_name = (
        'warrior'      => 'melee',  'monk'      => 'melee',  'rogue'   => 'melee',  'berserker' => 'melee',
        'paladin'      => 'hybrid', 'ranger'    => 'hybrid', 'shadowknight' => 'hybrid',
        'shadow knight' => 'hybrid', 'bard'     => 'hybrid', 'beastlord' => 'hybrid',
        'cleric'       => 'priest', 'druid'     => 'priest', 'shaman'  => 'priest',
        'necromancer'  => 'caster', 'wizard'    => 'caster', 'magician' => 'caster', 'enchanter' => 'caster',
    );

    return $by_id{$class} if defined $class && $class =~ /^\d+$/;
    return $by_name{lc($class // '')} // 'melee';
}

1;
