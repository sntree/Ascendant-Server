-- guildlobby\Magus_Alaria.lua NPCID 344013

function event_spawn(e)
	--auto-afk check to not draw a model
	local xloc = e.self:GetX();
	local yloc = e.self:GetY();
	eq.set_proximity(xloc - 75, xloc + 75, yloc - 75, yloc + 75);
end

function event_enter(e)
	e.other:Signal(1);
end

function event_exit(e)
	e.other:Signal(1);
end

function event_say(e)
	-- Ascendant: Nedaria's Landing only once Legacy of Ykesha is live (GMs always see it)
	local is_gm = (e.other:Admin() > 80 and e.other:GetGM());
	local nedaria_open = eq.is_the_legacy_of_ykesha_enabled() or is_gm;
	local nedaria_link = nedaria_open and ("[" .. eq.say_link("Nedaria's Landing",true,"Nedaria's Landing") .. "], ") or "";
	local nedaria_offer = nedaria_open and ("  I have enough time to send you to [" .. eq.say_link("Nedaria's Landing") .. "], if you'd like.") or "";

	--Adventurers Stone
	if(e.other:KeyRingCheck(741000) or e.other:HasItem(741000)) then
		if(e.message:findi("hail")) then
			e.other:Message(315, "Magus Alaria whispers, 'Hey " .. e.other:GetName() .. "! Do you have a structured LDoN schedule and you need ports now? Call Magus Alaria at [" .. eq.say_link("ports",true,"877-Ports-Now") .. "]!'");
		elseif(e.message:findi("ports")) then
			e.other:Message(315, "Magus Alaria whispers, 'It's YOUR adventure and you NEED ports NOW! I can send you to any of the Wayfarer camps in [" .. eq.say_link("Butcherblock",true,"Butcherblock") .. "], [" .. eq.say_link("Commonlands",true,"Commonlands") .. "], [" .. eq.say_link("Everfrost",true,"Everfrost") .. "], [" .. eq.say_link("North Ro",true,"North Ro") .. "], " .. nedaria_link .. "or [" .. eq.say_link("South Ro",true,"South Ro") .. "]. Operators are standing by!'");
		elseif(e.message:findi("nedaria") and nedaria_open) then
			e.other:MovePC(182,1463,1053,82.86,136); -- Zone: nedaria
		elseif(e.message:findi("butcherblock")) then
			e.other:MovePC(68,-2489,-1107,-.9,136); -- Zone: butcher
			--e.self:CastSpell(4179,e.other:GetID(),0,0);
		elseif(e.message:findi("commonlands")) then
			e.other:MovePC(22, -144,-1543,2.5,254); -- Zone: ecommons
			--e.self:CastSpell(4176,e.other:GetID(),0,0);
		elseif(e.message:findi("everfrost")) then
			e.other:MovePC(30, -5043,1863,-61.4,254); -- Zone: everfrost
			--e.self:CastSpell(4180,e.other:GetID(),0,0);
		elseif(e.message:findi("north ro")) then
			e.other:MovePC(34,914,2673,-26.09,456); -- Zone: nro
			--e.self:CastSpell(4177,e.other:GetID(),0,0);
		elseif(e.message:findi("south ro")) then
			e.other:MovePC(35,1053,-1461,-25.9,456); -- Zone: sro
			--e.self:CastSpell(4178,e.other:GetID(),0,0);
		end
	else --no Adventurers Stone
		if(e.message:findi("hail")) then
			e.other:Message(315, "Magus Alaria whispers, 'Have you seen my commercial? I'd love to help, but you're gonna need an Adventurer's Stone first. Talk to the Wayfarers, get yourself set up, and THEN call me at 877-PORTS-NOW!" .. nedaria_offer .. "'");
		elseif(e.message:findi("nedaria") and nedaria_open) then
			e.other:MovePC(182,1463,1053,82.86,136); -- Zone: nedaria
		end
	end
end

function event_trade(e)
	local item_lib = require("items");
	item_lib.return_items(e.self, e.other, e.trade);
end
