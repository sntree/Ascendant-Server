local charm_credit = require("potimeb_charm_credit");

--an_abhorrent (223198)
--Phase 5 (Inny Army)
--potimeb

function event_death_complete(e)
	charm_credit.death(e, { {223000, 1} });
end

function event_charm_start(e)
	charm_credit.charm_start(e, { {223000, 1} });
end

function event_charm_end(e)
	charm_credit.charm_end(e);
end