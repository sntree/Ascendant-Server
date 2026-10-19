#include "zone/client.h"

namespace {
	void SendMarketplaceCoinsUsage(Client *c)
	{
		c->Message(Chat::White, "Usage: #marketplacecoins show");
		c->Message(Chat::White, "Usage: #marketplacecoins [add|remove|set] [amount]");
	}
}

void command_marketplacecoins(Client *c, const Seperator *sep)
{
	const std::string subcommand = Strings::ToLower(sep->arg[1]);
	const bool        is_show    = subcommand == "show";
	const bool        is_add     = subcommand == "add";
	const bool        is_remove  = subcommand == "remove";
	const bool        is_set     = subcommand == "set";

	if (!is_show && !is_add && !is_remove && !is_set) {
		SendMarketplaceCoinsUsage(c);
		return;
	}

	if (!is_show && !sep->IsNumber(2)) {
		SendMarketplaceCoinsUsage(c);
		return;
	}

	Client *target = c;
	if (c->GetTarget() && c->GetTarget()->IsClient()) {
		target = c->GetTarget()->CastToClient();
	}

	bool changed = true;
	if (!is_show) {
		const uint32 amount = Strings::ToUnsignedInt(sep->arg[2]);
		if (is_add) {
			changed = target->AddMarketplaceCoins(amount, "gm", fmt::format("#marketplacecoins by {}", c->GetCleanName()));
		} else if (is_remove) {
			changed = target->RemoveMarketplaceCoins(amount, "gm", fmt::format("#marketplacecoins by {}", c->GetCleanName()));
		} else {
			changed = target->SetMarketplaceCoins(amount, "gm", fmt::format("#marketplacecoins by {}", c->GetCleanName()));
		}
	}

	c->Message(
		Chat::White,
		fmt::format(
			"{}Marketplace Coins of {} (account-wide): {}.",
			changed ? "" : "Not changed. ",
			c->GetTargetDescription(target),
			target->GetMarketplaceCoins()
		).c_str()
	);
}
