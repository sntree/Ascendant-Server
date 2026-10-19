#include "zone/client.h"
#include "zone/zone.h"

namespace {
	void SendAchievementUsage(Client *c)
	{
		c->Message(Chat::White, "Usage: #achievement info [achievement_id]");
		c->Message(Chat::White, "Usage: #achievement complete [achievement_id]");
		c->Message(Chat::White, "Usage: #achievement reset [achievement_id]");
		c->Message(Chat::White, "Usage: #achievement progress [achievement_id] [component_id] [amount]");
	}
}

void command_achievement(Client *c, const Seperator *sep)
{
	const std::string subcommand = Strings::ToLower(sep->arg[1]);
	const bool        is_info     = subcommand == "info";
	const bool        is_complete = subcommand == "complete";
	const bool        is_reset    = subcommand == "reset";
	const bool        is_progress = subcommand == "progress";

	if (!is_info && !is_complete && !is_reset && !is_progress) {
		SendAchievementUsage(c);
		return;
	}

	if (!sep->IsNumber(2) || (is_progress && (!sep->IsNumber(3) || !sep->IsNumber(4)))) {
		SendAchievementUsage(c);
		return;
	}

	Client *target = c;
	if (c->GetTarget() && c->GetTarget()->IsClient()) {
		target = c->GetTarget()->CastToClient();
	}

	if (!target->AreAchievementsEnabled()) {
		c->Message(
			Chat::White,
			fmt::format(
				"Achievements are not enabled for {}; they require rule Character:EnableAchievements and a ToB or newer client.",
				c->GetTargetDescription(target)
			).c_str()
		);
		return;
	}

	const uint32 achievement_id = Strings::ToUnsignedInt(sep->arg[2]);
	const auto &achievements    = zone->GetAchievementDefinitions();
	const int  index            = achievements.GetIndex(achievement_id);
	if (index < 0) {
		c->Message(Chat::White, fmt::format("Achievement {} does not exist.", achievement_id).c_str());
		return;
	}

	const auto &definition = achievements.GetDefinitions()[index];

	if (is_info) {
		c->Message(
			Chat::White,
			fmt::format(
				"Achievement {} ({}) for {}: completed {}.",
				definition.id,
				definition.name,
				c->GetTargetDescription(target),
				target->IsAchievementCompleted(achievement_id) ? "yes" : "no"
			).c_str()
		);

		for (const auto &component : definition.components) {
			c->Message(
				Chat::White,
				fmt::format(
					"Component {} | type {} | event type {} | event id {} | event count {} | progress {}",
					component.component_id,
					static_cast<uint32>(component.type),
					static_cast<uint32>(component.event_type),
					component.event_id,
					component.event_count,
					target->GetAchievementComponentProgress(achievement_id, component.component_id)
				).c_str()
			);
		}

		return;
	}

	if (is_complete) {
		c->Message(
			Chat::White,
			fmt::format(
				"{} achievement {} ({}) for {}.",
				target->CompleteAchievement(achievement_id) ? "Completed" : "Could not complete",
				definition.id,
				definition.name,
				c->GetTargetDescription(target)
			).c_str()
		);
		return;
	}

	if (is_reset) {
		c->Message(
			Chat::White,
			fmt::format(
				"{} achievement {} ({}) for {}.",
				target->ResetAchievement(achievement_id) ? "Reset" : "Could not reset",
				definition.id,
				definition.name,
				c->GetTargetDescription(target)
			).c_str()
		);
		return;
	}

	const uint32 component_id = Strings::ToUnsignedInt(sep->arg[3]);
	const uint32 amount       = Strings::ToUnsignedInt(sep->arg[4]);

	c->Message(
		Chat::White,
		fmt::format(
			"{} component {} of achievement {} ({}) by {} for {}.",
			target->UpdateAchievementComponent(achievement_id, component_id, amount) ? "Advanced" : "Could not advance",
			component_id,
			definition.id,
			definition.name,
			amount,
			c->GetTargetDescription(target)
		).c_str()
	);
}
