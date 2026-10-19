local charm_credit = require("potimeb_charm_credit");

--A_Deepwater_Assassin (223141)
--Phase 2 
--potimeb

function event_death_complete(e)
	charm_credit.death(e, { {223242, 1} });
end

function event_charm_start(e)
	charm_credit.charm_start(e, { {223242, 1} });
end

function event_charm_end(e)
	charm_credit.charm_end(e);
end

function event_signal(e)
	--signal from A War Shapen Emmisary
	if e.signal == 1 then
		e.self:SetSpecialAbility(SpecialAbility.immune_aggro_on, 0);
		e.self:SetSpecialAbility(SpecialAbility.immune_aggro, 0);
		e.self:SetBodyType(BT.Monster, true);
	end
end
