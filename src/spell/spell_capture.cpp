//       _________ __                 __
//      /   _____//  |_____________ _/  |______     ____  __ __  ______
//      \_____  \\   __\_  __ \__  \\   __\__  \   / ___\|  |  \/  ___/
//      /        \|  |  |  | \// __ \|  |  / __ \_/ /_/  >  |  /\___ |
//     /_______  /|__|  |__|  (____  /__| (____  /\___  /|____//____  >
//             \/                  \/          \//_____/            \/
//  ______________________                           ______________________
//                        T H E   W A R   B E G I N S
//         Stratagus - A free fantasy real time strategy game engine
//
/**@name spell_capture.cpp - The spell Capture. */
//
//      (c) Copyright 1998-2012 by Vladi Belperchinov-Shabanski, Lutz Sammer,
//                                 Jimmy Salmon, and Joris DAUPHIN
//
//      This program is free software; you can redistribute it and/or modify
//      it under the terms of the GNU General Public License as published by
//      the Free Software Foundation; only version 2 of the License.
//
//      This program is distributed in the hope that it will be useful,
//      but WITHOUT ANY WARRANTY; without even the implied warranty of
//      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//      GNU General Public License for more details.
//
//      You should have received a copy of the GNU General Public License
//      along with this program; if not, write to the Free Software
//      Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
//      02111-1307, USA.
//

//@{

#include "stratagus.h"

#include "spell/spell_capture.h"

#include "../ai/ai_local.h"

#include "commands.h"
#include "game.h"
#include "script.h"
#include "unit.h"

void Spell_Capture::Parse(lua_State *l, int startIndex, int endIndex) /* override */
{
	for (int j = startIndex; j < endIndex; ++j) {
		const std::string_view value = LuaToString(l, -1, j + 1);
		++j;
		if (value == "sacrifice") {
			this->SacrificeEnable = true;
		} else if (value == "join-to-ai-force") {
			this->JoinToAIForce = true;
		} else if (value == "damage") {
			this->Damage = LuaToNumber(l, -1, j + 1);
		} else if (value == "percent") {
			this->DamagePercent = LuaToNumber(l, -1, j + 1);
		} else {
			LuaError(l, "Unsupported Capture tag: %s", value.data());
		}
	}
}

/**
**  Cast capture.
**
**  @param caster       Unit that casts the spell
**  @param spell        Spell-type pointer
**  @param target       Target unit that spell is addressed to
**  @param goalPos      coord of target spot when/if target does not exist
**
**  @return             =!0 if spell should be repeated, 0 if not
*/
int Spell_Capture::Cast(CUnit &caster,
                        const SpellType &spell,
                        CUnit *&target,
                        const Vec2i & /*goalPos*/) /* override */
{
	if (!target || caster.Player == target->Player) {
		return 0;
	}

	if (this->DamagePercent) {
		if ((100 * target->Variable[HP_INDEX].Value) /
			target->Variable[HP_INDEX].Max > this->DamagePercent &&
			target->Variable[HP_INDEX].Value > this->Damage) {
			HitUnit(&caster, *target, this->Damage);
			if (this->SacrificeEnable) {
				// No corpse.
				caster.Remove(nullptr);
				caster.Release();
			}
			return 1;
		}
	}
	caster.Player->Score += target->Variable[POINTS_INDEX].Value;
	if (caster.IsEnemy(*target)) {
		if (target->Type->Building) {
			caster.Player->TotalRazings++;
		} else {
			caster.Player->TotalKills++;
		}
		if (UseHPForXp) {
			caster.Variable[XP_INDEX].Max += target->Variable[HP_INDEX].Value;
		} else {
			caster.Variable[XP_INDEX].Max += target->Variable[POINTS_INDEX].Value;
		}
		caster.Variable[XP_INDEX].Value = caster.Variable[XP_INDEX].Max;
		caster.Variable[KILL_INDEX].Value++;
		caster.Variable[KILL_INDEX].Max++;
		caster.Variable[KILL_INDEX].Enable = 1;
	}
	target->ChangeOwner(*caster.Player);
	UnitClearOrders(*target);
	if (this->JoinToAIForce && caster.Player->AiEnabled) {
		int force = caster.Player->Ai->Force.GetForce(caster);
		if (force != -1) {
			caster.Player->Ai->Force[force].Insert(*target);
			target->GroupId = caster.GroupId;
			CommandDefend(*target, caster, FlushCommands);
		}
	}
	if (this->SacrificeEnable) {
		// No corpse.
		caster.Remove(nullptr);
		caster.Release();
	} else {
		caster.Variable[MANA_INDEX].Value -= spell.ManaCost;
	}
	return 0;
}

//@}
