local charm_credit = require("potimeb_charm_credit");

--Rythor_of_the_Undead (223129)
--Phase 1 - Rythor of The Undead Trial
--potimeb

function event_death_complete(e)
	charm_credit.death(e, { {223171, 2} });
end

function event_charm_start(e)
	charm_credit.charm_start(e, { {223171, 2} });
end

function event_charm_end(e)
	charm_credit.charm_end(e);
end
