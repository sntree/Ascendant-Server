local charm_credit = require("potimeb_charm_credit");

--an_undead_protector (223150)
--Phase 1 - Rythor of The Undead Trial
--potimeb

function event_spawn(e)
	if math.random(100) >= 50 then	--default slow mitigation 20%, but has a chance to mitigate up to 50%
		e.self:ModifyNPCStat("slow_mitigation","50");
	end
end

function event_death_complete(e)
	charm_credit.death(e, { {223171, 2} });
end

function event_charm_start(e)
	charm_credit.charm_start(e, { {223171, 2} });
end

function event_charm_end(e)
	charm_credit.charm_end(e);
end
