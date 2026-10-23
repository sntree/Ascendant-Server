/*	EQEmu: EQEmulator

	Copyright (C) 2001-2026 EQEmu Development Team

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 3 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program. If not, see <http://www.gnu.org/licenses/>.
*/
#include "zone/client.h"
#include "zone/merc.h"

namespace {
	// One line per class and stance, shown by $merc status
	const char* GetStanceMeaning(uint8 merc_class, uint8 stance)
	{
		switch (stance) {
			case Stance::Passive:
								if (merc_class == HEALER) {
									return RuleB(Mercs, PassiveHealerEmergencyHeals)
										? "Heals and cures only in an emergency. No buffs or resurrections."
										: "Does nothing: no heals, buffs or resurrections.";
								}

								return "Follows you and never attacks. Call Assist still works for one mob.";
			case Stance::Balanced:
				switch (merc_class) {
					case TANK:
						return "Fights what hits the group, taunts, and joins your target once you have hate.";
					case HEALER:
						return "Heals early, keeps some mana back, and buffs.";
					case CASTERDPS:
						return "Assists once you have hate, eases off near the tank's hate, keeps some mana back, no area nukes.";
					default:
						return "Assists once you have hate and eases off near the tank's hate.";
				}
			case Stance::Efficient:
			case Stance::Efficient2:
				return "Heals later with the cheapest heal that fits, and keeps more mana back.";
			case Stance::Reactive:
				return "Heals sooner with the fastest heal that fits.";
			case Stance::Aggressive:
				return "Joins as soon as you attack, from further away. Taunts and area taunts to hold everything.";
			case Stance::Assist:
				return "Fights your target. No taunts; hate disciplines only when you are not top.";
			case Stance::Burn:
				return "Joins when you attack and spends everything with no hate check. No area nukes.";
			case Stance::AEBurn:
				return "Burn, plus area nukes when enough mobs are fighting the group.";
			default:
				return "";
		}
	}

	bool ParseOnOff(const char* argument, bool& value)
	{
		if (!strcasecmp(argument, "on")) {
			value = true;
			return true;
		}

		if (!strcasecmp(argument, "off")) {
			value = false;
			return true;
		}

		return false;
	}

	const char* OnOff(bool value)
	{
		return value ? "on" : "off";
	}

	void SendUsage(Client* c)
	{
		c->Message(Chat::White, "Usage: $merc status");
		c->Message(Chat::White, "Usage: $merc assist [on|off|call] - Auto Assist on or off, or call your mercenary onto your target");
		c->Message(Chat::White, "Usage: $merc hold | back - Hold off attacking until you say back (Call Assist still works)");
		c->Message(Chat::White, "%s", fmt::format("Usage: $merc follow [{}-{}|default] - How close your mercenary stays", MercFollowDistanceMin, MercFollowDistanceMax).c_str());
		c->Message(Chat::White, "%s", fmt::format("Usage: $merc heal [0-{}] - A healer heals you this many percent earlier", MercHealOffsetMax).c_str());
		c->Message(Chat::White, "Usage: $merc announce [on|off] - Short messages about what your mercenary is doing");
		c->Message(Chat::White, "Usage: $merc ae [on|off] - Let a caster mercenary cast area nukes when its stance allows");
	}

	void SendStatus(Client* c)
	{
		const MercOwnerSettings& settings = c->GetMercSettings();
		Merc* const merc = c->GetMerc();

		if (merc) {
			c->Message(Chat::White, "%s", fmt::format("{} ({}): {}", merc->GetCleanName(), Stance::GetName(merc->GetStance()), GetStanceMeaning(merc->GetClass(), merc->GetStance())).c_str());
			c->Message(
				Chat::White,
				"%s",
				fmt::format(
					"State: {}{}. Mana {}%.",
					settings.hold ? "holding" : (merc->IsEngaged() ? "fighting" : "ready"),
					merc->GetTarget() ? fmt::format(", target {}", merc->GetTarget()->GetCleanName()) : "",
					static_cast<int>(merc->GetManaRatio())
				).c_str()
			);
		}
		else {
			c->Message(Chat::White, "You have no mercenary out. Your settings are kept for the next one.");
		}

		c->Message(
			Chat::White,
			"%s",
			fmt::format(
				"Auto Assist {}, announce {}, area nukes {}, follow {}, heal {}.",
				OnOff(c->IsMercAutoAssistOn()),
				OnOff(settings.announce),
				OnOff(settings.area_nukes),
				settings.follow_distance ? std::to_string(settings.follow_distance) : "default",
				settings.heal_offset ? fmt::format("+{}%", settings.heal_offset) : "default"
			).c_str()
		);
	}

