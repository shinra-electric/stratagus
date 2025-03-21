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
/**@name ai_resource.cpp - AI resource manager. */
//
//      (c) Copyright 2000-2015 by Lutz Sammer, Antonis Chaniotis
//		and Andrettin
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

#include "settings.h"
#include "stratagus.h"

#include "ai_local.h"

#include "action/action_build.h"
#include "action/action_repair.h"
#include "action/action_resource.h"
#include "commands.h"
#include "depend.h"
#include "map.h"
#include "pathfinder.h"
#include "player.h"
#include "tileset.h"
#include "unit.h"
#include "unit_find.h"
#include "unittype.h"
#include "upgrade.h"
#include "network.h"

/*----------------------------------------------------------------------------
--  Defines
----------------------------------------------------------------------------*/

#define COLLECT_RESOURCES_INTERVAL 4

/*----------------------------------------------------------------------------
--  Functions
----------------------------------------------------------------------------*/

static bool AiMakeUnit(CUnitType &type, const Vec2i &nearPos);

/**
**  Check if the costs are available for the AI.
**
**  Take reserve and already used resources into account.
**
**  @param costs  Costs for something.
**
**  @return       A bit field of the missing costs.
*/
static int AiCheckCosts(const int (&costs)[MaxCosts])
{
	// FIXME: the used costs shouldn't be calculated here
	int(&used)[MaxCosts] = AiPlayer->Used;

	ranges::fill(used, 0);
	for (CUnit *unit : AiPlayer->Player->GetUnits()) {
		for (const auto &order : unit->Orders) {
			if (order->Action == UnitAction::Build) {
				const COrder_Build &orderBuild = *static_cast<const COrder_Build *>(order.get());
				const int *building_costs = orderBuild.GetUnitType().Stats[AiPlayer->Player->Index].Costs;

				for (int j = 1; j < MaxCosts; ++j) {
					used[j] += building_costs[j];
				}
			}
		}
	}

	int err = 0;
	const int *resources = AiPlayer->Player->Resources;
	const int *storedresources = AiPlayer->Player->StoredResources;
	const int *reserve = AiPlayer->Reserve;
	for (int i = 1; i < MaxCosts; ++i) {
		if (resources[i] + storedresources[i] - used[i] < costs[i] - reserve[i]) {
			err |= 1 << i;
		}
	}
	return err;
}

/**
**  Check if the AI player needs food.
**
**  It counts buildings in progress and units in training queues.
**
**  @param pai   AI player.
**  @param type  Unit-type that should be build.
**
**  @return      True if enough, false otherwise.
**
**  @todo  The number of food currently trained can be stored global
**         for faster use.
*/
static bool AiCheckSupply(const PlayerAi &pai, const CUnitType &type)
{
	// Count food supplies under construction.
	int remaining = 0;
	for (const AiBuildQueue &queue : pai.UnitTypeBuilt) {
		if (queue.Type->Stats[pai.Player->Index].Variables[SUPPLY_INDEX].Value) {
			remaining += queue.Made * queue.Type->Stats[pai.Player->Index].Variables[SUPPLY_INDEX].Value;
		}
	}

	// We are already out of food.
	remaining += pai.Player->Supply - pai.Player->Demand - type.Stats[pai.Player->Index].Variables[DEMAND_INDEX].Value;
	if (remaining < 0) {
		return false;
	}
	// Count what we train.
	for (const AiBuildQueue &queue : pai.UnitTypeBuilt) {
		remaining -= queue.Made * queue.Type->Stats[pai.Player->Index].Variables[DEMAND_INDEX].Value;
		if (remaining < 0) {
			return false;
		}
	}
	return true;
}

/**
**  Check if the costs for an unit-type are available for the AI.
**
**  Take reserve and already used resources into account.
**
**  @param type  Unit-type to check the costs for.
**
**  @return      A bit field of the missing costs.
*/
static int AiCheckUnitTypeCosts(const CUnitType &type)
{
	return AiCheckCosts(type.Stats[AiPlayer->Player->Index].Costs);
}

template <bool ignoreVisibility = false>
class IsAEnemyUnitOf
{
public:
	explicit IsAEnemyUnitOf(const CPlayer &_player) : player(&_player) {}
	bool operator()(const CUnit *unit) const
	{
		return (ignoreVisibility || unit->IsVisibleAsGoal(*player)) && unit->IsEnemy(*player);
	}
private:
	const CPlayer *player;
};

template <bool ignoreVisibility = false>
class IsAEnemyUnitWhichCanCounterAttackOf
{
public:
	explicit IsAEnemyUnitWhichCanCounterAttackOf(const CPlayer &_player, const CUnitType &_type) :
		player(&_player), type(&_type)
	{}
	bool operator()(const CUnit *unit) const
	{
		return (ignoreVisibility || unit->IsVisibleAsGoal(*player))
			   && unit->IsEnemy(*player)
			   && CanTarget(*unit->Type, *type);
	}
private:
	const CPlayer *player;
	const CUnitType *type;
};

