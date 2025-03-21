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
/**@name script_unit.cpp - The unit ccl functions. */
//
//      (c) Copyright 2001-2015 by Lutz Sammer, Jimmy Salmon and Andrettin
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

/*----------------------------------------------------------------------------
--  Includes
----------------------------------------------------------------------------*/

#include "stratagus.h"
#include "unit.h"

#include "actions.h"
#include "animation.h"
#include "commands.h"
#include "construct.h"
#include "interface.h"
#include "map.h"
#include "netconnect.h"
#include "network.h"
#include "pathfinder.h"
#include "player.h"
#include "script.h"
#include "spells.h"
#include "trigger.h"
#include "unit_find.h"
#include "unit_manager.h"
#include "unittype.h"
#include "upgrade.h"

/*----------------------------------------------------------------------------
--  Variables
----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------
--  Functions
----------------------------------------------------------------------------*/

/// Get resource by name
extern unsigned CclGetResourceByName(lua_State *l);

/**
** <b>Description</b>
**
**  Set training queue
**
**  @param l  Lua state.
**
**  @return  The old state of the training queue
**
** Example:
**
** <div class="example"><code>-- Training queue available. Train multiple units.
**		<strong>SetTrainingQueue</strong>(true)
**		-- Train one unit at a time.
**		<strong>SetTrainingQueue</strong>(false)</code></div>
*/
static int CclSetTrainingQueue(lua_State *l)
{
	LuaCheckArgs(l, 1);
	EnableTrainingQueue = LuaToBoolean(l, 1);
	return 0;
}

/**
**  Set capture buildings
**
**  @param l  Lua state.
**
**  @return   The old state of the flag
**
** Example:
**
** <div class="example"><code><strong>SetBuildingCapture</strong>(true)
**	<strong>SetBuildingCapture</strong>(false)</code></div>
*/
static int CclSetBuildingCapture(lua_State *l)
{
	LuaCheckArgs(l, 1);
	EnableBuildingCapture = LuaToBoolean(l, 1);
	return 0;
}

/**
**  Set reveal attacker
**
**  @param l  Lua state.
**
**  @return   The old state of the flag
**
** Example:
**
** <div class="example"><code><strong>SetRevealAttacker</strong>(true)
**	<strong>SetRevealAttacker</strong>(false)</code></div>
*/
static int CclSetRevealAttacker(lua_State *l)
{
	LuaCheckArgs(l, 1);
	RevealAttacker = LuaToBoolean(l, 1);
	return 0;
}

/**
**  Set cost multiplier to RepairCost for buildings additional workers helping (0 = no additional cost)
**
**  @param l  Lua state.
**
** Example:
**
** <div class="example"><code>-- No cost
**	<strong>ResourcesMultiBuildersMultiplier</strong>(0)
**	-- Each builder helping will cost 1 resource
**	<strong>ResourcesMultiBuildersMultiplier</strong>(1)
**	-- Each builder helping will cost 10 resource
**	<strong>ResourcesMultiBuildersMultiplier</strong>(10)</code></div>
*/
static int CclResourcesMultiBuildersMultiplier(lua_State *l)
{
	LuaCheckArgs(l, 1);
	ResourcesMultiBuildersMultiplier = LuaToNumber(l, 1);
	return 0;
}

/**
**  Get a unit pointer
**
**  @param l  Lua state.
**
**  @return   The unit pointer
*/
static CUnit *CclGetUnit(lua_State *l)
{
	int num = LuaToNumber(l, -1);
	if (num == -1) {
		if (!Selected.empty()) {
			return Selected[0];
		} else if (UnitUnderCursor) {
			return UnitUnderCursor;
		}
		return nullptr;
	}
	return &UnitManager->GetSlotUnit(num);
}

/**
**  Get a unit pointer from ref string
**
**  @param l  Lua state.
**
**  @return   The unit pointer
*/
CUnit *CclGetUnitFromRef(lua_State *l)
{
	const std::string_view value = LuaToString(l, -1);
	unsigned int slot = to_number(value.substr(1), 16);
	Assert(slot < UnitManager->GetUsedSlotCount());
	return &UnitManager->GetSlotUnit(slot);
}


bool COrder::ParseGenericData(lua_State *l, int &j, std::string_view value)
{
	if (value == "finished") {
		this->Finished = true;
	} else if (value == "goal") {
		++j;
		lua_rawgeti(l, -1, j + 1);
		this->SetGoal(CclGetUnitFromRef(l));
		lua_pop(l, 1);
	} else {
		return false;
	}
	return true;
}



void PathFinderInput::Load(lua_State *l)
{
	if (!lua_istable(l, -1)) {
		LuaError(l, "incorrect argument in PathFinderInput::Load");
	}
	const int args = 1 + lua_rawlen(l, -1);
	for (int i = 1; i < args; ++i) {
		const std::string_view tag = LuaToString(l, -1, i);
		++i;
		if (tag == "unit-size") {
			lua_rawgeti(l, -1, i);
			CclGetPos(l, &this->unitSize);
			lua_pop(l, 1);
		} else if (tag == "goalpos") {
			lua_rawgeti(l, -1, i);
			CclGetPos(l, &this->goalPos);
			lua_pop(l, 1);
		} else if (tag == "goal-size") {
			lua_rawgeti(l, -1, i);
			CclGetPos(l, &this->goalSize);
			lua_pop(l, 1);
		} else if (tag == "minrange") {
			this->minRange = LuaToNumber(l, -1, i);
		} else if (tag == "maxrange") {
			this->maxRange = LuaToNumber(l, -1, i);
		} else if (tag == "invalid") {
			this->isRecalculatePathNeeded = true;
			--i;
		} else {
			LuaError(l, "PathFinderInput::Load: Unsupported tag: %s", tag.data());
		}
	}
}


void PathFinderOutput::Load(lua_State *l)
{
	if (!lua_istable(l, -1)) {
		LuaError(l, "incorrect argument in PathFinderOutput::Load");
	}
	const int args = 1 + lua_rawlen(l, -1);
	for (int i = 1; i < args; ++i) {
		const std::string_view tag = LuaToString(l, -1, i);
		++i;
		if (tag == "cycles") {
			this->Cycles = LuaToNumber(l, -1, i);
		} else if (tag == "fast") {
			this->Fast = LuaToNumber(l, -1, i);
		} else if (tag == "overflow-length") {
			this->OverflowLength = LuaToNumber(l, -1, i);
		} else if (tag == "path") {
			lua_rawgeti(l, -1, i);
			if (!lua_istable(l, -1)) {
				LuaError(l, "incorrect argument _");
			}
			const int subargs = lua_rawlen(l, -1);
			if (subargs <= PathFinderOutput::MAX_PATH_LENGTH)
			{
				for (int k = 0; k < subargs; ++k) {
					this->Path[k] = LuaToNumber(l, -1, k + 1);
				}
				this->Length = subargs;
			}
			lua_pop(l, 1);
		} else {
			LuaError(l, "PathFinderOutput::Load: Unsupported tag: %s", tag.data());
		}
	}
}

