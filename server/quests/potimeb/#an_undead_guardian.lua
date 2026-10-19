local charm_credit = require("potimeb_charm_credit");

--#an_undead_guardian
--Phase 2 Undead
--potimeb

function event_death_complete(e)
	charm_credit.death(e, { {223242, 2} });
end

function event_charm_start(e)
	charm_credit.charm_start(e, { {223242, 2} });
end

function event_charm_end(e)
	charm_credit.charm_end(e);
end