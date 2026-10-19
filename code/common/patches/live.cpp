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

#include "common/global_define.h"
#include "common/eqemu_logsys.h"
#include "live.h"
#include "common/opcodemgr.h"
#include "common/eq_stream_intf.h"
#include "common/eq_stream_ident.h"
#include "common/crc32.h"
#include "common/eq_packet_structs.h"
#include "common/misc_functions.h"
#include "common/strings.h"
#include "common/serialize_buffer.h"
#include "common/path_manager.h"
#include "common/patches/tob_structs.h"
#include "common/patches/live_structs.h"

namespace Live
{
	static const char* name = "Live";
	static OpcodeManager* opcodes = nullptr;
	static Strategy struct_strategy;

	void Register(EQStreamIdentifier& into)
	{
		if (opcodes == nullptr) {
			std::string opfile = fmt::format("{}/patch_{}.conf", PathManager::Instance()->GetPatchPath(), name);
			opcodes = new RegularOpcodeManager();
			if (!opcodes->LoadOpcodes(opfile.c_str())) {
				LogNetcode("[OPCODES] Error loading opcodes file [{}]. Not registering patch [{}]", opfile.c_str(), name);
				return;
			}
		}

		EQStreamInterface::Signature signature;
		std::string pname;

		// The Live client is told apart from TOB by its first packet: both opcode values and sizes differ.
		pname = std::string(name) + "_world";
		signature.ignore_eq_opcode = 0;
		signature.first_length = LIVE_LOGIN_INFO_SIZE;
		signature.first_eq_opcode = opcodes->EmuToEQ(OP_SendLoginInfo);
		into.RegisterPatch(signature, pname.c_str(), &opcodes, &struct_strategy);

		pname = std::string(name) + "_zone";
		signature.ignore_eq_opcode = opcodes->EmuToEQ(OP_AckPacket);
		signature.first_length = LIVE_CLIENT_ZONE_ENTRY_SIZE;
		signature.first_eq_opcode = opcodes->EmuToEQ(OP_ZoneEntry);
		into.RegisterPatch(signature, pname.c_str(), &opcodes, &struct_strategy);
		LogNetcode("[StreamIdentify] Registered patch [{}]", name);
	}

	void Reload()
	{
		if (opcodes != nullptr) {
			std::string opfile = fmt::format("{}/patch_{}.conf", PathManager::Instance()->GetPatchPath(), name);
			if (!opcodes->ReloadOpcodes(opfile.c_str())) {
				LogNetcode("[OPCODES] Error reloading opcodes file [{}] for patch [{}]", opfile.c_str(), name);
				return;
			}
			LogNetcode("[OPCODES] Reloaded opcodes for patch [{}]", name);
		}
	}

	bool IsLiveStream(const std::shared_ptr<EQStreamInterface>& dest)
	{
		return dest && opcodes != nullptr && dest->GetOpcodeManager() == opcodes;
	}

	Strategy::Strategy() : TOB::Strategy()
	{
		// TOB::Strategy filled in every TOB encoder/decoder; replace the ones whose Live layout differs.
#include "ss_register.h"
#include "live_ops.h"
	}

	std::string Strategy::Describe() const
	{
		std::string r;
		r += "Patch ";
		r += name;
		return(r);
	}

	const EQ::versions::ClientVersion Strategy::ClientVersion() const
	{
		return EQ::versions::ClientVersion::TOB;
	}

#include "ss_define.h"

	// ENCODE/DECODE methods for the packets listed in live_ops.h

	// The records follow the TOB client's achievement files. Live ships a different achievement list and would
	// misread them, so it gets none.
	ENCODE(OP_AchievementCompare)
	{
		delete *p;
		*p = nullptr;
	}

	ENCODE(OP_AchievementLoadFiles) { ENCODE_FORWARD(OP_AchievementCompare); }
	ENCODE(OP_AchievementRequestTrigger) { ENCODE_FORWARD(OP_AchievementCompare); }

	ENCODE(OP_CashReward)
	{
		ENCODE_LENGTH_EXACT(CashReward_Struct);
		SETUP_DIRECT_ENCODE(CashReward_Struct, structs::CashReward_Struct);

		eq->copper = emu->copper;
		eq->silver = emu->silver;
		eq->gold = emu->gold;
		eq->platinum = emu->platinum;

		FINISH_ENCODE();
	}

	ENCODE(OP_LevelUpdate)
	{
		ENCODE_LENGTH_EXACT(LevelUpdate_Struct);
		SETUP_DIRECT_ENCODE(LevelUpdate_Struct, structs::LevelUpdate_Struct);

		eq->level = emu->level;
		eq->level_old = emu->level_old;
		eq->exp = emu->exp;
		eq->flag = 0;

		FINISH_ENCODE();
	}

