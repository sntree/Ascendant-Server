-- sro\Magus_Jerira.lua NPCID 35066

function event_say(e)
	-- Ascendant: Nedaria's Landing only once Legacy of Ykesha is live (GMs always see it)
	local is_gm = (e.other:Admin() > 80 and e.other:GetGM());
	local nedaria_open = eq.is_the_legacy_of_ykesha_enabled() or is_gm;
	local nedaria_link = nedaria_open and ("[" .. eq.say_link("Nedaria's Landing",false,"Nedaria's Landing") .. "], ") or "";
	local nedaria_offer = nedaria_open and ("  I have enough time to send you to [" .. eq.say_link("Nedaria's Landing") .. "], if you'd like.") or "";

	--Adventurers Stone
	if(e.other:KeyRingCheck(741000) or e.other:HasItem(741000)) then
		if(e.message:findi("hail")) then
			e.self:Say("Nice to see you, " .. e.other:GetName() .. ".  Am I safe in assuming you wish to travel today?  I do admire the adventurers that so willingly take the risk involved with this spell.  I enjoy knowing all of my hard work is not in vain.  There's only been a few unfortunate folk that haven't, uhm, been so lucky.  Frightful mess.  I'll be sure to be extra careful where you are concerned.  Honestly.  A dark elf's word is... Well, forget that.  He he.  Tell me where you would like to go and I will send you there.  I can send you to any of the other camps in [" .. eq.say_link("Butcherblock",false,"Butcherblock") .. "], [" .. eq.say_link("Commonlands",false,"Commonlands") .. "], [" .. eq.say_link("Everfrost",false,"Everfrost") .. "], " .. nedaria_link .. "or [" .. eq.say_link("North Ro",false,"North Ro") .. "].");
		
		elseif(e.message:findi("nedaria") and nedaria_open) then
			e.self:CastSpell(4580,e.other:GetID(),0,1); -- Spell: Teleport: Nedaria
		elseif(e.message:findi("everfrost")) then
			e.self:CastSpell(4180,e.other:GetID(),0,1); -- Spell: Teleport: Everfrost
		elseif(e.message:findi("butcherblock")) then
			e.self:CastSpell(4179,e.other:GetID(),0,1); -- Spell: Teleport: Butcherblock

		elseif(e.message:findi("north ro")) then
		--e.self:CastSpell(4177,e.other:GetID(),0,1);		 
			e.other:MovePC(34, -914, 2679, -25, 20); -- Zone: nro

		elseif(e.message:findi("commonlands")) then
			--e.self:CastSpell(4176,e.other:GetID(),0,1);
			e.other:MovePC(22,-140,-1520,3,280); -- needs_heading_validation
		end
	else --no Adventurers Stone
		if(e.message:findi("hail")) then
			e.self:Say("You will have to excuse me, I am quite busy studying this Farstone and the possibility of using the magic stored inside of it.  Perhaps you should talk to those at the Wayfarer camps to see if they have any use for you." .. nedaria_offer);
		elseif(e.message:findi("nedaria") and nedaria_open) then
			e.self:CastSpell(4580,e.other:GetID(),0,1); -- Spell: Teleport: Nedaria
		end
	end
end

function event_trade(e)
	local item_lib = require("items");
	item_lib.return_items(e.self, e.other, e.trade);
end