	void SetOnOffSetting(Client* c, const Seperator* sep, bool& setting, const char* bucket, const char* name)
	{
		bool value = false;

		if (!ParseOnOff(sep->arg[2], value)) {
			c->Message(Chat::White, "%s", fmt::format("{} is {}. Use $merc {} on or off.", name, OnOff(setting), sep->arg[1]).c_str());
			return;
		}

		setting = value;
		c->SetBucket(bucket, value ? "1" : "0");
		c->Message(Chat::White, "%s", fmt::format("{} is now {}.", name, OnOff(value)).c_str());
	}
}

void command_merc(Client *c, const Seperator *sep)
{
	if (!RuleB(Mercs, AllowMercs)) {
		c->Message(Chat::White, "Mercenaries are not enabled on this server.");
		return;
	}

	MercOwnerSettings& settings = c->GetMercSettings();
	const char* subcommand = sep->arg[1];

	if (!strcasecmp(subcommand, "status")) {
		SendStatus(c);
	}
	else if (!strcasecmp(subcommand, "assist")) {
		bool value = false;

		if (!strcasecmp(sep->arg[2], "call")) {
			if (!c->CallMercAssist()) {
				c->Message(Chat::White, "Target a mob your mercenary can attack first.");
			}
		}
		else if (ParseOnOff(sep->arg[2], value)) {
			c->SetMercAutoAssist(value);
			c->Message(Chat::White, "%s", fmt::format("Auto Assist is now {}.", OnOff(value)).c_str());
		}
		else {
			c->Message(Chat::White, "%s", fmt::format("Auto Assist is {}. Use $merc assist on, off or call.", OnOff(c->IsMercAutoAssistOn())).c_str());
		}
	}
	else if (!strcasecmp(subcommand, "hold")) {
		settings.hold = true;
		c->Message(Chat::White, "Your mercenary holds off attacking until you use $merc back. Call Assist still works.");
	}
	else if (!strcasecmp(subcommand, "back")) {
		settings.hold = false;
		c->Message(Chat::White, "Your mercenary fights as its stance says again.");
	}
	else if (!strcasecmp(subcommand, "follow")) {
		if (!strcasecmp(sep->arg[2], "default")) {
			settings.follow_distance = 0;
			c->DeleteBucket(MercFollowBucket);
			c->Message(Chat::White, "Your mercenary follows at the default distance.");
		}
		else if (sep->IsNumber(2)) {
			const int distance = Strings::ToInt(sep->arg[2]);

			if (distance < MercFollowDistanceMin || distance > MercFollowDistanceMax) {
				c->Message(Chat::White, "%s", fmt::format("Follow distance must be {} to {}.", MercFollowDistanceMin, MercFollowDistanceMax).c_str());
				return;
			}

			settings.follow_distance = static_cast<uint8>(distance);
			c->SetBucket(MercFollowBucket, std::to_string(distance));
			c->Message(Chat::White, "%s", fmt::format("Your mercenary follows at {} units.", distance).c_str());
		}
		else {
			c->Message(Chat::White, "%s", fmt::format("Follow distance is {}. Use $merc follow {}-{} or default.", settings.follow_distance ? std::to_string(settings.follow_distance) : "default", MercFollowDistanceMin, MercFollowDistanceMax).c_str());
		}
	}
	else if (!strcasecmp(subcommand, "heal")) {
		if (sep->IsNumber(2)) {
			const int offset = Strings::ToInt(sep->arg[2]);

			if (offset < 0 || offset > MercHealOffsetMax) {
				c->Message(Chat::White, "%s", fmt::format("Heal offset must be 0 to {}.", MercHealOffsetMax).c_str());
				return;
			}

			settings.heal_offset = static_cast<uint8>(offset);
			c->SetBucket(MercHealOffsetBucket, std::to_string(offset));
			c->Message(Chat::White, "%s", fmt::format("A healer mercenary heals you {} percent earlier.", offset).c_str());
		}
		else {
			c->Message(Chat::White, "%s", fmt::format("Heal offset is {}. Use $merc heal 0-{}.", settings.heal_offset, MercHealOffsetMax).c_str());
		}
	}
	else if (!strcasecmp(subcommand, "announce")) {
		SetOnOffSetting(c, sep, settings.announce, MercAnnounceBucket, "Mercenary announcements");
	}
	else if (!strcasecmp(subcommand, "ae")) {
		SetOnOffSetting(c, sep, settings.area_nukes, MercAreaNukesBucket, "Mercenary area nukes");
	}
	else {
		SendUsage(c);
	}
}