	ENCODE(OP_Sound)
	{
		EQApplicationPacket* in = *p;
		*p = nullptr;

		if (in->size != sizeof(QuestReward_Struct)) {
			LogNetcode("Live::ENCODE(OP_Sound) unexpected size [{}], dropping", in->size);
			delete in;
			return;
		}

		auto emu = reinterpret_cast<QuestReward_Struct*>(in->pBuffer);

		// unused slots are zero (or -1), the Live client takes a count instead
		uint32 item_count = 0;
		for (uint32 i = 0; i < QUESTREWARD_COUNT && item_count < structs::QUEST_REWARD_MAX_ITEMS; ++i) {
			if (emu->item_id[i] > 0) {
				++item_count;
			}
		}

		SerializeBuffer buf(structs::QUEST_REWARD_HEADER_SIZE + item_count * sizeof(uint32));
		buf.WriteUInt32(emu->mob_id);
		buf.WriteUInt32(emu->target_id);
		buf.WriteUInt32(emu->exp_reward);
		buf.WriteUInt32(emu->faction);
		buf.WriteUInt32(static_cast<uint32>(emu->faction_mod));
		buf.WriteUInt64(emu->copper);
		buf.WriteUInt64(emu->silver);
		buf.WriteUInt64(emu->gold);
		buf.WriteUInt64(emu->platinum);
		buf.WriteUInt32(item_count);
		for (uint32 i = 0, written = 0; i < QUESTREWARD_COUNT && written < item_count; ++i) {
			if (emu->item_id[i] > 0) {
				buf.WriteUInt32(static_cast<uint32>(emu->item_id[i]));
				++written;
			}
		}

		auto outapp = new EQApplicationPacket(OP_Sound, buf.size());
		outapp->WriteData(buf.buffer(), buf.size());
		dest->FastQueuePacket(&outapp, ack_req);
		delete in;
	}

	DECODE(OP_ClientUpdate)
	{
		// the client's send buffer can be longer than the struct
		DECODE_LENGTH_ATLEAST(structs::PlayerPositionUpdateClient_Struct);
		SETUP_DIRECT_DECODE(PlayerPositionUpdateClient_Struct, structs::PlayerPositionUpdateClient_Struct);

		IN(spawn_id);
		IN(vehicle_id);
		IN(sequence);
		emu->x_pos = eq->position.x;
		emu->y_pos = eq->position.y;
		emu->z_pos = eq->position.z;
		emu->heading = eq->position.heading;
		emu->delta_x = eq->position.delta_x;
		emu->delta_y = eq->position.delta_y;
		emu->delta_z = eq->position.delta_z;
		emu->delta_heading = eq->position.delta_heading;
		emu->animation = eq->position.animation;

		FINISH_DIRECT_DECODE();
	}

	DECODE(OP_Consider)
	{
		DECODE_LENGTH_EXACT(structs::Consider_Struct);
		SETUP_DIRECT_DECODE(Consider_Struct, structs::Consider_Struct);

		IN(playerid);
		IN(targetid);
		IN(faction);
		IN(level);

		FINISH_DIRECT_DECODE();
	}

	DECODE(OP_ConsiderCorpse) { DECODE_FORWARD(OP_Consider); }

	DECODE(OP_GuildDemote)
	{
		// Live sends 144 bytes: the TOB fields (two 64-byte names, rank) plus a zero u64 and padding
		if (__packet->size == 144) {
			__packet->size = sizeof(TOB::structs::GuildDemote_Struct);
		}
		TOB::Strategy::Decode_OP_GuildDemote(__packet);
	}

	DECODE(OP_ShopPlayerSell)
	{
		// Live appends 4 bytes (unused, often stack junk) to the TOB sell request; drop them and use the TOB decoder
		if (__packet->size == sizeof(TOB::structs::Merchant_Purchase_Request_Struct) + 4) {
			__packet->size = sizeof(TOB::structs::Merchant_Purchase_Request_Struct);
		}
		TOB::Strategy::Decode_OP_ShopPlayerSell(__packet);
	}

	DECODE(OP_SetServerFilter)
	{
		DECODE_LENGTH_EXACT(structs::SetServerFilter_Struct);
		SETUP_DIRECT_DECODE(SetServerFilter_Struct, structs::SetServerFilter_Struct);

		// the server only uses the first 29 filters
		for (int r = 0; r < 29; r++) {
			IN(filters[r]);
		}

		FINISH_DIRECT_DECODE();
	}

} /*Live*/