/**
**  Parse orders.
**
**  @param l     Lua state.
**  @param unit  Unit pointer which should get the orders.
*/
static void CclParseOrders(lua_State *l, CUnit &unit)
{
	unit.Orders.clear();
	const int n = lua_rawlen(l, -1);

	for (int j = 0; j < n; ++j) {
		lua_rawgeti(l, -1, j + 1);

		unit.Orders.push_back(CclParseOrder(l, unit));
		lua_pop(l, 1);
	}
}

/**
**  Parse unit
**
**  @param l  Lua state.
**
**  @todo  Verify that vision table is always correct (transporter)
**  @todo (PlaceUnit() and host-info).
**
** Example:
**
** <div class="example"><code>footman = CreateUnit("unit-footman", 0, {0, 1})
**	-- The unit will appear selected
**	<strong>Unit</strong>(footman,{"selected"})
**	-- The unit will be considered destroyed
**	<strong>Unit</strong>(footman,{"destroyed"})
**	-- The unit will be considered removed
**	<strong>Unit</strong>(footman,{"removed"})
**	-- The unit will be considered as a summoned unit
**	<strong>Unit</strong>(footman,{"summoned",500})
**	-- The unit will face on south
**	<strong>Unit</strong>(footman,{"direction",0})
**	-- The unit will be displayed with his 3rd frame
**	<strong>Unit</strong>(footman,{"frame", 3})
**	-- The footman will have a high sight
**	<strong>Unit</strong>(footman,{"current-sight-range",9})
**	-- Change the unit color to be the ones from player 1
**	<strong>Unit</strong>(footman,{"rescued-from",1})</code></div>
*/
static int CclUnit(lua_State *l)
{
	const int slot = LuaToNumber(l, 1);

	if (!lua_istable(l, 2)) {
		LuaError(l, "incorrect argument");
	}

	CUnit *unit = &UnitManager->GetSlotUnit(slot);
	bool hadType = unit->Type != nullptr;
	CUnitType *type = nullptr;
	CUnitType *seentype = nullptr;
	CPlayer *player = nullptr;

	// Parse the list:
	const int args = lua_rawlen(l, 2);
	for (int j = 0; j < args; ++j) {
		const std::string_view value = LuaToString(l, 2, j + 1);
		++j;

		if (value == "type") {
			type = &UnitTypeByIdent(LuaToString(l, 2, j + 1));
		} else if (value == "seen-type") {
			seentype = &UnitTypeByIdent(LuaToString(l, 2, j + 1));
		} else if (value == "player") {
			player = &Players[LuaToNumber(l, 2, j + 1)];

			// During a unit's death animation (when action is "die" but the
			// unit still has its original type, i.e. it's still not a corpse)
			// the unit is already removed from map and from player's
			// unit list (=the unit went through LetUnitDie() which
			// calls RemoveUnit() and UnitLost()).  Such a unit should not
			// be put on player's unit list!  However, this state is not
			// easily detected from this place.  It seems that it is
			// characterized by
			// unit->CurrentAction()==UnitAction::Die so we have to wait
			// until we parsed at least Unit::Orders[].
			Assert(type);
			unit->Init(*type);
			unit->Seen.Type = seentype;
			unit->Active = 0;
			unit->Removed = 0;
			Assert(UnitNumber(*unit) == slot);
		} else if (value == "current-sight-range") {
			unit->CurrentSightRange = LuaToNumber(l, 2, j + 1);
		} else if (value == "refs") {
			unit->Refs = LuaToNumber(l, 2, j + 1);
		} else if (value == "host-info") {
			lua_rawgeti(l, 2, j + 1);
			if (!lua_istable(l, -1) || lua_rawlen(l, -1) != 4) {
				LuaError(l, "incorrect argument");
			}
			Vec2i pos;
			int w;
			int h;

			pos.x = LuaToNumber(l, -1, 1);
			pos.y = LuaToNumber(l, -1, 2);
			w = LuaToNumber(l, -1, 3);
			h = LuaToNumber(l, -1, 4);
			MapSight(*player, *unit, pos, w, h, unit->CurrentSightRange, MapMarkTileSight);
			// Detectcloak works in container
			if (unit->Type->BoolFlag[DETECTCLOAK_INDEX].value) {
				MapSight(*player, *unit, pos, w, h, unit->CurrentSightRange, MapMarkTileDetectCloak);
			}
			// Radar(Jammer) not.
			lua_pop(l, 1);
		} else if (value == "tile") {
			lua_rawgeti(l, 2, j + 1);
			CclGetPos(l, &unit->tilePos, -1);
			lua_pop(l, 1);
			unit->Offset = Map.getIndex(unit->tilePos);
		} else if (value == "seen-tile") {
			lua_rawgeti(l, 2, j + 1);
			CclGetPos(l, &unit->Seen.tilePos, -1);
			lua_pop(l, 1);
		} else if (value == "stats") {
			unit->Stats = &type->Stats[LuaToNumber(l, 2, j + 1)];
		} else if (value == "pixel") {
			lua_rawgeti(l, 2, j + 1);
			CclGetPos(l, &unit->IX , &unit->IY, -1);
			lua_pop(l, 1);
		} else if (value == "seen-pixel") {
			lua_rawgeti(l, 2, j + 1);
			CclGetPos(l, &unit->Seen.IX , &unit->Seen.IY, -1);
			lua_pop(l, 1);
		} else if (value == "frame") {
			unit->Frame = LuaToNumber(l, 2, j + 1);
		} else if (value == "seen") {
			unit->Seen.Frame = LuaToNumber(l, 2, j + 1);
		} else if (value == "not-seen") {
			unit->Seen.Frame = UnitNotSeen;
			--j;
		} else if (value == "direction") {
			unit->Direction = LuaToNumber(l, 2, j + 1);
		} else if (value == "damage-type") {
			unit->DamagedType = LuaToNumber(l, 2, j + 1);
		} else if (value == "attacked") {
			// FIXME : unsigned long should be better handled
			unit->Attacked = LuaToNumber(l, 2, j + 1);
		} else if (value == "auto-repair") {
			unit->AutoRepair = 1;
			--j;
		} else if (value == "burning") {
			unit->Burning = 1;
			--j;
		} else if (value == "destroyed") {
			unit->Destroyed = 1;
			--j;
		} else if (value == "removed") {
			unit->Removed = 1;
			--j;
		} else if (value == "selected") {
			unit->Selected = 1;
			--j;
		} else if (value == "summoned") {
			// FIXME : unsigned long should be better handled
			unit->Summoned = LuaToNumber(l, 2, j + 1);
		} else if (value == "waiting") {
			unit->Waiting = 1;
			--j;
		} else if (value == "mine-low") {
			unit->MineLow = 1;
			--j;
		} else if (value == "rescued-from") {
			unit->RescuedFrom = &Players[LuaToNumber(l, 2, j + 1)];
		} else if (value == "seen-by-player") {
			const std::string_view s = LuaToString(l, 2, j + 1);
			unit->Seen.ByPlayer = 0;
			for (int i = 0; i < PlayerMax && s.size(); ++i) {
				if (s[i] == '-' || s[i] == '_' || s[i] == ' ') {
					unit->Seen.ByPlayer &= ~(1 << i);
				} else {
					unit->Seen.ByPlayer |= (1 << i);
				}
			}
		} else if (value == "seen-destroyed") {
			const std::string_view s = LuaToString(l, 2, j + 1);
			unit->Seen.Destroyed = 0;
			for (int i = 0; i < PlayerMax && s.size(); ++i) {
				if (s[i] == '-' || s[i] == '_' || s[i] == ' ') {
					unit->Seen.Destroyed &= ~(1 << i);
				} else {
					unit->Seen.Destroyed |= (1 << i);
				}
			}
		} else if (value == "constructed") {
			unit->Constructed = 1;
			--j;
		} else if (value == "seen-constructed") {
			unit->Seen.Constructed = 1;
			--j;
		} else if (value == "seen-state") {
			unit->Seen.State = LuaToNumber(l, 2, j + 1);
		} else if (value == "active") {
			unit->Active = 1;
			--j;
		} else if (value == "ttl") {
			// FIXME : unsigned long should be better handled
			unit->TTL = LuaToNumber(l, 2, j + 1);
		} else if (value == "threshold") {
			// FIXME : unsigned long should be better handled
			unit->Threshold = LuaToNumber(l, 2, j + 1);
		} else if (value == "group-id") {
			unit->GroupId = LuaToNumber(l, 2, j + 1);
		} else if (value == "last-group") {
			unit->LastGroup = LuaToNumber(l, 2, j + 1);
		} else if (value == "resources-held") {
			unit->ResourcesHeld = LuaToNumber(l, 2, j + 1);
		} else if (value == "current-resource") {
			lua_rawgeti(l, 2, j + 1);
			lua_pushvalue(l, -1);
			unit->CurrentResource = CclGetResourceByName(l);
			lua_pop(l, 1);
		} else if (value == "pathfinder-input") {
			lua_rawgeti(l, 2, j + 1);
			lua_pushvalue(l, -1);
			unit->pathFinderData->input.Load(l);
			lua_pop(l, 1);
		} else if (value == "pathfinder-output") {
			lua_rawgeti(l, 2, j + 1);
			lua_pushvalue(l, -1);
			unit->pathFinderData->output.Load(l);
			lua_pop(l, 1);
		} else if (value == "wait") {
			unit->Wait = LuaToNumber(l, 2, j + 1);
		} else if (value == "anim-data") {
			lua_rawgeti(l, 2, j + 1);
			CAnimations::LoadUnitAnim(l, *unit, -1);
			lua_pop(l, 1);
		} else if (value == "wait-anim-data") {
			lua_rawgeti(l, 2, j + 1);
			CAnimations::LoadWaitUnitAnim(l, *unit, -1);
			lua_pop(l, 1);
		} else if (value == "blink") {
			unit->Blink = LuaToNumber(l, 2, j + 1);
		} else if (value == "moving") {
			unit->Moving = 1;
			--j;
		} else if (value == "moving-2") {
			unit->Moving = 2;
			--j;
		} else if (value == "moving-3") {
			unit->Moving = 3;
			--j;
		} else if (value == "re-cast") {
			unit->ReCast = 1;
			--j;
		} else if (value == "boarded") {
			unit->Boarded = 1;
			--j;
		} else if (value == "next-worker") {
			LuaError(l, "Unsupported old savegame");
		} else if (value == "resource-workers") {
			lua_rawgeti(l, 2, j + 1);
			if (!lua_istable(l, -1)) {
				LuaError(l, "incorrect argument");
			}
			const int subargs = lua_rawlen(l, -1);
			for (int k = 0; k < subargs; ++k) {
				lua_rawgeti(l, -1, k + 1);
				CUnit *u = CclGetUnitFromRef(l);
				lua_pop(l, 1);
				unit->Resource.AssignedWorkers.push_back(u);
			}
			lua_pop(l, 1);
		} else if (value == "resource-assigned") {
			LuaError(l, "Unsupported old savegame");
		} else if (value == "resource-active") {
			lua_rawgeti(l, 2, j + 1);
			lua_pushvalue(l, -1);
			unit->Resource.Active = LuaToNumber(l, -1);
			lua_pop(l, 1);
		} else if (value == "units-boarded-count") {
			unit->BoardCount = LuaToNumber(l, 2, j + 1);
		} else if (value == "units-contained") {
			int subargs;
			int k;
			lua_rawgeti(l, 2, j + 1);
			if (!lua_istable(l, -1)) {
				LuaError(l, "incorrect argument");
			}
			subargs = lua_rawlen(l, -1);
			for (k = 0; k < subargs; ++k) {
				lua_rawgeti(l, -1, k + 1);
				CUnit *u = CclGetUnitFromRef(l);
				lua_pop(l, 1);
				u->AddInContainer(*unit);
			}
			lua_pop(l, 1);
		} else if (value == "orders") {
			lua_rawgeti(l, 2, j + 1);
			lua_pushvalue(l, -1);
			CclParseOrders(l, *unit);
			lua_pop(l, 1);
			// now we know unit's action so we can assign it to a player
			Assert(player != nullptr);
			unit->AssignToPlayer(*player);
			if (unit->CurrentAction() == UnitAction::Built) {
				LuaDebugPrint(l, "HACK: the building is not ready yet\n");
				// HACK: the building is not ready yet
				unit->Player->UnitTypesCount[type->Slot]--;
				if (unit->Active) {
					unit->Player->UnitTypesAiActiveCount[type->Slot]--;
				}
			}
		} else if (value == "critical-order") {
			lua_rawgeti(l, 2, j + 1);
			lua_pushvalue(l, -1);
			unit->CriticalOrder = CclParseOrder(l, *unit);
			lua_pop(l, 1);
		} else if (value == "saved-order") {
			lua_rawgeti(l, 2, j + 1);
			lua_pushvalue(l, -1);
			unit->SavedOrder = CclParseOrder(l, *unit);
			lua_pop(l, 1);
		} else if (value == "new-order") {
			lua_rawgeti(l, 2, j + 1);
			lua_pushvalue(l, -1);
			unit->NewOrder = CclParseOrder(l, *unit);
			lua_pop(l, 1);
		} else if (value == "goal") {
			unit->Goal = &UnitManager->GetSlotUnit(LuaToNumber(l, 2, j + 1));
		} else if (value == "auto-cast") {
			const std::string_view s = LuaToString(l, 2, j + 1);
			if (unit->AutoCastSpell.empty()) {
				unit->AutoCastSpell.resize(SpellTypeTable.size());
			}
			unit->AutoCastSpell[SpellTypeByIdent(s).Slot] = true;
		} else if (value == "spell-cooldown") {
			lua_rawgeti(l, 2, j + 1);
			if (!lua_istable(l, -1) || lua_rawlen(l, -1) != SpellTypeTable.size()) {
				LuaError(l, "incorrect argument");
			}
			if (unit->SpellCoolDownTimers.empty()) {
				unit->SpellCoolDownTimers.resize(SpellTypeTable.size());
			}
			for (size_t k = 0; k < SpellTypeTable.size(); ++k) {
				unit->SpellCoolDownTimers[k] = LuaToNumber(l, -1, k + 1);
			}
			lua_pop(l, 1);
		} else {
			const int index = UnitTypeVar.VariableNameLookup[value];// User variables
			if (index != -1) { // Valid index
				lua_rawgeti(l, 2, j + 1);
				DefineVariableField(l, &unit->Variable[index], -1);
				lua_pop(l, 1);
				continue;
			}
			LuaError(l, "Unit: Unsupported tag: %s", value.data());
		}
	}

	// Unit may not have been assigned to a player before now. If not,
	// do so now. It is only assigned earlier if we have orders.
	// for loading of units from a MAP, and not a savegame, we won't
	// have orders for those units.  They should appear here as if
	// they were just created.
	if (!unit->Player) {
		Assert(player);
		unit->AssignToPlayer(*player);
		UpdateForNewUnit(*unit, 0);
	}

	//  Revealers are units that can see while removed
	if (unit->Removed && unit->Type->BoolFlag[REVEALER_INDEX].value) {
		MapMarkUnitSight(*unit);
	}

	if (!hadType && unit->Container) {
		// this unit was assigned to a container before it had a type, so we
		// need to actually add it now, since only with a type do we know the
		// BoardSize it takes up in the container
		CUnit *host = unit->Container;
		unit->Container = nullptr;
		unit->AddInContainer(*host);
	}

	return 0;
}

