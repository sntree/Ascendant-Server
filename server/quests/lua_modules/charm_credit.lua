local charm_credit = {};

local default_var = "ascendant_charm_credited";

local function get_var_name(var_name)
	return var_name or default_var;
end

local function has_credit(e, var_name)
	return e.self:EntityVariableExists(get_var_name(var_name));
end

function charm_credit.credit_once(e, credit_func, var_name)
	if has_credit(e, var_name) then
		return false;
	end

	e.self:SetEntityVariable(get_var_name(var_name), "1");

	if credit_func then
		credit_func(e);
	end

	return true;
end

function charm_credit.signal(e, signals, var_name)
	return charm_credit.credit_once(e, function()
		for _, signal in ipairs(signals) do
			eq.signal(signal[1], signal[2]);
		end
	end, var_name);
end

function charm_credit.depop_on_charm_end(e, var_name)
	if has_credit(e, var_name) then
		e.self:Depop(false);
	end
end

return charm_credit;