/**
**  Check if there are enemy units in a given range.
**
**  @param player  Find enemies of this player
**  @param type    Optional unit type to check if enemy can target this
**  @param pos     location
**  @param range   Distance range to look.
**
**  @return        If there are any enemy units in the range
*/
bool AiEnemyUnitsInDistance(const CPlayer &player,
						    const CUnitType *type, const Vec2i &pos, unsigned range)
{
	const Vec2i offset(range, range);

	if (type == nullptr) {
		std::vector<CUnit *> units = Select<1>(pos - offset, pos + offset, IsAEnemyUnitOf<true>(player));
		return !units.empty();
	} else {
		const Vec2i typeSize(type->TileWidth - 1, type->TileHeight - 1);
		const IsAEnemyUnitWhichCanCounterAttackOf<true> pred(player, *type);

		std::vector<CUnit *> units = Select<1>(pos - offset, pos + typeSize + offset, pred);
		return !units.empty();
	}
}

/**
**  Check if there are enemy units in a given range.
**
**  @param unit   Find in distance for this unit.
**  @param range  Distance range to look.
**
**  @return       If there are any enemy units in the range
*/
bool AiEnemyUnitsInDistance(const CUnit &unit, unsigned range)
{
	return AiEnemyUnitsInDistance(*unit.Player, unit.Type, unit.tilePos, range);
}

static bool IsAlreadyWorking(const CUnit &unit)
{
	for (const auto &order : unit.Orders) {
		const UnitAction action = order->Action;

		if (action == UnitAction::Build || action == UnitAction::Repair) {
			return true;
		}
		if (action == UnitAction::Resource) {
			const COrder_Resource &orderRes = *static_cast<const COrder_Resource *>(order.get());

			if (orderRes.IsGatheringStarted()) {
				return true;
			}
		}
	}
	return false;
}


/**
**  Check if we can build the building.
**
**  @param type      Unit that can build the building.
**  @param building  Building to be build.
**
**  @return          True if made, false if can't be made.
**
**  @note            We must check if the dependencies are fulfilled.
*/
static bool AiBuildBuilding(const CUnitType &type, CUnitType &building, const Vec2i &nearPos)
{
	std::vector<CUnit *> table = FindPlayerUnitsByType(*AiPlayer->Player, type, true);

	// Remove all workers on the way building something
	ranges::erase_if(table, [](const CUnit *unit) { return IsAlreadyWorking(*unit); });
	if (table.empty()) {
		// No workers available to build
		return false;
	}

	CUnit &candidate = (table.size() == 1) ? *table[0] : *table[SyncRand() % table.size()];

	// Find a place to build.
	if (auto pos = AiFindBuildingPlace(candidate, building, nearPos)) {
		CommandBuildBuilding(candidate, *pos, building, EFlushMode::On);
		return true;
	} else {
		//when first worker can't build then rest also won't be able (save CPU)
		if (Map.Info.IsPointOnMap(nearPos)) {
			//Crush CPU !!!!!
			for (auto *unit : table) {
				if (unit == &candidate) { // already checked.
					continue;
				}
				// Find a place to build.
				if (auto pos = AiFindBuildingPlace(*unit, building, nearPos)) {
					CommandBuildBuilding(*unit, *pos, building, EFlushMode::On);
					return true;
				}
			}
		}
	}
	return false;
}

static bool AiRequestedTypeAllowed(const CPlayer &player, const CUnitType &type)
{
	for (const CUnitType *builder : AiHelpers.Build()[type.Slot]) {
		if (player.UnitTypesAiActiveCount[builder->Slot] > 0
			&& CheckDependByType(player, type)) {
			return true;
		}
	}
	return false;
}

struct cnode {
	int unit_cost;
	int needmask;
	CUnitType *type;
};

static bool cnode_cmp(const cnode &lhs, const cnode &rhs)
{
	return lhs.unit_cost < rhs.unit_cost;
}

std::array<int, UnitTypeMax> AiGetBuildRequestsCount(const PlayerAi &pai)
{
	std::array<int, UnitTypeMax> res{};

	for (const AiBuildQueue &queue : pai.UnitTypeBuilt) {
		res[queue.Type->Slot] += queue.Want;
	}
	return res;
}

extern CUnit *FindDepositNearLoc(CPlayer &p, const Vec2i &pos, int range, int resource);


void AiNewDepotRequest(CUnit &worker)
{
#if 0
	DebugPrint("%d: Worker %d report: Resource [%d] too far from depot, returning time [%d].\n",
			   worker->Player->Index, worker->Slot,
			   worker->CurrentResource,
			   worker->Data.Move.Cycles);
#endif
	Assert(worker.CurrentAction() == UnitAction::Resource);
	COrder_Resource &order = *static_cast<COrder_Resource *>(worker.CurrentOrder());
	const int resource = order.GetCurrentResource();

	const Vec2i pos = order.GetHarvestLocation();
	const int range = 15;

	if (pos.x != -1 && nullptr != FindDepositNearLoc(*worker.Player, pos, range, resource)) {
		/*
		 * New Depot has just be finished and worker just return to old depot
		 * (far away) from new Depot.
		 */
		return;
	}
	CUnitType *best_type = nullptr;
	int best_cost = 0;
	//int best_mask = 0;
	// Count the already made build requests.
	const auto counter = AiGetBuildRequestsCount(*worker.Player->Ai);

	for (CUnitType *type : AiHelpers.Depots()[resource - 1]) {
		if (counter[type->Slot]) { // Already ordered.
			return;
		}
		if (!AiRequestedTypeAllowed(*worker.Player, *type)) {
			continue;
		}

		// Check if resources available.
		//int needmask = AiCheckUnitTypeCosts(type);
		int cost = 0;
		for (int c = 1; c < MaxCosts; ++c) {
			cost += type->Stats[worker.Player->Index].Costs[c];
		}

		if (best_type == nullptr || (cost < best_cost)) {
			best_type = type;
			best_cost = cost;
			//best_mask = needmask;
		}
	}

	if (best_type) {
		//if(!best_mask) {
		AiBuildQueue queue;

		queue.Type = best_type;
		queue.Want = 1;
		queue.Made = 0;
		queue.Pos = pos;

		worker.Player->Ai->UnitTypeBuilt.push_back(queue);

		DebugPrint("%d: Worker %d report: Requesting new depot near [%d,%d].\n",
		           worker.Player->Index,
		           UnitNumber(worker),
		           queue.Pos.x,
		           queue.Pos.y);
		/*
		} else {
			AiPlayer->NeededMask |= best_mask;
		}
		*/
	}
}

