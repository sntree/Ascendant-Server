local charm_credit = require("charm_credit");

local function credit_wave(e)
if(e.self:GetSp2() == 282505) then --spawn group inside the overlord room
eq.signal(226204,1); --#overlord_trigger (226204) to add to wave counter
end
end

function event_death_complete(e)
charm_credit.credit_once(e, credit_wave);
end

function event_charm_start(e)
charm_credit.credit_once(e, credit_wave);
end

function event_charm_end(e)
charm_credit.depop_on_charm_end(e);
end
