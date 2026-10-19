local charm_credit = require("potimeb_charm_credit");

--A_Fierce_War_Boar (223139)
--Phase 5 RZ Army
--potimeb

function event_death_complete(e)
	charm_credit.death(e, { {223001, 1} });
end

function event_charm_start(e)
	charm_credit.charm_start(e, { {223001, 1} });
end

function event_charm_end(e)
	charm_credit.charm_end(e);
end