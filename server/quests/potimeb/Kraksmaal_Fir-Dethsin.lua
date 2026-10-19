local charm_credit = require("potimeb_charm_credit");

--Kraksmaal_Fir`Dethsin
--Phase 3 Event Mini-boss
--potimeb

function event_combat(e)
	if not e.joined then
		--mob will instantly path back to spawn if no one on hate list
		e.self:CastToNPC():MoveTo(e.self:GetGuardPointX(),e.self:GetGuardPointY(),e.self:GetGuardPointZ(),385,true);	
	end
end

function event_death_complete(e)
	charm_credit.death(e, { {223097, 223017}, {223097, 4} });
end

function event_charm_start(e)
	charm_credit.charm_start(e, { {223097, 223017}, {223097, 4} });
end

function event_charm_end(e)
	charm_credit.charm_end(e);
end