/**
**  Move a unit on map, optionally with offset.
**
**  @param l  Lua state.
**
**  @return   Returns the slot number of the made placed.
**
** Example:
**
** <div class="example"><code>-- Create the unit
**	footman = CreateUnit("unit-footman", 0, {7, 4})
**	-- Move the unit to position 20 (x) and 10 (y)
**	<strong>MoveUnit</strong>(footman,{20,10})
**	-- Move the unit to position 15 (x) and 9 (y) + 4 (x) and 7 (y) pixels overlap into the next tile
**	<strong>MoveUnit</strong>(footman,{15,9},{4,7})
** </code></div>
*/
static int CclMoveUnit(lua_State *l)
{
	int nargs = lua_gettop(l);
	if (nargs < 2 || nargs > 3) {
		LuaError(l, "incorrect argument, expected 2 or 3 arguments");
	}

	lua_pushvalue(l, 1);
	CUnit *unit = CclGetUnit(l);
	lua_pop(l, 1);

	Vec2i ipos;
	CclGetPos(l, &ipos, 2);

	if (!unit->Removed) {
		unit->Remove(unit->Container);
	}

	if (UnitCanBeAt(*unit, ipos)) {
		unit->Place(ipos);
	} else {
		const int heading = SyncRand() % 256;

		unit->tilePos = ipos;
		DropOutOnSide(*unit, heading, nullptr);
	}

	if (nargs == 3) {
		CclGetPos(l, &ipos, 3);
		unit->IX = ipos.x;
		unit->IY = ipos.y;
	}

	lua_pushvalue(l, 1);
	return 1;
}

