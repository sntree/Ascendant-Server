function event_combat(e)
if e.joined then
else
end
end


function event_timer(e)
	if (e.timer == 'Hardblur') then
		e.self:WipeHateList();
	elseif (e.timer == 'Softblur') then
		if (math.random(100)<=10) then
			e.self:WipeHateList();
		end
	end
end
