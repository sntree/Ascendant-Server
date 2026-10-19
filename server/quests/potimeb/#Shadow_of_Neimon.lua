local charm_credit = require("potimeb_charm_credit");

--#Shadow_of_Neimon (223240)
--Phase 1 - Neimon of Air Trial
--potimeb

function event_death_complete(e)
	charm_credit.death(e, { {223170, 2} });
end

function event_charm_start(e)
	charm_credit.charm_start(e, { {223170, 2} });
end

function event_charm_end(e)
	charm_credit.charm_end(e);
end