/**
** <b>Description</b>
**
**  Remove unit from the map.
**
**  @param l  Lua state.
**
**  @return   Returns 1.
**
** Example:
**
** <div class="example"><code>ogre = CreateUnit("unit-ogre", 0, {24, 89})
**
**		AddTrigger(
**  		function() return (GameCycle > 150) end,
**  		function()
**    			<strong>RemoveUnit</strong>(ogre)
**    			return false end -- end of function
**		)</code></div>
*/
static int CclRemoveUnit(lua_State *l)
{
	LuaCheckArgs(l, 1);
	lua_pushvalue(l, 1);
	CUnit *unit = CclGetUnit(l);
	lua_pop(l, 1);
	if (unit) {
		unit->Remove(nullptr);
		LetUnitDie(*unit);
	}
	lua_pushvalue(l, 1);
	return 1;
}

/**
** <b>Description</b>
**
**  Create a unit and place it on the map
**
**  @param l  Lua state.
**
**  @return   Returns the slot number of the made unit.
**
** Example:
**
** <div class="example"><code><strong>CreateUnit</strong>("unit-human-transport", 1, {94, 0})</code></div>
*/
static int CclCreateUnit(lua_State *l)
{
	LuaCheckArgs(l, 3);

	lua_pushvalue(l, 1);
	CUnitType *unittype = CclGetUnitType(l);
	if (unittype == nullptr) {
		LuaError(l, "Bad unittype");
	}
	lua_pop(l, 1);
	Vec2i ipos;
	CclGetPos(l, &ipos, 3);

	lua_pushvalue(l, 2);
	CPlayer *player = CclGetPlayer(l);
	lua_pop(l, 1);
	if (player == nullptr) {
		printf("CreateUnit: You cannot use \"any\" in create-unit, specify a player\n");
		LuaError(l, "bad player");
		return 0;
	}
	if (player->Type == PlayerTypes::PlayerNobody) {
		printf("CreateUnit: player %s does not exist\n", lua_tostring(l, 2));
		LuaError(l, "bad player");
		return 0;
	}
	CUnit *unit = MakeUnit(*unittype, player);
	if (unit == nullptr) {
		LuaDebugPrint(l, "Unable to allocate unit");
		return 0;
	} else {
		if (UnitCanBeAt(*unit, ipos)
			|| (unit->Type->Building && CanBuildUnitType(nullptr, *unit->Type, ipos, 0))) {
			unit->Place(ipos);
		} else {
			const int heading = SyncRand() % 256;

			unit->tilePos = ipos;
			DropOutOnSide(*unit, heading, nullptr);
		}
		UpdateForNewUnit(*unit, 0);

		if (unit->Type->OnReady) {
			unit->Type->OnReady(UnitNumber(*unit));
		}

		lua_pushnumber(l, UnitNumber(*unit));
		return 1;
	}
}

/**
** <b>Description</b>
**
**  'Upgrade' a unit in place to a unit of different type.
**
**  @param l  Lua state.
**
**  @return   Returns success.
**
** Example:
**
** <div class="example"><code>-- Make a peon for player 5
**		peon = CreateUnit("unit-peon", 5, {58, 9})
**		-- The peon will be trasformed into a Grunt
**		<strong>TransformUnit</strong>(peon,"unit-grunt")</code></div>
*/
static int CclTransformUnit(lua_State *l)
{
	lua_pushvalue(l, 1);
	CUnit *targetUnit = CclGetUnit(l);
	lua_pop(l, 1);
	lua_pushvalue(l, 2);
	CUnitType *unittype = CclGetUnitType(l);
	lua_pop(l, 1);
	if (unittype && targetUnit) {
		CommandUpgradeTo(*targetUnit, *unittype, EFlushMode::On, true);
	}
	lua_pushvalue(l, 1);
	return 1;
}

