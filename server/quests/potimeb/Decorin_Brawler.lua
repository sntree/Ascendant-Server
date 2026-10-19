local charm_credit = require("potimeb_charm_credit");

--Decorin_Brawler (223002)
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