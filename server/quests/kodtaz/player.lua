--KT fern click for Berserker 1.5
-- items: 16949

-- Ascendant: leaving an Ikkinz trial drops you on its entrance trigger (#Zoner4-6);
-- remember zone-in time so the zoners can ignore you for a few seconds
function event_enter_zone(e)
  e.self:SetEntityVariable("kt_zonein", tostring(os.time()));
end

function event_click_door(e)
  local door_id = e.door:GetDoorID();
	if(door_id == 25 or door_id == 26 or door_id == 27) then
	  local qglobals = eq.get_qglobals(e.self);
		if(e.self:Class() == "Berserker" and qglobals["kt_" .. door_id .. "fern"] == nil) then
				e.self:SummonItem(16949); --Taelosian Fern Sample 
				eq.set_global("kt_" .. door_id .. "fern","1",3,"H1")
		end
	end
end