/**
** <b>Description</b>
**
**  Damages unit, additionally using another unit as first's attacker
**
**  @param l  Lua state.
**
**  @return   Returns the slot number of the made unit.
**
** Example:
**
** <div class="example"><code>-- Make a grunt for player 5
**		grunt = CreateUnit("unit-grunt", 5, {58, 8})
**		-- Damage the grunt with 15 points
**		<strong>DamageUnit</strong>(-1,grunt,15)</code></div>
*/
static int CclDamageUnit(lua_State *l)
{
	LuaCheckArgs(l, 3);

	const int attacker = LuaToNumber(l, 1);
	CUnit *attackerUnit = nullptr;
	if (attacker != -1) {
		attackerUnit = &UnitManager->GetSlotUnit(attacker);
	}
	lua_pushvalue(l, 2);
	CUnit *targetUnit = CclGetUnit(l);
	lua_pop(l, 1);
	const int damage = LuaToNumber(l, 3);
	HitUnit(attackerUnit, *targetUnit, damage);

	return 1;
}

/**
**  Set resources held by a unit
**
**  @param l  Lua state.
*/
static int CclSetResourcesHeld(lua_State *l)
{
	LuaCheckArgs(l, 2);

	if (lua_isnil(l, 1)) {
		return 0;
	}

	lua_pushvalue(l, 1);
	CUnit *unit = CclGetUnit(l);
	lua_pop(l, 1);
	const int value = LuaToNumber(l, 2);
	unit->ResourcesHeld = value;
	unit->Variable[GIVERESOURCE_INDEX].Value = value;
	unit->Variable[GIVERESOURCE_INDEX].Max = value;
	unit->Variable[GIVERESOURCE_INDEX].Enable = 1;

	return 0;
}

/**
**  Set teleport destination for teleporter unit
**
**  @param l  Lua state.
*/
static int CclSetTeleportDestination(lua_State *l)
{
	LuaCheckArgs(l, 2);

	lua_pushvalue(l, 1);
	CUnit *unit = CclGetUnit(l);
	lua_pop(l, 1);
	if (unit->Type->BoolFlag[TELEPORTER_INDEX].value == false) {
		LuaError(l, "Unit not a teleporter");
	}
	lua_pushvalue(l, 2);
	CUnit *dest = CclGetUnit(l);
	lua_pop(l, 1);
	if (dest->IsAliveOnMap()) {
		unit->Goal = dest;
	}

	return 0;
}

/**
** <b>Description</b>
**
**  Order a unit
**
**  @param l  Lua state.
**
**  OrderUnit(player, unit-type, start_loc, dest_loc, order)
**
** Example:
**
** <div class="example"><code>-- Move transport from position x=94,y=0 to x=80,y=9
**		<strong>OrderUnit</strong>(1,"unit-human-transport",{94,0},{80,9},"move")</code></div>
*/
static int CclOrderUnit(lua_State *l)
{
	LuaCheckArgs(l, 5);

	lua_pushvalue(l, 1);
	const auto unitPlayerValidator = TriggerGetPlayer(l);
	lua_pop(l, 1);
	lua_pushvalue(l, 2);
	const auto unitValidator = TriggerGetUnitType(l);
	lua_pop(l, 1);
	if (!lua_istable(l, 3)) {
		LuaError(l, "incorrect argument");
	}
	Vec2i pos1;
	pos1.x = LuaToNumber(l, 3, 1);
	pos1.y = LuaToNumber(l, 3, 2);

	Vec2i pos2;
	if (lua_rawlen(l, 3) == 4) {
		pos2.x = LuaToNumber(l, 3, 3);
		pos2.y = LuaToNumber(l, 3, 4);
	} else {
		pos2 = pos1;
	}
	if (!lua_istable(l, 4)) {
		LuaError(l, "incorrect argument");
	}
	Vec2i dpos1;
	Vec2i dpos2;
	dpos1.x = LuaToNumber(l, 4, 1);
	dpos1.y = LuaToNumber(l, 4, 2);
	if (lua_rawlen(l, 4) == 4) {
		dpos2.x = LuaToNumber(l, 4, 3);
		dpos2.y = LuaToNumber(l, 4, 4);
	} else {
		dpos2 = dpos1;
	}
	const std::string_view order = LuaToString(l, 5);
	std::vector<CUnit *> table = Select(pos1, pos2);
	for (CUnit *unit : table) {
		if (unitValidator(*unit) && unitPlayerValidator(*unit)) {
			if (order == "move") {
				CommandMove(*unit, (dpos1 + dpos2) / 2, EFlushMode::On);
			} else if (order == "stop") {
				CommandStopUnit(*unit); //Stop the unit
			} else if (order == "stand-ground") {
				CommandStandGround(*unit, EFlushMode::Off); //Stand and flush every order
			} else if (order == "attack") {
				CUnit *attack = TargetOnMap(*unit, dpos1, dpos2);
				CommandAttack(*unit, (dpos1 + dpos2) / 2, attack, EFlushMode::On);
			} else if (order == "explore") {
				CommandExplore(*unit, EFlushMode::On);
			} else if (order == "patrol") {
				CommandPatrolUnit(*unit, (dpos1 + dpos2) / 2, EFlushMode::On);
			} else {
				LuaError(l, "Unsupported order: %s", order.data());
			}
		}
	}
	return 0;
}

/**
** <b>Description</b>
**
**  Kill a unit
**
**  @param l  Lua state.
**
**  @return   Returns true if a unit was killed.
**
** Example:
**
** <div class="example"><code>-- Kills an ogre controlled by player 3
**		<strong>KillUnit</strong>("unit-ogre", 3)</code></div>
*/
static int CclKillUnit(lua_State *l)
{
	LuaCheckArgs(l, 2);

	lua_pushvalue(l, 1);
	const auto unitValidator = TriggerGetUnitType(l);
	lua_pop(l, 1);
	CPlayer *player = CclGetPlayer(l);
	if (player == nullptr) {
		auto it = ranges::find_if(UnitManager->GetUnits(),
		                          [&](const CUnit *unit) { return unitValidator(*unit); });

		if (it != UnitManager->GetUnits().end()) {
			LetUnitDie(**it);
			lua_pushboolean(l, 1);
			return 1;
		}
	} else {
		auto it = ranges::find_if(player->GetUnits(),
		                          [&](const CUnit *unit) { return unitValidator(*unit); });

		if (it != player->GetUnits().end()) {
			LetUnitDie(**it);
			lua_pushboolean(l, 1);
			return 1;
		}
	}
	lua_pushboolean(l, 0);
	return 1;
}