class IsAWorker
{
public:
	explicit IsAWorker() {}
	bool operator()(const CUnit *const unit) const
	{
		return (unit->Type->BoolFlag[HARVESTER_INDEX].value && !unit->Removed);
	}
};

class CompareDepotsByDistance
{
public:
	explicit CompareDepotsByDistance(const CUnit &worker) : worker(worker) {}
	bool operator()(const CUnit *lhs, const CUnit *rhs) const
	{
		return lhs->MapDistanceTo(worker) < rhs->MapDistanceTo(worker);
	}
private:
	const CUnit &worker;
};

/**
**  Get a suitable depot for better resource harvesting.
**
**  @param worker    Worker itself.
**  @param oldDepot  Old assigned depot.
**
**  @return          new depot, resourceUnit if found, {nullptr, nullptr} otherwise.
*/
std::pair<CUnit *, CUnit *> AiGetSuitableDepot(const CUnit &worker, const CUnit &oldDepot)
{
	Assert(worker.CurrentAction() == UnitAction::Resource);
	COrder_Resource &order = *static_cast<COrder_Resource *>(worker.CurrentOrder());
	const int resource = order.GetCurrentResource();
	std::vector<CUnit *> depots;

	for (CUnit *unit : worker.Player->GetUnits()) {
		if (unit->Type->CanStore[resource] && !unit->IsUnusable()) {
			depots.push_back(unit);
		}
	}
	// If there aren't any alternatives, exit
	if (depots.size() < 2) {
		return {nullptr, nullptr};
	}
	ranges::sort(depots, CompareDepotsByDistance(worker));

	for (CUnit* unitPtr : depots) {
		CUnit &unit = *unitPtr;

		const unsigned int tooManyWorkers = 15;
		const int range = 15;

		if (&oldDepot == &unit) {
			continue;
		}
		if (unit.Refs > tooManyWorkers) {
			continue;
		}
		if (AiEnemyUnitsInDistance(worker, range)) {
			continue;
		}
		CUnit *res = UnitFindResource(worker, unit, range, resource, unit.Player->AiEnabled);
		if (res) {
			return {&unit, res};
		}
	}
	return {nullptr, nullptr};
}

/**
**  Build new units to reduce the food shortage.
*/
static bool AiRequestSupply()
{
	// Don't request supply if we're sleeping.  When the script starts it may
	// request a better unit than the one we pick here.  If we only have enough
	// resources for one unit we don't want to build the wrong one.
	if (AiPlayer->SleepCycles != 0) {
		/* we still need supply */
		return true;
	}

	// Count the already made build requests.
	const auto counter = AiGetBuildRequestsCount(*AiPlayer);
	struct cnode cache[16]{};

	//
	// Check if we can build this?
	//
	int j = 0;

	for (CUnitType *type : AiHelpers.UnitLimit()[0]) {
		if (counter[type->Slot]) { // Already ordered.
#if defined(DEBUG) && defined(DebugRequestSupply)
			DebugPrint("%d: AiRequestSupply: Supply already build in %s\n",
					   AiPlayer->Player->Index, type->Name.c_str());
#endif
			return false;
		}

		if (!AiRequestedTypeAllowed(*AiPlayer->Player, *type)) {
			continue;
		}

		//
		// Check if resources available.
		//
		cache[j].needmask = AiCheckUnitTypeCosts(*type);

		for (int c = 1; c < MaxCosts; ++c) {
			cache[j].unit_cost += type->Stats[AiPlayer->Player->Index].Costs[c];
		}
		cache[j].unit_cost += type->Stats[AiPlayer->Player->Index].Variables[SUPPLY_INDEX].Value - 1;
		cache[j].unit_cost /= type->Stats[AiPlayer->Player->Index].Variables[SUPPLY_INDEX].Value;
		cache[j++].type = type;
		Assert(j < 16);
	}

	if (j > 1) {
		std::sort(&cache[0], &cache[j], cnode_cmp);
	}
	if (j) {
		if (!cache[0].needmask) {
			CUnitType &type = *cache[0].type;
			Vec2i invalidPos(-1, -1);
			if (AiMakeUnit(type, invalidPos)) {
				AiBuildQueue newqueue;
				newqueue.Type = &type;
				newqueue.Want = 1;
				newqueue.Made = 1;
				AiPlayer->UnitTypeBuilt.insert(
					AiPlayer->UnitTypeBuilt.begin(), newqueue);
#if defined( DEBUG) && defined( DebugRequestSupply )
				DebugPrint("%d: AiRequestSupply: build Supply in %s\n",
						   AiPlayer->Player->Index, type->Name.c_str());
#endif
				return false;
			}
		}
		AiPlayer->NeededMask |= cache[0].needmask;
	}


#if defined( DEBUG) && defined( DebugRequestSupply )
	std::string needed("");
	for (int i = 1; i < MaxCosts; ++i) {
		if (cache[0].needmask & (1 << i)) {
			needed += ":";
			switch (i) {
				case GoldCost:
					needed += "Gold<";
					break;
				case WoodCost:
					needed += "Wood<";
					break;
				case OilCost:
					needed += "Oil<";
					break;
				default:
					needed += "unknown<";
					break;
			}
			needed += '0' + i;
			needed += ">";
		}
	}
	DebugPrint("%d: AiRequestSupply: needed build %s with %s resource\n",
			   AiPlayer->Player->Index, cache[0].type->Name.c_str(), needed.c_str());
#endif
	return true;
}

