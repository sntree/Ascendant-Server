local charm_credit = require("potimeb_charm_credit");

--a_triloun_gatherer
--Phase 1 - Anar of Water Trial
--potimeb

function event_spawn(e)
	if math.random(100) >= 50 then	--default slow mitigation 0%, but may mitigate 20%
		e.self:ModifyNPCStat("slow_mitigation","20");
	end
end

function event_death_complete(e)
	charm_credit.death(e, { {223172, 1} });
end

function event_charm_start(e)
	charm_credit.charm_start(e, { {223172, 1} });
end

function event_charm_end(e)
	charm_credit.charm_end(e);
end