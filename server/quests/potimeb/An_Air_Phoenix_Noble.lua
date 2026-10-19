local charm_credit = require("potimeb_charm_credit");

--An_Air_Phoenix_Noble (223087)
--Phase 1 Air Trial
--potimeb

function event_death_complete(e)
	charm_credit.death(e, { {223170, 1} });
end

function event_charm_start(e)
	charm_credit.charm_start(e, { {223170, 1} });
end

function event_charm_end(e)
	charm_credit.charm_end(e);
end
