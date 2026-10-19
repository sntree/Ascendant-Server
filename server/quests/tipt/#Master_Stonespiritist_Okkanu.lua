function event_say(e)
  local qglobals = eq.get_qglobals(e.other)
  local has_tipt_access = (qglobals.god_tipt_access and qglobals.god_tipt_access == "1")
  local has_kodtaz_access = (qglobals.god_kodtaz_access and qglobals.god_kodtaz_access == "1")

  local tipt_flag_key = string.format("%s-god_tipt", e.other:CharacterID())

  if e.message:findi("hail") then
    -- this expedition gets locked so no need to track flagged count
    if has_kodtaz_access then
      e.other:Message(MT.NPCQuestSay, "Okkanu tells you, 'I'm afraid that I have already aided as many as I can, I do not have the strength to assist you further.")
    else
      -- live gives the same message whether it's a temporary or permanent flag
      -- Ascendant (TLP-style access): finishing Tipt alone grants the permanent flag and Kod'Taz access
      e.other:Message(MT.NPCQuestSay, "Okkanu tells you, 'Your bravery is notable, my friend. You have earned the trust of my people. The way to the temples is open to you.'")
      e.other:Message(MT.Yellow, "You have earned access to Kod'Taz, Broken Trial Grounds.")

      if true then
        eq.set_data(tipt_flag_key, "1")
        eq.set_global("god_kodtaz_access", "1", 5, "F") -- make permanent
      else
        eq.set_data(tipt_flag_key, "T")
      end
    end
  end
end
