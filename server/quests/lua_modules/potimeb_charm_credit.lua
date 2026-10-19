local charm_credit = {};

local credited_var = "potimeb_charm_credited";

local function has_credit(e)
	return e.self:EntityVariableExists(credited_var);
end

function charm_credit.credit(e, signals)
	if has_credit(e) then
		return false;
	end

	e.self:SetEntityVariable(credited_var, "1");

	for _, signal in ipairs(signals) do
		eq.signal(signal[1], signal[2]);
	end

	return true;
end

function charm_credit.death(e, signals)
	charm_credit.credit(e, signals);
end

function charm_credit.charm_start(e, signals)
	charm_credit.credit(e, signals);
end

function charm_credit.charm_end(e)
	if has_credit(e) then
		e.self:Depop(false);
	end
end

return charm_credit;
