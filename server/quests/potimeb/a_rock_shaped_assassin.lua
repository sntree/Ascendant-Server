local charm_credit = require("potimeb_charm_credit");

--a_rock_shaped_assassin (223147)
--Phase 1 - Terlok of Earth Trial
--potimeb

function event_death_complete(e)
	charm_credit.death(e, { {223169, 1} });
end

function event_charm_start(e)
	charm_credit.charm_start(e, { {223169, 1} });
end

function event_charm_end(e)
	charm_credit.charm_end(e);
end