/**
**  Check if we can train the unit.
**
**  @param type  Unit that can train the unit.
**  @param what  What to be trained.
**
**  @return      True if made, false if can't be made.
**
**  @note        We must check if the dependencies are fulfilled.
*/
static bool AiTrainUnit(const CUnitType &type, CUnitType &what)
{
	std::vector<CUnit *> table = FindPlayerUnitsByType(*AiPlayer->Player, type, true);

	for (CUnit *unit : table) {
		if (unit->IsIdle()) {
			CommandTrainUnit(*unit, what, EFlushMode::On);
			return true;
		}
	}
	return false;
}

/**
**  Check if we can make a unit-type.
**
**  @param type  Unit-type that must be made.
**
**  @return      True if made, false if can't be made.
**
**  @note        We must check if the dependencies are fulfilled.
*/
static bool AiMakeUnit(CUnitType &typeToMake, const Vec2i &nearPos)
{
	// Find equivalents unittypes.
	const auto usableTypes = AiFindAvailableUnitTypeEquiv(typeToMake);

	// Iterate them
	for (int typeIndex : usableTypes) {
		CUnitType &type = *getUnitTypes()[typeIndex];
		//
		// Check if we have a place for building or a unit to build.
		//
		const std::vector<std::vector<CUnitType *> > &tablep = type.Building ? AiHelpers.Build() : AiHelpers.Train();
		if (type.Slot > tablep.size()) { // Oops not known.
			DebugPrint("%d: AiMakeUnit I: Nothing known about '%s'\n",
			           AiPlayer->Player->Index,
			           type.Ident.c_str());
			continue;
		}
		const std::vector<CUnitType *> &table = tablep[type.Slot];
		if (table.empty()) { // Oops not known.
			DebugPrint("%d: AiMakeUnit II: Nothing known about '%s'\n",
			           AiPlayer->Player->Index,
			           type.Ident.c_str());
			continue;
		}

		const int *unit_count = AiPlayer->Player->UnitTypesAiActiveCount;
		for (const CUnitType *unitType : table) {
			//
			// The type for builder/trainer is available
			//
			if (unit_count[unitType->Slot]) {
				if (type.Building) {
					if (AiBuildBuilding(*unitType, type, nearPos)) {
						return true;
					}
				} else {
					if (AiTrainUnit(*unitType, type)) {
						return true;
					}
				}
			}
		}
	}
	return false;
}

/**
**  Check if we can research the upgrade.
**
**  @param type  Unit that can research the upgrade.
**  @param what  What should be researched.
**
**  @return      True if made, false if can't be made.
**
**  @note        We must check if the dependencies are fulfilled.
*/
static bool AiResearchUpgrade(const CUnitType &type, CUpgrade &what)
{
	std::vector<CUnit *> table = FindPlayerUnitsByType(*AiPlayer->Player, type, true);

	for (CUnit *unit : table) {
		if (unit->IsIdle()) {
			CommandResearch(*unit, what, EFlushMode::On);
			return true;
		}
	}
	return false;
}

/**
**  Check if the research can be done.
**
**  @param upgrade  Upgrade to research
*/
void AiAddResearchRequest(CUpgrade *upgrade)
{
	// Check if resources are available.
	const int costNeeded = AiCheckCosts(upgrade->Costs);

	if (costNeeded) {
		AiPlayer->NeededMask |= costNeeded;
		return;
	}
	//
	// Check if we have a place for the upgrade to research
	//
	{ // multi-research case
		const int n = AiHelpers.Research().size();
		std::vector<std::vector<CUnitType *> > &tablep = AiHelpers.Research();

		if (upgrade->ID < n) { // not known as multi-researchable upgrade
			std::vector<CUnitType *> &table = tablep[upgrade->ID];
			if (!table.empty()) { // not known as multi-researchable upgrade
				const int *unit_count = AiPlayer->Player->UnitTypesAiActiveCount;
				for (const CUnitType *type : table) {
					// The type is available
					if (unit_count[type->Slot]
						&& AiResearchUpgrade(*type, *upgrade)) {
						return;
					}
				}
				return;
			}
		}
	}
	{ // single-research case
		const int n = AiHelpers.SingleResearch().size();
		std::vector<std::vector<CUnitType *> > &tablep = AiHelpers.SingleResearch();

		if (upgrade->ID < n) { // not known
			std::vector<CUnitType *> &table = tablep[upgrade->ID];
			if (!table.empty()) { // not known
				// known as a single-research upgrade, check if we're already
				// researching it. if so, ignore this request.
				if (AiPlayer->Player->UpgradeTimers.Upgrades[upgrade->ID]) {
					return;
				}
				const int *unit_count = AiPlayer->Player->UnitTypesAiActiveCount;
				for (const CUnitType *type : table) {
					// The type is available
					if (unit_count[type->Slot]
						&& AiResearchUpgrade(*type, *upgrade)) {
						return;
					}
				}
				return;
			}
		}
	}
	DebugPrint("%d: AiAddResearchRequest I: Nothing known about '%s'\n",
	           AiPlayer->Player->Index,
	           upgrade->Ident.c_str());
}