/**
** <b>Description</b>
**
**  Kill a unit at a location
**
**  @param l  Lua state.
**
**  @return   Returns the number of units killed.
**
** Example:
**
** <div class="example"><code>-- Kill 8 peasants controlled by player 7 from position {27,1} to {34,5}
**		<strong>KillUnitAt</strong>("unit-peasant",7,8,{27,1},{34,5})</code></div>
*/
static int CclKillUnitAt(lua_State *l)
{
	LuaCheckArgs(l, 5);

	lua_pushvalue(l, 1);
	const auto unitValidator = TriggerGetUnitType(l);
	lua_pop(l, 1);
	lua_pushvalue(l, 2);
	const auto unitPlayerValidator = TriggerGetPlayer(l);
	lua_pop(l, 1);
	int q = LuaToNumber(l, 3);

	if (!lua_istable(l, 4) || !lua_istable(l, 5)) {
		LuaError(l, "incorrect argument");
	}
	Vec2i pos1;
	Vec2i pos2;
	CclGetPos(l, &pos1, 4);
	CclGetPos(l, &pos2, 5);
	if (pos1.x > pos2.x) {
		std::swap(pos1.x, pos2.x);
	}
	if (pos1.y > pos2.y) {
		std::swap(pos1.y, pos2.y);
	}

	std::vector<CUnit *> table = Select(pos1, pos2);

	int s = 0;
	for (std::vector<CUnit *>::iterator it = table.begin(); it != table.end() && s < q; ++it) {
		CUnit &unit = **it;

		if (unitValidator(unit) && unitPlayerValidator(unit) && unit.IsAlive()) {
			LetUnitDie(unit);
			++s;
		}
	}
	lua_pushnumber(l, s);
	return 1;
}

/**
** <b>Description</b>
**
**  Get a player's units
**
**  @param l  Lua state.
**
**  @return   Array of units.
**
** Example:
**
** <div class="example"><code>-- Get units from player 0
**	units = <strong>GetUnits</strong>(0)
**	for i, id_unit in ipairs(units) do
**	    print(id_unit)
**	end</code></div>
*/
static int CclGetUnits(lua_State *l)
{
	LuaCheckArgs(l, 1);

	const CPlayer *player = CclGetPlayer(l);

	lua_newtable(l);
	if (player == nullptr) {
		int i = 0;
		for (const CUnit *unit : UnitManager->GetUnits()) {
			lua_pushnumber(l, UnitNumber(*unit));
			lua_rawseti(l, -2, i + 1);
			++i;
		}
	} else {
		for (int i = 0; i < player->GetUnitCount(); ++i) {
			lua_pushnumber(l, UnitNumber(player->GetUnit(i)));
			lua_rawseti(l, -2, i + 1);
		}
	}
	return 1;
}

/**
** <b>Description</b>
**
**  Get a player's units in rectangle box specified with 2 coordinates
**
**  @param l  Lua state.
**
**  @return   Array of units.
**
** Example:
**
** <div class="example"><code>circlePower = CreateUnit("unit-circle-of-power", 15, {59, 4})
**		-- Get the units near the circle of power.
**      unitsOnCircle = <strong>GetUnitsAroundUnit</strong>(circle,1,true)</code></div>
*/
static int CclGetUnitsAroundUnit(lua_State *l)
{
	const int nargs = lua_gettop(l);
	if (nargs != 2 && nargs != 3) {
		LuaError(l, "incorrect argument\n");
	}

	const int slot = LuaToNumber(l, 1);
	const CUnit &unit = UnitManager->GetSlotUnit(slot);
	const int range = LuaToNumber(l, 2);
	bool allUnits = false;
	if (nargs == 3) {
		allUnits = LuaToBoolean(l, 3);
	}
	lua_newtable(l);
	std::vector<CUnit *> table =
		allUnits ? SelectAroundUnit(unit, range, HasNotSamePlayerAs(Players[PlayerNumNeutral]))
				 : SelectAroundUnit(unit, range, HasSamePlayerAs(*unit.Player));

	size_t n = 0;
	for (CUnit *unit : table) {
		if (unit->IsAliveOnMap()) {
			lua_pushnumber(l, UnitNumber(*unit));
			lua_rawseti(l, -2, ++n);
		}
	}
	return 1;
}

/**
**
**  Get the value of the unit bool-flag.
**
**  @param l  Lua state.
**
**  @return   The value of the bool-flag of the unit.
*/
static int CclGetUnitBoolFlag(lua_State *l)
{
	LuaCheckArgs(l, 2);

	lua_pushvalue(l, 1);
	const CUnit *unit = CclGetUnit(l);
	lua_pop(l, 1);

	const std::string_view value = LuaToString(l, 2);
	int index = UnitTypeVar.BoolFlagNameLookup[value];// User bool flags
	if (index == -1) {
		LuaError(l, "Bad bool-flag name '%s'\n", value.data());
	}
	lua_pushboolean(l, unit->Type->BoolFlag[index].value);
	return 1;
}

/**
** <b>Description</b>
**
**  Get the value of the unit variable.
**
**  @param l  Lua state.
**
**  @return   The value of the variable of the unit.
**
** Example:
**
** <div class="example"><code>-- Make a grunt for player 5
**		grunt = CreateUnit("unit-grunt", 5, {58, 8})
**		-- Take the name of the unit
**		unit_name = <strong>GetUnitVariable</strong>(grunt,"Name")
**		-- Take the player number based on the unit
**		player_type = <strong>GetUnitVariable</strong>(grunt,"PlayerType")
**		-- Take the value of the armor
**		armor_value = <strong>GetUnitVariable</strong>(grunt,"Armor")
**		-- Show the message in the game.
**		AddMessage(unit_name .. " " .. player_type .. " " .. armor_value)</code></div>
*/
static int CclGetUnitVariable(lua_State *l)
{
	const int nargs = lua_gettop(l);
	Assert(nargs == 2 || nargs == 3);

	lua_pushvalue(l, 1);
	CUnit *unit = CclGetUnit(l);
	if (unit == nullptr) {
		return 1;
	}
	UpdateUnitVariables(*unit);
	lua_pop(l, 1);

	const std::string_view value = LuaToString(l, 2);
	if (value == "RegenerationRate") {
		lua_pushnumber(l, unit->Variable[HP_INDEX].Increase);
	} else if (value == "RegenerationFrequency") {
		lua_pushnumber(l, std::max((int)unit->Variable[HP_INDEX].IncreaseFrequency, 1));
	} else if (value == "Ident") {
		lua_pushstring(l, unit->Type->Ident.c_str());
	} else if (value == "ResourcesHeld") {
		lua_pushnumber(l, unit->ResourcesHeld);
	} else if (value == "GiveResourceType") {
		lua_pushnumber(l, unit->Type->GivesResource);
	} else if (value == "CurrentResource") {
		lua_pushnumber(l, unit->CurrentResource);
	} else if (value == "Name") {
		lua_pushstring(l, unit->Type->Name.c_str());
	} else if (value == "PlayerType") {
		lua_pushstring(l, PlayerTypeNames[static_cast<int>(unit->Player->Type)].c_str());
	} else if (value == "Summoned") {
		lua_pushnumber(l, unit->Summoned);
	} else if (value == "TTLPercent") {
		if (unit->Summoned && unit->TTL) {
			unsigned long time_lived = GameCycle - unit->Summoned;
			Assert(time_lived >= 0);
			unsigned long time_to_live = unit->TTL - unit->Summoned;
			Assert(time_to_live > 0);
			double pcnt = time_lived * 100.0 / time_to_live;
			int pcnt_i = (int)round(pcnt);
			lua_pushinteger(l, pcnt_i);
		} else {
			lua_pushinteger(l, -1);
		}
	} else if (value == "IndividualUpgrade") {
		LuaCheckArgs(l, 3);
		std::string_view upgrade_ident = LuaToString(l, 3);
		if (CUpgrade::Get(upgrade_ident)) {
			lua_pushboolean(l, unit->IndividualUpgrades[CUpgrade::Get(upgrade_ident)->ID]);
		} else {
			LuaError(l, "Individual upgrade \"%s\" doesn't exist.", upgrade_ident.data());
		}
		return 1;
	} else if (value == "Active") {
		lua_pushboolean(l, unit->Active);
		return 1;
	} else if (value == "Idle") {
		lua_pushboolean(l, unit->IsIdle());
		return 1;
	} else if (value == "PixelPos") {
		PixelPos pos = unit->GetMapPixelPosCenter();
		lua_newtable(l);
		lua_pushnumber(l, pos.x);
		lua_setfield(l, -2, "x");
		lua_pushnumber(l, pos.y);
		lua_setfield(l, -2, "y");
		return 1;
	} else {
		int index = UnitTypeVar.VariableNameLookup[value];// User variables
		if (index == -1) {
			if (nargs == 2) {
				index = UnitTypeVar.BoolFlagNameLookup[value];
				if (index != -1) {
					lua_pushboolean(l, unit->Type->BoolFlag[index].value);
					return 1;
				}
			}
		}
		if (index == -1) {
			LuaError(l, "Bad variable name '%s'\n", value.data());
		}
		if (nargs == 2) {
			lua_pushnumber(l, unit->Variable[index].Value);
		} else {
			const std::string_view type = LuaToString(l, 3);
			if (type == "Value") {
				lua_pushnumber(l, unit->Variable[index].Value);
			} else if (type == "Max") {
				lua_pushnumber(l, unit->Variable[index].Max);
			} else if (type == "Increase") {
				lua_pushnumber(l, unit->Variable[index].Increase);
			} else if (type == "IncreaseFrequency") {
				lua_pushnumber(l, std::max((int)unit->Variable[index].IncreaseFrequency, 1));
			} else if (type == "Enable") {
				lua_pushnumber(l, unit->Variable[index].Enable);
			} else {
				LuaError(l, "Bad variable type '%s'\n", type.data());
			}
		}
	}
	return 1;
}

