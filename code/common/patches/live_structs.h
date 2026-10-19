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

#pragma once

#include "common/types.h"
#include "common/patches/tob_structs.h"

namespace Live {
namespace structs {

// Live layouts that differ from TOB::structs. Unchanged structs are used from TOB::structs directly.

#pragma pack(1)

// 20-byte position block used by OP_ClientUpdate (server to client) and by every spawn record.
// Same fields as TOB::structs::Spawn_Struct_Position, different bit positions. Pad bits are written as 0.
struct Spawn_Struct_Position
{
	union {
		struct {
			// dword 0
			signed   deltaHeading : 10;
			signed   x : 19;
			unsigned pad0 : 3;
			// dword 1
			signed   deltaZ : 13;
			signed   deltaX : 13;
			unsigned pad1 : 6;
			// dword 2
			unsigned heading : 12;
			signed   deltaY : 13;
			unsigned pad2 : 7;
			// dword 3
			signed   z : 19;
			signed   animation : 10;
			unsigned pad3 : 3;
			// dword 4
			signed   y : 19;
			unsigned pitch : 12;
			unsigned pad4 : 1;
		};
		uint32 raw[5];
	};
};
static_assert(sizeof(Spawn_Struct_Position) == 20, "Live Spawn_Struct_Position must be 20 bytes");

struct PlayerPositionUpdateServer_Struct
{
	/*00*/ uint16 spawn_id;
	/*02*/ uint16 vehicle_id;
	/*04*/ Spawn_Struct_Position position;
	/*24*/
};
static_assert(sizeof(PlayerPositionUpdateServer_Struct) == 24, "Live PlayerPositionUpdateServer_Struct must be 24 bytes");

// Position block sent by the client. The padding bits of the three bitfield dwords are not initialised by the
// client, so only the named fields are meaningful.
struct Client_Position
{
	/*0x00*/ float y;
	/*0x04*/ float delta_z;
	/*0x08*/ unsigned heading : 12;
	         unsigned padding08 : 20;
	/*0x0c*/ float x;
	/*0x10*/ float z;
	/*0x14*/ signed   delta_heading : 10;
	         signed   animation : 10;
	         unsigned padding14 : 12;
	/*0x18*/ float delta_y;
	/*0x1c*/ float delta_x;
	/*0x20*/ unsigned pitch : 12;
	         unsigned padding20 : 20;
	/*0x24*/
};
static_assert(sizeof(Client_Position) == 36, "Live Client_Position must be 36 bytes");

struct PlayerPositionUpdateClient_Struct
{
	/*00*/ uint16 sequence;
	/*02*/ uint16 spawn_id;
	/*04*/ uint16 vehicle_id;
	/*06*/ Client_Position position;
	/*42*/
};
static_assert(sizeof(PlayerPositionUpdateClient_Struct) == 42, "Live PlayerPositionUpdateClient_Struct must be 42 bytes");

// OP_Consider and OP_ConsiderCorpse, both directions. TOB's struct plus 8 bytes at the end.
struct Consider_Struct {
	/*000*/ uint32 playerid;
	/*004*/ uint32 targetid;
	/*008*/ uint32 faction;
	/*012*/ uint32 level;
	/*016*/ uint32 report_mode;
	/*020*/ uint8  rare_creature;
	/*021*/ uint8  loot_locked;
	/*022*/ uint8  unknown022;
	/*023*/ uint8  unknown023;
	/*024*/ uint32 unknown024;	// client sends 0xFFFFFFFF, ignores it when received
	/*028*/ uint8  unknown028;
	/*029*/ uint8  unknown029[3];
	/*032*/
};
static_assert(sizeof(Consider_Struct) == 32, "Live Consider_Struct must be 32 bytes");

// OP_SetServerFilter: one more filter than TOB (70 instead of 69). Only filters[0..28] are used.
struct SetServerFilter_Struct {
	uint32 filters[70];
};
static_assert(sizeof(SetServerFilter_Struct) == 280, "Live SetServerFilter_Struct must be 280 bytes");

// OP_SendMaxCharacters: TOB's struct with one extra u32 (a heroic count) inserted at +0x1c, so every later field
// moves up by 4. The encoder builds the TOB struct and inserts the new field, this is the resulting size.
constexpr size_t MAX_CHARACTERS_SIZE = sizeof(TOB::structs::MaxCharacters_Struct) + 4;
constexpr size_t MAX_CHARACTERS_NEW_FIELD_OFFSET = 0x1c;
static_assert(MAX_CHARACTERS_SIZE == 64, "Live OP_SendMaxCharacters must be 64 bytes");

// OP_SendMembership: TOB's layout with 36 entries instead of 33.
struct Membership_Struct
{
	/*000*/ uint8  membership;
	/*001*/ uint32 races;
	/*005*/ uint32 classes;
	/*009*/ uint32 entrysize;
	/*013*/ int32  entries[36];
	/*157*/
};
static_assert(sizeof(Membership_Struct) == 157, "Live Membership_Struct must be 157 bytes");

// OP_SpawnDoor record: TOB's Door_Struct with Id widened to uint32 at +80, every later field moves up by 4.
struct Door_Struct
{
	/*000*/ char   name[32];
	/*032*/ float  DefaultY;
	/*036*/ float  DefaultX;
	/*040*/ float  DefaultZ;
	/*044*/ float  DefaultHeading;
	/*048*/ uint32 DefaultDoorAngle;
	/*052*/ float  Y;
	/*056*/ float  X;
	/*060*/ float  Z;
	/*064*/ float  Heading;
	/*068*/ float  DoorAngle;
	/*072*/ uint16 ScaleFactor;
	/*074*/ uint8  padding074[2];
	/*076*/ uint32 Unknown76;
	/*080*/ uint32 Id;
	/*084*/ uint8  Type;
	/*085*/ uint8  State;
	/*086*/ uint8  DefaultState;
	/*087*/ uint8  padding087;
	/*088*/ int32  Param;
	/*092*/ uint32 AdventureDoorId;
	/*096*/ uint32 DynDoorID;
	/*100*/ uint32 RealEstateDoorID;
	/*104*/ uint8  bHasScript;
	/*105*/ uint8  bUsable;
	/*106*/ uint8  bRemainOpen;
	/*107*/ uint8  bVisible;
	/*108*/ uint8  bHeadingChanged;
	/*109*/ uint8  padding109[3];
	/*112*/ float  TopSpeed1;
	/*116*/ float  TopSpeed2;
	/*120*/ uint8  bNeedsTimeStampSet;
	/*121*/ uint8  padding121[3];
	/*124*/ float  unknownFloat1;
	/*128*/ float  unknownFloat2;
	/*132*/ uint8  unknownByte1;
	/*133*/ uint8  padding133[3];
	/*136*/
};
static_assert(sizeof(Door_Struct) == 136, "Live Door_Struct must be 136 bytes");

// OP_MoveDoor: TOB sends a uint8 id and a uint8 action; Live reads a uint32 id and the action byte at +4.
struct MoveDoor_Struct
{
	/*00*/ uint32 doorid;
	/*04*/ uint8  action;
	/*05*/ uint8  padding005[3];
	/*08*/
};
static_assert(sizeof(MoveDoor_Struct) == 8, "Live MoveDoor_Struct must be 8 bytes");

// OP_MoneyOnCorpse: TOB's struct with the four coin fields widened to uint64.
struct moneyOnCorpseStruct {
	/*000*/ uint8  type;
	/*001*/ uint8  padding1[3];
	/*004*/ uint32 flags;
	/*008*/ uint64 platinum;
	/*016*/ uint64 gold;
	/*024*/ uint64 silver;
	/*032*/ uint64 copper;
	/*040*/
};
static_assert(sizeof(moneyOnCorpseStruct) == 40, "Live moneyOnCorpseStruct must be 40 bytes");

// OP_Sound (quest reward window): TOB sends the 68-byte QuestReward_Struct raw. Live reads a stream:
// u32 mob_id, target_id, exp_reward, faction, faction_mod, u64 copper, silver, gold, platinum,
// u32 item_count, then item_count u32 item ids (the client keeps at most 8).
constexpr uint32 QUEST_REWARD_HEADER_SIZE = 5 * sizeof(uint32) + 4 * sizeof(uint64) + sizeof(uint32);
constexpr uint32 QUEST_REWARD_MAX_ITEMS   = 8;
static_assert(QUEST_REWARD_HEADER_SIZE == 56, "Live quest reward header must be 56 bytes");

// OP_CashReward: the four coin fields are uint64 (TOB: uint32).
struct CashReward_Struct
{
	/*000*/ uint64 copper;
	/*008*/ uint64 silver;
	/*016*/ uint64 gold;
	/*024*/ uint64 platinum;
	/*032*/
};
static_assert(sizeof(CashReward_Struct) == 32, "Live CashReward_Struct must be 32 bytes");

// OP_LevelUpdate: exp is a uint64 and Live reads a flag byte at +16 (sent as 0).
struct LevelUpdate_Struct
{
	/*000*/ uint32 level;
	/*004*/ uint32 level_old;
	/*008*/ uint64 exp;
	/*016*/ uint8  flag;
	/*017*/ uint8  padding017[3];
	/*020*/
};
static_assert(sizeof(LevelUpdate_Struct) == 20, "Live LevelUpdate_Struct must be 20 bytes");

#pragma pack()

}; /*structs*/

// First-packet sizes the stream identifier uses to recognise a Live client (bytes after the opcode).
constexpr uint32 LIVE_CLIENT_ZONE_ENTRY_SIZE = sizeof(TOB::structs::ClientZoneEntry_Struct); // 92, unchanged; the opcode tells Live apart
constexpr uint32 LIVE_LOGIN_INFO_SIZE = sizeof(TOB::structs::LoginInfo_Struct);           // 464, unchanged

namespace structs {

}; /*structs*/
}; /*Live*/