/**
**  Check if we can upgrade to unit-type.
**
**  @param type  Unit that can upgrade to unit-type
**  @param what  To what should be upgraded.
**
**  @return      True if made, false if can't be made.
*/
static bool AiUpgradeTo(const CUnitType &type, CUnitType &what)
{
	if (GameSettings.AiChecksDependencies) {
		if (!CheckDependByType(*AiPlayer->Player, what)) {
			return false;
		}
	}

	// Remove all units already doing something.
	std::vector<CUnit *> table = FindPlayerUnitsByType(*AiPlayer->Player, type, true);
	for (CUnit *unit : table) {
		if (unit->IsIdle()) {
			CommandUpgradeTo(*unit, what, EFlushMode::On);
			return true;
		}
	}
	return false;
}

/**
**  Check if the upgrade-to can be done.
**
**  @param type  FIXME: docu
*/
void AiAddUpgradeToRequest(CUnitType &type)
{
	// Check if resources are available.
	const int resourceNeeded = AiCheckUnitTypeCosts(type);
	if (resourceNeeded) {
		AiPlayer->NeededMask |= resourceNeeded;
		return;
	}
	if (AiPlayer->Player->CheckLimits(type) != ECheckLimit::Ok) {
		return;
	}
	//
	// Check if we have a place for the upgrade to.
	//
	const int n = AiHelpers.Upgrade().size();
	std::vector<std::vector<CUnitType *> > &tablep = AiHelpers.Upgrade();

	if (type.Slot > n) { // Oops not known.
		DebugPrint("%d: AiAddUpgradeToRequest I: Nothing known about '%s'\n",
		           AiPlayer->Player->Index,
		           type.Ident.c_str());
		return;
	}
	std::vector<CUnitType *> &table = tablep[type.Slot];
	if (table.empty()) { // Oops not known.
		DebugPrint("%d: AiAddUpgradeToRequest II: Nothing known about '%s'\n",
		           AiPlayer->Player->Index,
		           type.Ident.c_str());
		return;
	}

	const int *unit_count = AiPlayer->Player->UnitTypesAiActiveCount;
	for (const CUnitType *unitType : table) {
		//
		// The type is available
		//
		if (unit_count[unitType->Slot]) {
			if (AiUpgradeTo(*unitType, type)) {
				return;
			}
		}
	}
}

/**
**  Check what must be built / trained.
*/
static void AiCheckingWork()
{
	// Supply has the highest priority
	if (AiPlayer->NeedSupply) {
		if (AiPlayer->UnitTypeBuilt.empty() || AiPlayer->UnitTypeBuilt[0].Type->Stats[AiPlayer->Player->Index].Variables[SUPPLY_INDEX].Value == 0) {
			AiPlayer->NeedSupply = false;
			AiRequestSupply();
		}
	}
	// Look to the build requests, what can be done.
	const int sz = AiPlayer->UnitTypeBuilt.size();
	for (int i = 0; i < sz; ++i) {
		AiBuildQueue &queue = AiPlayer->UnitTypeBuilt[AiPlayer->UnitTypeBuilt.size() - sz + i];
		CUnitType &type = *queue.Type;
		bool new_supply = false;

		// FIXME: must check if requirements are fulfilled.
		// Buildings can be destroyed.

		// Check if we have enough food.
		if (type.Stats[AiPlayer->Player->Index].Variables[DEMAND_INDEX].Value && !AiCheckSupply(*AiPlayer, type)) {
			AiPlayer->NeedSupply = true;
			new_supply = true;
		}
		// Check limits, AI should be broken if reached.
		if (queue.Want > queue.Made && AiPlayer->Player->CheckLimits(type) != ECheckLimit::Ok) {
			continue;
		}
		// Check if resources available.
		const int c = AiCheckUnitTypeCosts(type);
		if (c) {
			AiPlayer->NeededMask |= c;
			// NOTE: we can continue and build things with lesser
			//  resource or other resource need!
			continue;
		} else if (queue.Want > queue.Made && queue.Wait <= GameCycle) {
			if (AiMakeUnit(type, queue.Pos)) {
				// AiRequestSupply can change UnitTypeBuilt so recalculate queue
				AiBuildQueue &queue2 = AiPlayer->UnitTypeBuilt[AiPlayer->UnitTypeBuilt.size() - sz + i];
				++queue2.Made;
				queue2.Wait = 0;
			} else if (queue.Type->Building) {
				// Finding a building place is costly, don't try again for a while
				if (queue.Wait == 0) {
					queue.Wait = GameCycle + 150;
				} else {
					queue.Wait = GameCycle + 450;
				}
			}
		}
		if (new_supply) {
			// trigger this last, because it may re-arrange the queue and invalidate our queue item
			AiRequestSupply();
		}
	}
}

/*----------------------------------------------------------------------------
--  WORKERS/RESOURCES
----------------------------------------------------------------------------*/

