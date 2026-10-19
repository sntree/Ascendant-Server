local charm_credit = require("potimeb_charm_credit");

--Earthen_Overseer (223134)
--Phase 2 Mini-boss
--potimeb

function event_death_complete(e)
	charm_credit.death(e, { {223097, 223134}, {223242, 1} });
end

function event_charm_start(e)
	charm_credit.charm_start(e, { {223097, 223134}, {223242, 1} });
end

function event_charm_end(e)
	charm_credit.charm_end(e);
end
