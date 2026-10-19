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

// Packets whose Live layout differs from TOB. Each one here replaces the TOB encoder/decoder for Live clients;
// everything not listed uses TOB's.

E(OP_AchievementCompare)
E(OP_AchievementLoadFiles)
E(OP_AchievementRequestTrigger)
E(OP_CashReward)
E(OP_LevelUpdate)
E(OP_Sound)

D(OP_ClientUpdate)
D(OP_Consider)
D(OP_ConsiderCorpse)
D(OP_SetServerFilter)
D(OP_GuildDemote)
D(OP_ShopPlayerSell)
