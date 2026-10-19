local charm_credit = require("potimeb_charm_credit");

--a_flame_mephit_ (223094)
--Phase 1 - Kazrok of Fire Trial
--potimeb

function event_spawn(e)
	if math.random(100) >= 50 then --50% chance to be unmezzable (default is mezzable)
		e.self:SetSpecialAbility(SpecialAbility.unmezable, 1);	--flag as unmezzable
	end
end

function event_death_complete(e)
	charm_credit.death(e, { {223173, 2} });
end

function event_charm_start(e)
	charm_credit.charm_start(e, { {223173, 2} });
end

function event_charm_end(e)
	charm_credit.charm_end(e);
end