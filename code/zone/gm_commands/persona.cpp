#include "zone/client.h"

namespace {
	void SendPersonaUsage(Client *c)
	{
		c->Message(Chat::White, "Usage: #persona list");
		c->Message(Chat::White, "Usage: #persona switch [class id or name]");
		c->Message(Chat::White, "Usage: #persona delete [class id or name]");
	}

	// returns 0 when the argument names no player class
	uint8 ParsePersonaClass(const std::string &argument)
	{
		if (Seperator::IsNumber(argument.c_str())) {
			const uint32 class_id = Strings::ToUnsignedInt(argument);
			return class_id >= Class::Warrior && class_id <= Class::PLAYER_CLASS_COUNT ? static_cast<uint8>(class_id) : 0;
		}

		const std::string wanted = Strings::ToLower(Strings::Replace(argument, " ", ""));
		for (uint8 class_id = Class::Warrior; class_id <= Class::PLAYER_CLASS_COUNT; class_id++) {
			const std::string name = Strings::ToLower(Strings::Replace(GetClassIDName(class_id), " ", ""));
			if (wanted == name || wanted == Strings::ToLower(GetPlayerClassAbbreviation(class_id))) {
				return class_id;
			}
		}

		return 0;
	}
}

void command_persona(Client *c, const Seperator *sep)
{
	const std::string subcommand = Strings::ToLower(sep->arg[1]);
	const bool        is_list    = subcommand == "list";
	const bool        is_switch  = subcommand == "switch";
	const bool        is_delete  = subcommand == "delete";

	if (!is_list && !is_switch && !is_delete) {
		SendPersonaUsage(c);
		return;
	}

	uint8 class_id = 0;
	if (!is_list) {
		class_id = ParsePersonaClass(sep->arg[2]);
		if (!class_id) {
			SendPersonaUsage(c);
			return;
		}
	}

	Client *target = c;
	if (c->GetTarget() && c->GetTarget()->IsClient()) {
		target = c->GetTarget()->CastToClient();
	}

	if (!target->IsPersonasEnabled()) {
		c->Message(Chat::White, "Personas are not enabled.");
		return;
	}

	const std::string target_description = c->GetTargetDescription(target);
	const uint8       active_class       = target->GetBaseClass();

	if (is_list) {
		c->Message(Chat::White, fmt::format("Personas of {}:", target_description).c_str());
		for (const auto &[owned_class, level] : target->GetPersonaClasses()) {
			c->Message(
				Chat::White,
				fmt::format(
					"{} ({}) level {}{}",
					GetClassIDName(owned_class),
					owned_class,
					level,
					owned_class == active_class ? " [active]" : ""
				).c_str()
			);
		}

		return;
	}

	if (!target->HasPersona(class_id)) {
		c->Message(
			Chat::White,
			fmt::format("{} does not have a {} persona.", target_description, GetClassIDName(class_id)).c_str()
		);
		return;
	}

	if (is_switch) {
		if (class_id == active_class) {
			c->Message(Chat::White, fmt::format("{} is already a {}.", target_description, GetClassIDName(class_id)).c_str());
			return;
		}

		if (!target->SwitchPersona(class_id, true)) {
			c->Message(Chat::White, fmt::format("Could not switch {} to {}.", target_description, GetClassIDName(class_id)).c_str());
		}

		return;
	}

	if (class_id == active_class) {
		c->Message(Chat::White, "The active persona cannot be deleted. Switch to another persona first.");
		return;
	}

	if (!target->DeletePersona(class_id)) {
		c->Message(
			Chat::White,
			fmt::format("Could not delete the {} persona of {}.", GetClassIDName(class_id), target_description).c_str()
		);
		return;
	}

	c->Message(
		Chat::White,
		fmt::format("Deleted the {} persona of {}.", GetClassIDName(class_id), target_description).c_str()
	);
}
