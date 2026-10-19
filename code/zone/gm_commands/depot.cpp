#include "zone/client.h"
#include "zone/zonedb.h"

namespace {
	void SendDepotUsage(Client *c)
	{
		c->Message(Chat::White, "Usage: #depot list");
		c->Message(Chat::White, "Usage: #depot add [item_id] [quantity]");
		c->Message(Chat::White, "Usage: #depot remove [item_id] [quantity]");
	}
}

void command_depot(Client *c, const Seperator *sep)
{
	const std::string subcommand = Strings::ToLower(sep->arg[1]);
	const bool        is_list    = subcommand == "list";
	const bool        is_add     = subcommand == "add";
	const bool        is_remove  = subcommand == "remove";

	if (!is_list && !is_add && !is_remove) {
		SendDepotUsage(c);
		return;
	}

	if (!is_list && (!sep->IsNumber(2) || !sep->IsNumber(3))) {
		SendDepotUsage(c);
		return;
	}

	Client *target = c;
	if (c->GetTarget() && c->GetTarget()->IsClient()) {
		target = c->GetTarget()->CastToClient();
	}

	if (is_list) {
		const auto &rows = target->GetTradeskillDepotItems();
		c->Message(
			Chat::White,
			fmt::format(
				"Tradeskill Depot of {}: {} of {} items.",
				c->GetTargetDescription(target),
				rows.size(),
				target->GetTradeskillDepotCapacity()
			).c_str()
		);

		for (const auto &[item_id, quantity] : rows) {
			const auto item = database.GetItem(item_id);
			c->Message(
				Chat::White,
				fmt::format("{} x {} ({})", quantity, item ? item->Name : "unknown item", item_id).c_str()
			);
		}

		return;
	}

	const uint32 item_id  = Strings::ToUnsignedInt(sep->arg[2]);
	const uint32 quantity = Strings::ToUnsignedInt(sep->arg[3]);
	const bool   changed  = is_add ?
		target->AddTradeskillDepotItem(item_id, quantity) :
		target->RemoveTradeskillDepotItem(item_id, quantity);

	c->Message(
		Chat::White,
		fmt::format(
			"{} {} of item {} {} the Tradeskill Depot of {} (now {}).",
			changed ? (is_add ? "Added" : "Removed") : (is_add ? "Could not add" : "Could not remove"),
			quantity,
			item_id,
			is_add ? "to" : "from",
			c->GetTargetDescription(target),
			target->GetTradeskillDepotQuantity(item_id)
		).c_str()
	);
}