/**
**  Assign worker to gather a certain resource from terrain.
**
**  @param unit      pointer to the unit.
**  @param resource  resource identification.
**
**  @return          true if the worker was assigned, false otherwise.
*/
static bool AiAssignHarvesterFromTerrain(CUnit &unit, int resource)
{
	// TODO : hardcoded forest
	// Code for terrain harvesters. Search for piece of terrain to mine.
	if (auto forestPos = FindTerrainType(unit.Type->MovementMask, MapFieldForest, 1000, *unit.Player, unit.tilePos)) {
		CommandResourceLoc(unit, *forestPos, EFlushMode::On);
		return true;
	}
	// Ask the AI to explore...
	AiExplore(unit.tilePos, MapFieldLandUnit);

	// Failed.
	return false;
}

/**
**  Assign worker to gather a certain resource from Unit.
**
**  @param unit      pointer to the unit.
**  @param resource  resource identification.
**
**  @return          true if the worker was assigned, false otherwise.
*/
static bool AiAssignHarvesterFromUnit(CUnit &unit, int resource)
{
	// Try to find the nearest depot first.
	CUnit *depot = FindDeposit(unit, 1000, resource);
	// Find a resource to harvest from.
	CUnit *mine = UnitFindResource(unit, depot ? *depot : unit, 1000, resource, true);

	if (mine) {
		CommandResource(unit, *mine, EFlushMode::On);
		return true;
	}

	int exploremask = 0;

	for (const CUnitType *type : getUnitTypes()) {
		if (type && type->GivesResource == resource) {
			switch (type->MoveType) {
				case EMovement::Land: exploremask |= MapFieldLandUnit; break;
				case EMovement::Fly: exploremask |= MapFieldAirUnit; break;
				case EMovement::Naval: exploremask |= MapFieldSeaUnit; break;
				default: Assert(0);
			}
		}
	}
	// Ask the AI to explore
	AiExplore(unit.tilePos, exploremask);
	// Failed.
	return false;
}
/**
**  Assign worker to gather a certain resource.
**
**  @param unit      pointer to the unit.
**  @param resource  resource identification.
**
**  @return          true if the worker was assigned, false otherwise.
*/
static bool AiAssignHarvester(CUnit &unit, int resource)
{
	// It can't.
	if (unit.Removed) {
		return false;
	}

	Assert(unit.Type->ResInfo[resource]);
	const ResourceInfo &resinfo = *unit.Type->ResInfo[resource];

	if (resinfo.TerrainHarvester) {
		return AiAssignHarvesterFromTerrain(unit, resource);
	} else {
		return AiAssignHarvesterFromUnit(unit, resource);
	}
}

static bool CmpWorkers(const CUnit *lhs, const CUnit *rhs)
{
	return lhs->ResourcesHeld < rhs->ResourcesHeld;
}