/**
** <b>Description</b>
**
**  Set the value of the unit variable.
**
**  @param l  Lua state.
**
**  @return The new value of the unit.
**
** Example:
**
** <div class="example"><code>-- Create a blacksmith for player 2
** blacksmith = CreateUnit("unit-human-blacksmith", 2, {66, 71})
** -- Specify the amount of hit points to assign to the blacksmith
** <strong>SetUnitVariable</strong>(blacksmith,"HitPoints",344)
** -- Set the blacksmiths color to the color of player 4
** <strong>SetUnitVariable</strong>(blacksmith,"Color",4)
** </code></div>
*/
static int CclSetUnitVariable(lua_State *l)
{
	const int nargs = lua_gettop(l);
	Assert(nargs >= 3 && nargs <= 5);

	lua_pushvalue(l, 1);
	CUnit *unit = CclGetUnit(l);
	lua_pop(l, 1);
	const std::string_view name = LuaToString(l, 2);
	int value = 0;
	if (name == "Player") {
		value = LuaToNumber(l, 3);
		unit->ChangeOwner(Players[value]);
	} else if (name == "Color") {
		if (lua_isstring(l, 3)) {
			const std::string_view colorName = LuaToString(l, 3);
			for (size_t i = 0; i < PlayerColorNames.size(); i++) {
				if (PlayerColorNames[i] == colorName) {
					unit->Colors = i;
					break;
				}
			}
		} else if (lua_isnil(l, 3)) {
			unit->Colors = -1;
		} else {
			value = LuaToNumber(l, 3);
			unit->Colors = value;
		}
	} else if (name == "TTL") {
		value = LuaToNumber(l, 3);
		unit->TTL = GameCycle + value;
	} else if (name == "Summoned") {
		value = LuaToNumber(l, 3);
		unit->Summoned = value;
	} else if (name == "RegenerationRate") {
		value = LuaToNumber(l, 3);
		unit->Variable[HP_INDEX].Increase = std::min(unit->Variable[HP_INDEX].Max, value);
	} else if (name == "RegenerationFrequency") {
		value = LuaToNumber(l, 3);
		unit->Variable[HP_INDEX].IncreaseFrequency = value;
		if (unit->Variable[HP_INDEX].IncreaseFrequency != value) {
			LuaError(l, "RegenerationFrequency out of range!");
		}
	} else if (name == "IndividualUpgrade") {
		LuaCheckArgs(l, 4);
		std::string_view upgrade_ident = LuaToString(l, 3);
		bool has_upgrade = LuaToBoolean(l, 4);
		if (CUpgrade::Get(upgrade_ident)) {
			if (has_upgrade && unit->IndividualUpgrades[CUpgrade::Get(upgrade_ident)->ID] == false) {
				IndividualUpgradeAcquire(*unit, CUpgrade::Get(upgrade_ident));
			} else if (!has_upgrade && unit->IndividualUpgrades[CUpgrade::Get(upgrade_ident)->ID]) {
				IndividualUpgradeLost(*unit, CUpgrade::Get(upgrade_ident));
			}
		} else {
			LuaError(l, "Individual upgrade \"%s\" doesn't exist.", upgrade_ident.data());
		}
	} else if (name == "Active") {
		bool ai_active = LuaToBoolean(l, 3);
		if (ai_active != unit->Active) {
			if (ai_active) {
				unit->Player->UnitTypesAiActiveCount[unit->Type->Slot]++;
			} else {
				unit->Player->UnitTypesAiActiveCount[unit->Type->Slot]--;
				if (unit->Player->UnitTypesAiActiveCount[unit->Type->Slot] < 0) { // if unit AI active count is negative, something wrong happened
					LuaError(l,
					         "Player %d has a negative '%s' AI active count of %d.\n",
					         unit->Player->Index,
					         unit->Type->Ident.c_str(),
					         unit->Player->UnitTypesAiActiveCount[unit->Type->Slot]);
				}
			}
		}
		unit->Active = ai_active;
	} else {
		const int index = UnitTypeVar.VariableNameLookup[name];// User variables
		if (index == -1) {
			LuaError(l, "Bad variable name '%s'\n", name.data());
		}
		value = LuaToNumber(l, 3);
		bool stats = false;
		if (nargs == 5) {
			stats = LuaToBoolean(l, 5);
		}
		if (stats) { // stat variables
			const std::string_view type = LuaToString(l, 4);
			if (type == "Value") {
				unit->Stats->Variables[index].Value = std::min(unit->Stats->Variables[index].Max, value);
			} else if (type == "Max") {
				unit->Stats->Variables[index].Max = value;
			} else if (type == "Increase") {
				unit->Stats->Variables[index].Increase = value;
			} else if (type == "IncreaseFrequency") {
				unit->Stats->Variables[index].IncreaseFrequency = value;
				if (unit->Stats->Variables[index].IncreaseFrequency != value) {
					LuaError(l, "%s.IncreaseFrequency out of range!", type.data());
				}
			} else if (type == "Enable") {
				unit->Stats->Variables[index].Enable = value;
			} else {
				LuaError(l, "Bad variable type '%s'\n", type.data());
			}
		} else if (nargs == 3) {
			unit->Variable[index].Value = std::min(unit->Variable[index].Max, value);
		} else {
			const std::string_view type = LuaToString(l, 4);
			if (type == "Value") {
				unit->Variable[index].Value = std::min(unit->Variable[index].Max, value);
			} else if (type == "Max") {
				unit->Variable[index].Max = value;
			} else if (type == "Increase") {
				unit->Variable[index].Increase = value;
			} else if (type == "IncreaseFrequency") {
				unit->Variable[index].IncreaseFrequency = value;
				if (unit->Variable[index].IncreaseFrequency != value) {
					LuaError(l, "%s.IncreaseFrequency out of range!", type.data());
				}
			} else if (type == "Enable") {
				unit->Variable[index].Enable = value;
			} else {
				LuaError(l, "Bad variable type '%s'\n", type.data());
			}
		}
	}
	lua_pushnumber(l, value);
	return 1;
}

