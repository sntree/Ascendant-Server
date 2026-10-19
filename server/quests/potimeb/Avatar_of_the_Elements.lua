local charm_credit = require("potimeb_charm_credit");

--Avatar_of_the_Elements (223073)
--Phase 3 Final Boss 
--potimeb

function event_death_complete(e)
	charm_credit.death(e, { {223097, 223073}, {223097, 4} });
end

function event_charm_start(e)
	charm_credit.charm_start(e, { {223097, 223073}, {223097, 4} });
end

function event_charm_end(e)
	charm_credit.charm_end(e);
end