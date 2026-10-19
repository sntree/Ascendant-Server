-- items: 60234
-- Ascendant (solo-ability): live required pickpocketing the Runic Inscription from the Caretaker and
-- handing it back. Here a hail hands it over instead, and killing the Caretaker also opens the doors,
-- so the trial can never get stuck.

local function open_caretaker_doors(sender)
	eq.zone_emote(MT.White,"Only research can solve the mystery of the runed glyphs");
	eq.ZoneMarquee(10,510,1,1,6000,"Only research can solve the mystery of the runed glyphs");
	eq.zone_emote(MT.Yellow,"Stone grinds against stone and the door slides open");
	eq.get_entity_list():FindDoor(15):SetLockPick(0);
	eq.get_entity_list():FindDoor(16):SetLockPick(0);
	eq.get_entity_list():FindDoor(15):ForceOpen(sender);
	eq.get_entity_list():FindDoor(16):ForceOpen(sender);
end

function event_say(e)
	eq.zone_emote(MT.White,"The stone worker does not move. It almost seems not to have any purpose at all, yet it blocks the door behind it.");
	if e.message:findi("hail") and not e.other:HasItem(60234) then
		e.other:Message(MT.Yellow, "As you touch the stone worker, a Runic Inscription comes loose from its body. Perhaps it should be returned.");
		e.other:SummonItem(60234); -- Item: Runic Inscription
	end
end

function event_trade(e)
	local item_lib = require("items");
	if(item_lib.check_turn_in(e.trade, {item1 = 60234})) then
		e.self:Emote("motions towards the door behind it. You have passed the first test.");
		open_caretaker_doors(e.self);
	end
	item_lib.return_items(e.self, e.other, e.trade);
end

function event_death(e)
	open_caretaker_doors(e.self);
end
