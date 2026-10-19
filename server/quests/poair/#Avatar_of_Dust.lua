function event_combat(e)
	if e.joined then
			if(not eq.is_paused_timer("despawn")) then
				eq.pause_timer("despawn");
			end
	else
		eq.resume_timer("despawn");
		e.self:SaveGuardSpot(e.self:GetX(),e.self:GetY(), e.self:GetZ(), e.self:GetHeading()); 
	end
end


function event_timer(e)
	if (e.timer == 'Hardblur') then
		e.self:WipeHateList();
	elseif (e.timer == 'Softblur') then
		if (math.random(100)<=5) then
			e.self:WipeHateList();
		end
	elseif (e.timer == "despawn") then
			eq.stop_timer("despawn");
			eq.depop();
	end
end


function event_spawn(e)
	eq.set_timer("despawn", 7200 * 1000); -- 2h idle-despawn (forgiving; only runs while unengaged)
	e.self:Shout("Elements of Air and Dust I call upon you to aid me in the defense of this land!");
end

function event_death_complete(e)
eq.signal(215461,4); -- NPC: #Dust_Trigger
end
