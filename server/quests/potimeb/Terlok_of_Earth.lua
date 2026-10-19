local charm_credit = require("potimeb_charm_credit");

--Terlok_of_Earth (223119)
--Phase 1 - Terlok of Earth Trial
--potimeb

function event_death_complete(e)
	charm_credit.death(e, { {223169, 2} });
end

function event_charm_start(e)
	charm_credit.charm_start(e, { {223169, 2} });
end

function event_charm_end(e)
	charm_credit.charm_end(e);
end