/**
**  Assign workers to collect resources.
**
**  If we have a shortage of a resource, let many workers collecting this.
**  If no shortage, split workers to all resources.
*/
static void AiCollectResources()
{
	std::vector<CUnit *> units_assigned[MaxCosts]; // Worker assigned to resource
	std::vector<CUnit *> units_unassigned[MaxCosts]; // Unassigned workers
	int num_units_with_resource[MaxCosts]{};
	int num_units_assigned[MaxCosts]{};
	int num_units_unassigned[MaxCosts]{};
	int total_harvester = 0;

	// Collect statistics about the current assignment
	for (CUnit *unit : AiPlayer->Player->GetUnits()) {
		if (!unit->Type->BoolFlag[HARVESTER_INDEX].value) {
			continue;
		}

		// See if it's assigned already
		if (unit->Orders.size() == 1 &&
			unit->CurrentAction() == UnitAction::Resource) {
			const COrder_Resource &order = *static_cast<COrder_Resource *>(unit->CurrentOrder());
			const int c = order.GetCurrentResource();
			units_assigned[c].push_back(unit);
			num_units_assigned[c]++;
			total_harvester++;
			continue;
		}

		// Ignore busy units. ( building, fighting, ... )
		if (!unit->IsIdle()) {
			continue;
		}

		// Send workers with resources back home.
		if (unit->ResourcesHeld) {
			const int c = unit->CurrentResource;

			num_units_with_resource[c]++;
			CommandReturnGoods(*unit, 0, EFlushMode::On);
			total_harvester++;
			continue;
		}

		// Look what the unit can do
		for (int c = 1; c < MaxCosts; ++c) {
			if (unit->Type->ResInfo[c]) {
				units_unassigned[c].push_back(unit);
				num_units_unassigned[c]++;
			}
		}
		++total_harvester;
	}

	if (!total_harvester) {
		return;
	}

	int wanted[MaxCosts]{};
	int percent[MaxCosts]{};
	int priority_resource[MaxCosts]{};
	int priority_needed[MaxCosts]{};

	int percent_total = 100;
	for (int c = 1; c < MaxCosts; ++c) {
		percent[c] = AiPlayer->Collect[c];
		if ((AiPlayer->NeededMask & (1 << c))) { // Double percent if needed
			percent_total += percent[c];
			percent[c] <<= 1;
		}
	}

	// Turn percent values into harvester numbers.
	for (int c = 1; c < MaxCosts; ++c) {
		if (percent[c]) {
			// Wanted needs to be representative.
			if (total_harvester < 5) {
				wanted[c] = 1 + (percent[c] * 5) / percent_total;
			} else {
				wanted[c] = 1 + (percent[c] * total_harvester) / percent_total;
			}
		}
	}

	// Initialise priority & mapping
	for (int c = 0; c < MaxCosts; ++c) {
		priority_resource[c] = c;
		priority_needed[c] = wanted[c] - num_units_assigned[c] - num_units_with_resource[c];

		if (c && num_units_assigned[c] > 1) {
			//first should go workers with lower ResourcesHeld value
			ranges::sort(units_assigned[c], CmpWorkers);
		}
	}
	CUnit *unit;
	do {
		// sort resources by priority
		for (int i = 0; i < MaxCosts; ++i) {
			for (int j = i + 1; j < MaxCosts; ++j) {
				if (priority_needed[j] > priority_needed[i]) {
					std::swap(priority_needed[i], priority_needed[j]);
					std::swap(priority_resource[i], priority_resource[j]);
				}
			}
		}
		unit = nullptr;

		// Try to complete each resource in the priority order
		for (int i = 0; i < MaxCosts; ++i) {
			int c = priority_resource[i];

			// If there is a free worker for c, take it.
			if (num_units_unassigned[c]) {
				// Take the unit.
				while (0 < num_units_unassigned[c] && !AiAssignHarvester(*units_unassigned[c][0], c)) {
					// can't assign to c => remove from units_unassigned !
					units_unassigned[c][0] = units_unassigned[c][--num_units_unassigned[c]];
					units_unassigned[c].pop_back();
				}

				// unit is assigned
				if (0 < num_units_unassigned[c]) {
					unit = units_unassigned[c][0];
					units_unassigned[c][0] = units_unassigned[c][--num_units_unassigned[c]];
					units_unassigned[c].pop_back();

					// remove it from other resources
					for (int j = 0; j < MaxCosts; ++j) {
						if (j == c || !unit->Type->ResInfo[j]) {
							continue;
						}
						for (int k = 0; k < num_units_unassigned[j]; ++k) {
							if (units_unassigned[j][k] == unit) {
								units_unassigned[j][k] = units_unassigned[j][--num_units_unassigned[j]];
								units_unassigned[j].pop_back();
								break;
							}
						}
					}
				}
			}

			// Else : Take from already assigned worker with lower priority.
			if (!unit) {
				// Take from lower priority only (i+1).
				for (int j = i + 1; j < MaxCosts && !unit; ++j) {
					// Try to move worker from src_c to c
					const int src_c = priority_resource[j];

					// Don't complete with lower priority ones...
					if (wanted[src_c] > wanted[c]
						|| (wanted[src_c] == wanted[c]
							&& num_units_assigned[src_c] <= num_units_assigned[c] + 1)) {
						continue;
					}

					for (int k = num_units_assigned[src_c] - 1; k >= 0 && !unit; --k) {
						unit = units_assigned[src_c][k];

						Assert(unit->CurrentAction() == UnitAction::Resource);
						COrder_Resource &order = *static_cast<COrder_Resource *>(unit->CurrentOrder());

						if (order.IsGatheringFinished()) {
							//worker returning with resource
							continue;
						}

						// unit can't harvest : next one
						if (!unit->Type->ResInfo[c] || !AiAssignHarvester(*unit, c)) {
							unit = nullptr;
							continue;
						}

						// Remove from src_c
						units_assigned[src_c][k] = units_assigned[src_c][--num_units_assigned[src_c]];
						units_assigned[src_c].pop_back();

						// j need one more
						priority_needed[j]++;
					}
				}
			}

			// We just moved an unit. Adjust priority & retry
			if (unit) {
				// i got a new unit.
				priority_needed[i]--;
				// Recompute priority now
				break;
			}
		}
	} while (unit);

	// Unassigned units there can't be assigned ( ie : they can't move to resource )
	// IDEA : use transporter here.
}

/*----------------------------------------------------------------------------
--  WORKERS/REPAIR
----------------------------------------------------------------------------*/

static bool IsReadyToRepair(const CUnit &unit)
{
	if (unit.IsIdle()) {
		return true;
	} else if (unit.Orders.size() == 1 && unit.CurrentAction() == UnitAction::Resource) {
		COrder_Resource &order = *static_cast<COrder_Resource *>(unit.CurrentOrder());

		if (order.IsGatheringStarted() == false) {
			return true;
		}
	}
	return false;
}

/**
**  Check if we can repair the building.
**
**  @param type      Unit that can repair the building.
**  @param building  Building to be repaired.
**
**  @return          True if can repair, false if can't repair..
*/
static bool AiRepairBuilding(const CPlayer &player, const CUnitType &type, CUnit &building)
{
	if (type.RepairRange == 0) {
		return false;
	}

	// We need to send all nearby free workers to repair that building
	// AI shouldn't send workers that are far away from repair point
	// Selection of mining workers.
	std::vector<CUnit *> candidates = FindPlayerUnitsByType(player, type, true);
	ranges::erase_if(candidates, [](const CUnit *unit) { return !IsReadyToRepair(*unit); });
	const int maxRange = 15;
	if (CUnit *unit = UnitFinder::find(candidates, maxRange, building)) {
		const Vec2i invalidPos(-1, -1);
		CommandRepair(*unit, invalidPos, &building, EFlushMode::On);
		return true;
	}
	return false;
}

