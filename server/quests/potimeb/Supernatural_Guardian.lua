local charm_credit = require("potimeb_charm_credit");

--Supernatural_Guardian (223074)
--Phase 3 Final Boss 
--potimeb

function event_death_complete(e)
	charm_credit.death(e, { {223097, 223074}, {223097, 4} });
end

function event_charm_start(e)
	charm_credit.charm_start(e, { {223097, 223074}, {223097, 4} });
end

function event_charm_end(e)
	charm_credit.charm_end(e);
end