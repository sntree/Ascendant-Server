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

#include "common/struct_strategy.h"
#include "common/patches/tob.h"

#include <memory>

class EQStreamIdentifier;
class EQStreamInterface;

// The current Live client (eqgame.exe 2026-09-17). It is the TOB client a year on: same login protocol, new
// opcode values (patch_Live.conf) and a handful of changed packet layouts. Everything else reuses the TOB
// encoders and decoders, and the server treats a Live client as a TOB client (ClientVersion::TOB).
namespace Live {

extern void Register(EQStreamIdentifier& into);
extern void Reload();

// True when dest speaks the Live opcode set. TOB encoders use this to pick the Live layout where it differs.
extern bool IsLiveStream(const std::shared_ptr<EQStreamInterface>& dest);

class Strategy : public TOB::Strategy
{
public:
	Strategy();

protected:
	std::string Describe() const override;
	const EQ::versions::ClientVersion ClientVersion() const override;

//magic macro to declare our opcode processors
#include "ss_declare.h"
#include "live_ops.h"
};

}; /*Live*/