/**
**  Check if we can repair this unit.
**
**  @param unit  Unit that must be repaired.
**
**  @return      True if made, false if can't be made.
*/
static bool AiRepairUnit(CUnit &unit)
{
	int n = AiHelpers.Repair().size();
	std::vector<std::vector<CUnitType *> > &tablep = AiHelpers.Repair();
	const CUnitType &type = *unit.Type;
	if (type.Slot > n) { // Oops not known.
		DebugPrint("%d: AiRepairUnit I: Nothing known about '%s'\n",
		           AiPlayer->Player->Index,
		           type.Ident.c_str());
		return false;
	}
	std::vector<CUnitType *> &table = tablep[type.Slot];
	if (table.empty()) { // Oops not known.
		DebugPrint("%d: AiRepairUnit II: Nothing known about '%s'\n",
		           AiPlayer->Player->Index,
		           type.Ident.c_str());
		return false;
	}

	const int *unit_count = AiPlayer->Player->UnitTypesAiActiveCount;
	for (const CUnitType *unitType : table) {
		//
		// The type is available
		//
		if (unit_count[unitType->Slot]) {
			if (AiRepairBuilding(*AiPlayer->Player, *unitType, unit)) {
				return true;
			}
		}
	}
	return false;
}

/**
**  Check if there's a unit that should be repaired.
*/
static void AiCheckRepair()
{
	const int n = AiPlayer->Player->GetUnitCount();
	int k = 0;

	// Selector for next unit
	for (int i = n - 1; i >= 0; --i) {
		const CUnit &unit = AiPlayer->Player->GetUnit(i);
		if (UnitNumber(unit) == AiPlayer->LastRepairBuilding) {
			k = i + 1;
		}
	}

	for (int i = k; i < n; ++i) {
		CUnit &unit = AiPlayer->Player->GetUnit(i);
		bool repair_flag = true;

		if (!unit.IsAliveOnMap()) {
			continue;
		}

		// Unit damaged?
		// Don't repair attacked unit (wait 5 sec before repairing)
		if (unit.Type->RepairHP
			&& unit.CurrentAction() != UnitAction::Built
			&& unit.CurrentAction() != UnitAction::UpgradeTo
			&& unit.Variable[HP_INDEX].Value < unit.Variable[HP_INDEX].Max
			&& unit.Attacked + 5 * CYCLES_PER_SECOND < GameCycle) {
			//
			// FIXME: Repair only units under control
			//
			if (AiEnemyUnitsInDistance(unit, unit.Stats->Variables[SIGHTRANGE_INDEX].Max)) {
				continue;
			}
			//
			// Must check, if there are enough resources
			//
			for (int j = 1; j < MaxCosts; ++j) {
				if (unit.Stats->Costs[j]
					&& (AiPlayer->Player->Resources[j] + AiPlayer->Player->StoredResources[j])  < 99) {
					repair_flag = false;
					break;
				}
			}

			//
			// Find a free worker, who can build this building can repair it?
			//
			if (repair_flag) {
				AiRepairUnit(unit);
				AiPlayer->LastRepairBuilding = UnitNumber(unit);
				return;
			}
		}
		// Building under construction but no worker
		if (unit.CurrentAction() == UnitAction::Built) {
			bool has_repairing_worker = ranges::any_of(AiPlayer->Player->GetUnits(), [&](const CUnit *punit) {
				COrder *order = punit->CurrentOrder();
				if (order->Action == UnitAction::Repair) {
					COrder_Repair &orderRepair = *static_cast<COrder_Repair *>(order);

					return orderRepair.GetReparableTarget() == &unit;
				}
				return false;
			});
			if (!has_repairing_worker) {
				int j;
				// Make sure we have enough resources first
				for (j = 0; j < MaxCosts; ++j) {
					// FIXME: the resources don't necessarily have to be in storage
					if (AiPlayer->Player->Resources[j] + AiPlayer->Player->StoredResources[j] < unit.Stats->Costs[j]) {
						break;
					}
				}
				if (j == MaxCosts) {
					AiRepairUnit(unit);
					AiPlayer->LastRepairBuilding = UnitNumber(unit);
					return;
				}
			}
		}
	}
	AiPlayer->LastRepairBuilding = 0;
}

/**
**  Add unit-type request to resource manager.
**
**  @param type   Unit type requested.
**  @param count  How many units.
**
**  @todo         FIXME: should store the end of list and not search it.
*/
void AiAddUnitTypeRequest(CUnitType &type, int count)
{
	AiBuildQueue queue;

	queue.Type = &type;
	queue.Want = count;
	queue.Made = 0;
	AiPlayer->UnitTypeBuilt.push_back(queue);
}

/**
**  Mark that a zone is requiring exploration.
**
**  @param pos   Pos of the zone
**  @param mask  Mask to explore ( land/sea/air )
*/
void AiExplore(const Vec2i &pos, int mask)
{
	if (!GameSettings.AiExplores) {
		return;
	}
	AiExplorationRequest req(pos, mask);

	// Link into the exploration requests list
	AiPlayer->FirstExplorationRequest.insert(
		AiPlayer->FirstExplorationRequest.begin(), req);
}

/**
**  Entry point of resource manager, periodically called.
*/
void AiResourceManager()
{
	// Check if something needs to be build / trained.
	AiCheckingWork();

	// Look if we can build a farm in advance.
	if (!AiPlayer->NeedSupply && AiPlayer->Player->Supply == AiPlayer->Player->Demand) {
		AiRequestSupply();
	}

	// Collect resources.
	if ((GameCycle / CYCLES_PER_SECOND) % COLLECT_RESOURCES_INTERVAL ==
		(unsigned long)AiPlayer->Player->Index % COLLECT_RESOURCES_INTERVAL) {
		AiCollectResources();
	}

	// Check repair.
	AiCheckRepair();

	AiPlayer->NeededMask = 0;
}

//@}
