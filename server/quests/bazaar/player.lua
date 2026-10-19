-- Bazaar zone player script.
--
-- Blocks the "Mass Mystical Transvergence" (2538) and "Rod of Mystical
-- Transvergence" (3188) spells from being cast in the Bazaar. Both spells
-- summon the Rod of Mystical Transvergence item, which can be exploited via
-- a trader-mode validation bug to knock every trader in the zone offline.
--
-- Until that bug is fixed upstream, any attempt to cast one of these spells
-- in the Bazaar will be cancelled and the caster will explode, with a
-- zone-wide emote announcing the backfire.

local BANNED_SPELLS = {
	[2538] = "Mass Mystical Transvergence",
	[3188] = "Rod of Mystical Transvergence",
}

function event_cast_begin(e)
	local spell_name = BANNED_SPELLS[e.spell]
	if spell_name == nil then
		return 0
	end

	local name = e.self:GetCleanName()

	eq.get_entity_list():Message(
		0,
		15,
		string.format(
			"%s attempted to channel forbidden bazaar magics (%s). The spell backfires violently, reducing them to a smoldering pile of ash!",
			name,
			spell_name
		)
	)

	eq.debug(string.format(
		"Bazaar exploit attempt: %s (char_id %d) tried to cast spell %d (%s); caster killed.",
		name,
		e.self:CharacterID(),
		e.spell,
		spell_name
	))

	e.self:Kill()

	return 1
end
