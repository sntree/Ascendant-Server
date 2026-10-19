local charm_credit = require("potimeb_charm_credit");

--Guardian_Knight_of_Pestilence (223196)
--Phase 5 (Bert Army)
--potimeb

function event_death_complete(e)
	charm_credit.death(e, { {223098, 1} });
end

function event_charm_start(e)
	charm_credit.charm_start(e, { {223098, 1} });
end

function event_charm_end(e)
	charm_credit.charm_end(e);
end