/**
**  Get the usage of unit slots during load to allocate memory
**
**  @param l  Lua state.
*/
static int CclSlotUsage(lua_State *l)
{
	UnitManager->Load(l);
	return 0;
}

/**
** <b>Description</b>
**
**  Select a single unit
**
**  @param l  Lua state.
**
**  @return 0, meaning the unit is selected.
**
** Example:
**
** <div class="example"><code>-- Make the hero unit Grom Hellscream for player 5
**		grom = CreateUnit("unit-beast-cry", 5, {58, 8})
**		-- Select only the unit Grom Hellscream
**		<strong>SelectSingleUnit</strong>(grom)</code></div>
*/
static int CclSelectSingleUnit(lua_State *l)
{
	const int nargs = lua_gettop(l);
	Assert(nargs == 1);
	lua_pushvalue(l, 1);
	CUnit *unit = CclGetUnit(l);
	lua_pop(l, 1);
	SelectSingleUnit(*unit);
	SelectionChanged();
	return 0;
}

/**
**  Find the next reachable resource unit that gives resource starting from a worker.
**  Optional third argument is the range to search.
**
**  @param l  Lua state.
**
** Example:
**
** <div class="example"><code>
**		peon = CreateUnit("unit-peon", 5, {58, 8})
**      goldmine = <strong>FindNextResource(peon, 0)</strong></code></div>
*/
static int CclFindNextResource(lua_State *l)
{
	const int nargs = lua_gettop(l);
	if (nargs < 2 || nargs > 3) {
		LuaError(l, "incorrect argument count");
	}

	lua_pushvalue(l, 1);
	CUnit *unit = CclGetUnit(l);
	lua_pop(l, 1);

	lua_pushvalue(l, 2);
	const int resource = CclGetResourceByName(l);
	lua_pop(l, 1);

	const int range = nargs == 3 ? LuaToNumber(l, 3) : 1000;

	CUnit *resourceUnit = UnitFindResource(*unit, *unit, range, resource);
	if (resourceUnit) {
		lua_pushnumber(l, UnitNumber(*unit));
	} else {
		lua_pushnil(l);
	}
	return 1;
}

/**
**  Enable/disable simplified auto targeting
**
**  @param l  Lua state.
**
**  @return   0 for success, 1 for wrong type;
*/
static int CclEnableSimplifiedAutoTargeting(lua_State *l)
{
	LuaCheckArgs(l, 1);
	const bool isSimplified = LuaToBoolean(l, 1);
	if (!IsNetworkGame()) {
		GameSettings.SimplifiedAutoTargeting = isSimplified;
	} else {
		NetworkSendExtendedCommand(ExtendedMessageAutoTargetingDB,
								   int(isSimplified), 0, 0, 0, 0);
	}
	return 0;
}

/**
**  Turn towards another unit or a location.
**
**  @param l  Lua state.
**
** Example:
**
** <div class="example"><code>
**		<strong>TurnTowardsLocation(peon, {10, 10})</strong> -- turn peon towards location 10x10
**		<strong>TurnTowardsLocation(peon, goldmine)</strong> -- turn peon towards the goldmine unit
** </code></div>
*/
static int CclTurnTowardsLocation(lua_State *l)
{
	LuaCheckArgs(l, 2);

	lua_pushvalue(l, 1);
	CUnit *unit = CclGetUnit(l);
	lua_pop(l, 1);

	Vec2i dir;
	if (lua_istable(l, 2)) {
		CclGetPos(l, &dir, 2);
		dir = dir - unit->tilePos;
	} else {
		lua_pushvalue(l, 2);
		CUnit *target = CclGetUnit(l);
		lua_pop(l, 1);
		dir = target->tilePos + target->Type->GetHalfTileSize() - unit->tilePos;
	}

	UnitHeadingFromDeltaXY(*unit, dir);

	return 0;
}

/**
**  Register CCL features for unit.
*/
void UnitCclRegister()
{
	lua_register(Lua, "SetTrainingQueue", CclSetTrainingQueue);
	lua_register(Lua, "SetBuildingCapture", CclSetBuildingCapture);
	lua_register(Lua, "SetRevealAttacker", CclSetRevealAttacker);
	lua_register(Lua, "ResourcesMultiBuildersMultiplier", CclResourcesMultiBuildersMultiplier);

	lua_register(Lua, "Unit", CclUnit);

	lua_register(Lua, "MoveUnit", CclMoveUnit);
	lua_register(Lua, "RemoveUnit", CclRemoveUnit);
	lua_register(Lua, "CreateUnit", CclCreateUnit);
	lua_register(Lua, "TransformUnit", CclTransformUnit);
	lua_register(Lua, "DamageUnit", CclDamageUnit);
	lua_register(Lua, "SetResourcesHeld", CclSetResourcesHeld);
	lua_register(Lua, "SetTeleportDestination", CclSetTeleportDestination);
	lua_register(Lua, "OrderUnit", CclOrderUnit);
	lua_register(Lua, "KillUnit", CclKillUnit);
	lua_register(Lua, "KillUnitAt", CclKillUnitAt);
	lua_register(Lua, "FindNextResource", CclFindNextResource);

	lua_register(Lua, "GetUnits", CclGetUnits);
	lua_register(Lua, "GetUnitsAroundUnit", CclGetUnitsAroundUnit);

	// unit member access functions
	lua_register(Lua, "GetUnitBoolFlag", CclGetUnitBoolFlag);
	lua_register(Lua, "GetUnitVariable", CclGetUnitVariable);
	lua_register(Lua, "SetUnitVariable", CclSetUnitVariable);

	lua_register(Lua, "SlotUsage", CclSlotUsage);

	lua_register(Lua, "SelectSingleUnit", CclSelectSingleUnit);
	lua_register(Lua, "EnableSimplifiedAutoTargeting", CclEnableSimplifiedAutoTargeting);

	lua_register(Lua, "TurnTowardsLocation", CclTurnTowardsLocation);
}

//@}
