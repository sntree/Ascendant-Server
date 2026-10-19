local charm_credit = require("potimeb_charm_credit");

--an_inferno_mephit (223095)
--Phase 1 - Kazrok of Fire Trial
--potimeb

function event_spawn(e)
	if math.random(100) >= 50 then --50% chance to be unmezzable (default is mezzable)
		e.self:SetSpecialAbility(SpecialAbility.unmezable, 1);	--flag as unmezzable
	end
	
	if math.random(100) >= 40 then	--default slow mitigation 20%, but has a chance also be fully slowable
		e.self:ModifyNPCStat("slow_mitigation","0");
	end
end

function event_death_complete(e)
	charm_credit.death(e, { {223173, 1} });
end

function event_charm_start(e)
	charm_credit.charm_start(e, { {223173, 1} });
end

function event_charm_end(e)
	charm_credit.charm_end(e);
end