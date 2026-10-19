local charm_credit = require("potimeb_charm_credit");

--Amygdalan_knight (223199)
--Phase 5 (CT Army)
--potimeb

function event_death_complete(e)
	charm_credit.death(e, { {223165, 1} });
end

function event_charm_start(e)
	charm_credit.charm_start(e, { {223165, 1} });
end

function event_charm_end(e)
	charm_credit.charm_end(e);
end