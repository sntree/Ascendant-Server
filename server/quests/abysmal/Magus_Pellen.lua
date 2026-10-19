-- abysmal\Magus_Pellen.lua NPCID 279217

function event_say(e)
	if e.message:findi("hail") then
		e.self:Say("I can provide you with travel to [" .. eq.say_link("Natimbi",false,"Natimbi") .. "] with our Farstone magic, or back to [" .. eq.say_link("Nedaria's Landing",false,"Nedaria's Landing") .. "].  Just tell me where you'd like to go and I shall send you.");
	elseif e.message:findi("natimbi") then
		e.other:MovePC(280, -1557, -853, 241, 180); -- Zone: natimbi
	elseif e.message:findi("nedaria") then
		-- Ascendant: return trip standing in for the Queen of Thorns boat
		e.other:MovePC(182, -1737, -181, 256, 0); -- Zone: nedaria (safe point)
	end
end

function event_trade(e)
	local item_lib = require("items");
	item_lib.return_items(e.self, e.other, e.trade);
end
