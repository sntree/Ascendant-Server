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
#include "zone/corpse.h"
#include "common/serialize_buffer.h"

#include <cctype>
#include <map>
#include <memory>
#include <vector>

// Advanced Loot plumbing test (TOB+ clients). Builds the server->client AdvLoot packets by hand so their layouts
// can be checked in the client's Advanced Loot window before any real AdvLoot logic exists.

namespace {

void SendAdvLootPacket(Client *c, EmuOpcode opcode, SerializeBuffer &&buf)
{
	auto outapp = new EQApplicationPacket(opcode, std::move(buf));
	c->QueuePacket(outapp);
	safe_delete(outapp);
}

bool ParseHex(const std::string &text, std::vector<uint8> &out)
{
	std::string digits;
	for (char ch : text) {
		if (std::isxdigit(static_cast<unsigned char>(ch))) {
			digits.push_back(ch);
		}
		else if (ch != ' ' && ch != ':' && ch != ',') {
			return false;
		}
	}

	if (digits.size() % 2) {
		return false;
	}

	for (size_t i = 0; i < digits.size(); i += 2) {
		out.push_back(static_cast<uint8>(std::stoul(digits.substr(i, 2), nullptr, 16)));
	}

	return true;
}

} // namespace

void command_advloottest(Client *c, const Seperator *sep)
{
	if (c->ClientVersion() < EQ::versions::ClientVersion::TOB) {
		c->Message(Chat::White, "Advanced Loot packets only exist for TOB and later clients.");
		return;
	}

	const std::string sub = Strings::ToLower(sep->arg[1]);
	if (sub.empty() || sub == "help") {
		c->Message(Chat::White, "Usage: #advloottest corpse - send your targeted corpse's items to the Advanced Loot window");
		c->Message(Chat::White, "Usage: #advloottest ml [name] - announce a master looter (default: you)");
		c->Message(Chat::White, "Usage: #advloottest sync - send an empty full sync with you as master looter");
		c->Message(Chat::White, "Usage: #advloottest remove [corpse id] - remove a corpse from the window");
		c->Message(Chat::White, "Usage: #advloottest reset - clear the window");
		c->Message(Chat::White, "Usage: #advloottest raw [advloot|corpse|ml|reset|remove|settings|construct] [hex] - send bytes as-is");
		c->Message(Chat::White, "Usage: #advloottest status | off");
		return;
	}

	if (sub == "status") {
		c->Message(
			Chat::White,
			fmt::format(
				"Advanced Looting [{}] Master Looter Candidate [{}] test corpse [{}]",
				c->IsAdvancedLooting() ? "on" : "off",
				c->IsMasterLooterCandidate() ? "yes" : "no",
				c->GetAdvLootTestCorpse()
			).c_str()
		);
		return;
	}

	if (sub == "off") {
		c->SetAdvLootTestCorpse(0);
		c->Message(Chat::White, "Advanced Loot test mode off.");
		return;
	}

	if (sub == "corpse") {
		Mob *target = c->GetTarget();
		if (!target || !target->IsCorpse()) {
			c->Message(Chat::White, "Target a corpse first.");
			return;
		}

		Corpse *corpse = target->CastToCorpse();
		if (corpse->GetLootItems().empty()) {
			c->Message(Chat::White, "That corpse has no items.");
			return;
		}

		corpse->SendAdvancedLootItems(c);

		// test mode: loot requests for this corpse get a fake success reply and are echoed to you
		c->SetAdvLootTestCorpse(target->GetID());
		c->Message(Chat::White, fmt::format("Sent corpse [{}] ({}) to the Advanced Loot window.", target->GetCleanName(), target->GetID()).c_str());
		return;
	}

	if (sub == "ml") {
		const std::string name = sep->arg[2][0] ? sep->arg[2] : c->GetCleanName();
		SerializeBuffer buf;
		buf.WriteUInt32(0);
		buf.WriteString(name);
		SendAdvLootPacket(c, OP_AdvLootMasterLooter, std::move(buf));
		c->Message(Chat::White, fmt::format("Sent master looter [{}].", name).c_str());
		return;
	}

	if (sub == "sync") {
		// same shape as Live's reply to a solo zone-in: no corpses, no transactions, no choices
		SerializeBuffer buf;
		buf.WriteUInt16(3);
		buf.WriteUInt32(0);
		buf.WriteString(c->GetCleanName());
		buf.WriteUInt32(0);
		buf.WriteUInt8(0);
		buf.WriteUInt8(0);
		SendAdvLootPacket(c, OP_AdvLoot, std::move(buf));
		c->Message(Chat::White, "Sent an empty full sync.");
		return;
	}

	if (sub == "remove") {
		uint32 corpse_id = sep->IsNumber(2) ? Strings::ToUnsignedInt(sep->arg[2]) : c->GetAdvLootTestCorpse();
		if (!corpse_id && c->GetTarget()) {
			corpse_id = c->GetTarget()->GetID();
		}

		SerializeBuffer buf;
		buf.WriteUInt32(corpse_id);
		SendAdvLootPacket(c, OP_AdvLootRemoveCorpse, std::move(buf));
		c->Message(Chat::White, fmt::format("Sent remove corpse [{}].", corpse_id).c_str());
		return;
	}

	if (sub == "reset") {
		auto outapp = new EQApplicationPacket(OP_AdvLootReset, 0);
		c->QueuePacket(outapp);
		safe_delete(outapp);
		c->Message(Chat::White, "Sent reset.");
		return;
	}

	if (sub == "raw") {
		static const std::map<std::string, EmuOpcode> opcodes = {
			{"advloot", OP_AdvLoot},
			{"corpse", OP_AdvLootCorpseItems},
			{"ml", OP_AdvLootMasterLooter},
			{"reset", OP_AdvLootReset},
			{"remove", OP_AdvLootRemoveCorpse},
			{"settings", OP_AdvLootSettings},
			{"construct", OP_AdvLootConstructInfo},
		};

		auto it = opcodes.find(Strings::ToLower(sep->arg[2]));
		std::vector<uint8> bytes;
		if (it == opcodes.end() || !ParseHex(sep->argplus[3], bytes)) {
			c->Message(Chat::White, "Usage: #advloottest raw [advloot|corpse|ml|reset|remove|settings|construct] [hex bytes]");
			return;
		}

		auto outapp = new EQApplicationPacket(it->second, bytes.size());
		if (!bytes.empty()) {
			memcpy(outapp->pBuffer, bytes.data(), bytes.size());
		}
		c->QueuePacket(outapp);
		safe_delete(outapp);
		c->Message(Chat::White, fmt::format("Sent {} bytes on {}.", bytes.size(), it->first).c_str());
		return;
	}

	c->Message(Chat::White, "Unknown subcommand; use #advloottest help.");
}
