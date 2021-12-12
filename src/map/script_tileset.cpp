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
/**@name script_tileset.cpp - The tileset ccl functions. */
//
//      (c) Copyright 2000-2007 by Lutz Sammer, Francois Beerten and Jimmy Salmon
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
#include "SDL_image.h"

#include "tileset.h"
#include "tile.h"
#include "map.h"
#include "video.h"

#include "script.h"
#include <cstring>
#include <math.h>

/*----------------------------------------------------------------------------
--  Functions
----------------------------------------------------------------------------*/

bool  CTileset::ModifyFlag(const char *flagName, tile_flags *flag, const int subtileCount)
{
	const struct {
		const char *name;
		tile_flags flag;
	} flags[] = {
		{"opaque", MapFieldOpaque},
		{"water", MapFieldWaterAllowed},
		{"land", MapFieldLandAllowed},
		{"coast", MapFieldCoastAllowed},
		{"no-building", MapFieldNoBuilding},
		{"unpassable", MapFieldUnpassable},
		{"wall", MapFieldWall},
		{"rock", MapFieldRocks},
		{"forest", MapFieldForest},
		{"cost4", MapFieldCost4},
		{"cost5", MapFieldCost5},
		{"cost6", MapFieldCost6},
		{"land-unit", MapFieldLandUnit},
		{"air-unit", MapFieldAirUnit},
		{"sea-unit", MapFieldSeaUnit},
		{"building", MapFieldBuilding},
		{"human", MapFieldHuman},
		{"decorative", MapFieldDecorative},
		{"non-mixing", MapFieldNonMixing}
	};

	for (unsigned int i = 0; i != sizeof(flags) / sizeof(*flags); ++i) {
		if (!strcmp(flagName, flags[i].name)) {
			*flag |= flags[i].flag;
			return true;
		}
	}

	const struct {
		const char *name;
		unsigned int speed;
	} speeds[] = {
		{"fastest", 0},
		{"fast", 1},
		{"slow", 2},
		{"slower", 3}
	};
	for (unsigned int i = 0; i != sizeof(speeds) / sizeof(*speeds); ++i) {
		if (!strcmp(flagName, speeds[i].name)) {
			*flag = (*flag & ~MapFieldSpeedMask) | speeds[i].speed;
			return true;
		}
	}

	if (flagName[0] == 'p' || flagName[0] == 'u') {
		if (strlen(flagName) != subtileCount) {
			return false;
		}
		tile_flags subtileFlags = 0;
		for (int i = 0; i < subtileCount; i++) {
			if (flagName[i] == 'u') {
				subtileFlags |= (1 << i);
			} else if (flagName[i] != 'p') {
				return false;
			}
		}
		*flag |= (subtileFlags << MapFieldSubtilesUnpassableShift);
		return true;
	}
	return false;
}

/**
**  Parse the flag section of a tile definition.
**
**  @param l     Lua state.
**  @param j     pointer for the location in the array. in and out
**
**  @return     parsed set of flags
**
*/
tile_flags CTileset::parseTilesetTileFlags(lua_State *l, int *j)
{
	tile_flags flags = 3;

	//  Parse the list: flags of the slot
	while (1) {
		lua_rawgeti(l, -1, *j + 1);
		if (!lua_isstring(l, -1)) {
			lua_pop(l, 1);
			break;
		}
		++(*j);
		const char *value = LuaToString(l, -1);
		lua_pop(l, 1);

		//  Flags are mostly needed for the editor
		if (ModifyFlag(value, &flags, logicalTileToGraphicalTileMultiplier * logicalTileToGraphicalTileMultiplier) == false) {
			LuaError(l, "solid: unsupported tag: %s" _C_ value);
		}
	}
	
	if (flags & MapFieldNonMixing) {
		flags |= MapFieldDecorative;
	}
	return flags;
}

/**
**  Parse the special slot part of a tileset definition
**
**  @param l        Lua state.
*/
void CTileset::parseSpecial(lua_State *l)
{
	if (!lua_istable(l, -1)) {
		LuaError(l, "incorrect argument");
	}
	const int args = lua_rawlen(l, -1);

	for (int j = 0; j < args; ++j) {
		const char *value = LuaToString(l, -1, j + 1);

		if (!strcmp(value, "top-one-tree")) {
			++j;
			topOneTreeTile = LuaToNumber(l, -1, j + 1);
		} else if (!strcmp(value, "mid-one-tree")) {
			++j;
			midOneTreeTile = LuaToNumber(l, -1, j + 1);
		} else if (!strcmp(value, "bot-one-tree")) {
			++j;
			botOneTreeTile = LuaToNumber(l, -1, j + 1);
		} else if (!strcmp(value, "removed-tree")) {
			++j;
			removedTreeTile = LuaToNumber(l, -1, j + 1);
		} else if (!strcmp(value, "growing-tree")) {
			++j;
			// keep for retro compatibility.
			// TODO : remove when game data are updated.
		} else if (!strcmp(value, "top-one-rock")) {
			++j;
			topOneRockTile = LuaToNumber(l, -1, j + 1);
		} else if (!strcmp(value, "mid-one-rock")) {
			++j;
			midOneRockTile = LuaToNumber(l, -1, j + 1);
		} else if (!strcmp(value, "bot-one-rock")) {
			++j;
			botOneRockTile = LuaToNumber(l, -1, j + 1);
		} else if (!strcmp(value, "removed-rock")) {
			++j;
			removedRockTile = LuaToNumber(l, -1, j + 1);
		} else {
			LuaError(l, "special: unsupported tag: %s" _C_ value);
		}
	}
}

/**
**  Parse the solid slot part of a tileset definition
**
**  @param l        Lua state.
*/
void CTileset::parseSolid(lua_State *l)
{
	const tile_index index = getTileCount();

	if (!increaseTileCountBy(16)) {
		LuaError(l, "Number of tiles limit has been reached.");
	}
	if (!lua_istable(l, -1)) {
		LuaError(l, "incorrect argument");
	}

	int j = 0;
	const terrain_typeIdx basic_name = getOrAddSolidTileIndexByName(LuaToString(l, -1, j + 1));
	++j;

	const tile_flags flagsCommon = parseTilesetTileFlags(l, &j);
	
	if (flagsCommon & MapFieldDecorative) {
		LuaError(l, "cannot set a decorative flag / custom basename in the main set of flags");
	}

	//  Vector: the tiles.
	lua_rawgeti(l, -1, j + 1);
	if (!lua_istable(l, -1)) {
		LuaError(l, "incorrect argument");
	}
	const int len = lua_rawlen(l, -1);

	j = 0;
	for (int i = 0; i < len; ++i, ++j) {
		lua_rawgeti(l, -1, i + 1);
		if (lua_istable(l, -1)) {
			int k = 0;
			const tile_flags tile_flag = parseTilesetTileFlags(l, &k);
			--j;
			lua_pop(l, 1);
			tiles[index + j].flag = tile_flag;

			if (tile_flag & MapFieldDecorative) {
				tiles[index + j].tileinfo.BaseTerrain = addDecoTerrainType();
			}
			continue;
		}
		const int pud = LuaToNumber(l, -1);
		lua_pop(l, 1);

		// ugly hack for sc tilesets, remove when fixed
		if (j > 15) {
			if (!increaseTileCountBy(j)) {
				LuaError(l, "Number of tiles limit has been reached.");
			}
		}
		CTile &tile = tiles[index + j];

		tile.tile = pud;
		tile.flag = flagsCommon;
		tile.tileinfo.BaseTerrain = basic_name;
		tile.tileinfo.MixTerrain = 0;
	}
	lua_pop(l, 1);
}

/**
**  Parse the mixed slot part of a tileset definition
**
**  @param l        Lua state.
*/
void CTileset::parseMixed(lua_State *l)
{
	tile_index index = getTileCount();

	if (!increaseTileCountBy(256)) {
		LuaError(l, "Number of tiles limit has been reached.");
	}

	if (!lua_istable(l, -1)) {
		LuaError(l, "incorrect argument");
	}

	if (!lua_istable(l, -1)) {
		LuaError(l, "incorrect argument");
	}
	int j = 0;
	const int args = lua_rawlen(l, -1);
	const terrain_typeIdx basic_name = getOrAddSolidTileIndexByName(LuaToString(l, -1, j + 1));
	++j;
	const terrain_typeIdx mixed_name = getOrAddSolidTileIndexByName(LuaToString(l, -1, j + 1));
	++j;

	const tile_flags flagsCommon = parseTilesetTileFlags(l, &j);
	
	if (flagsCommon & MapFieldDecorative) {
		LuaError(l, "cannot set a decorative flag / custom basename in the main set of flags");
	}

	for (; j < args; ++j) {
		lua_rawgeti(l, -1, j + 1);
		if (!lua_istable(l, -1)) {
			LuaError(l, "incorrect argument");
		}
		//  Vector: the tiles.
		const int len = lua_rawlen(l, -1);
		for (int i = 0; i < len; ++i) {
			const int pud = LuaToNumber(l, -1, i + 1);
			CTile &tile = tiles[index + i];

			tile.tile = pud;
			tile.flag = flagsCommon;
			tile.tileinfo.BaseTerrain = basic_name;
			tile.tileinfo.MixTerrain = mixed_name;
		}
		index += 16;
		lua_pop(l, 1);
	}
}

/**
**  Parse the slot part of a tileset definition
**
**  @param l        Lua state.
**  @param t        FIXME: docu
*/
void CTileset::parseSlots(lua_State *l, int t)
{
	tiles.clear();

	//  Parse the list: (still everything could be changed!)
	const int args = lua_rawlen(l, t);
	for (int j = 0; j < args; ++j) {
		const char *value = LuaToString(l, t, j + 1);
		++j;

		if (!strcmp(value, "special")) {
			lua_rawgeti(l, t, j + 1);
			parseSpecial(l);
			lua_pop(l, 1);
		} else if (!strcmp(value, "solid")) {
			lua_rawgeti(l, t, j + 1);
			parseSolid(l);
			lua_pop(l, 1);
		} else if (!strcmp(value, "mixed")) {
			lua_rawgeti(l, t, j + 1);
			parseMixed(l);
			lua_pop(l, 1);
		} else {
			LuaError(l, "slots: unsupported tag: %s" _C_ value);
		}
	}
}

void CTileset::parse(lua_State *l)
{
	clear();

	this->pixelTileSize.x = PixelTileSize.x;
	this->pixelTileSize.y = PixelTileSize.y;

	const int args = lua_gettop(l);
	for (int j = 1; j < args; ++j) {
		const char *value = LuaToString(l, j);
		++j;

		if (!strcmp(value, "name")) {
			this->Name = LuaToString(l, j);
		} else if (!strcmp(value, "image")) {
			this->ImageFile = LuaToString(l, j);
		} else if (!strcmp(value, "size")) {
			CclGetPos(l, &this->pixelTileSize.x, &this->pixelTileSize.y, j);
		} else if (!strcmp(value, "slots")) {
			// must be deferred to later, after the size is parsed
		} else {
			LuaError(l, "Unsupported tag: %s" _C_ value);
		}
	}

	// precalculate some other representations for performance in hot loops later
	double sizeShiftX = std::log2(this->pixelTileSize.x);
	this->graphicalTileSizeShiftX = std::lround(sizeShiftX);
	if (sizeShiftX != this->graphicalTileSizeShiftX) {
		LuaError(l, "graphical tile size x %d must be a power of 2" _C_ this->pixelTileSize.x);
	}

	double sizeShiftY = std::log2(this->pixelTileSize.y);
	this->graphicalTileSizeShiftY = std::lround(sizeShiftY);
	if (sizeShiftX != this->graphicalTileSizeShiftY) {
		LuaError(l, "graphical tile size y %d must be a power of 2" _C_ this->pixelTileSize.y);
	}

	int multiplier = this->pixelTileSize.x / PixelTileSize.x;
	if (multiplier != this->pixelTileSize.y / PixelTileSize.y) {
		LuaError(l, "logical tile sizes must use the same subdivision in x and y, not %d and %d" _C_ multiplier _C_ (this->pixelTileSize.y / PixelTileSize.y));
	}
	if (PixelTileSize.x * multiplier != this->pixelTileSize.x) {
		LuaError(l, "graphical tile size x %d must be a multiple of logical tile size %d" _C_ this->pixelTileSize.x _C_ PixelTileSize.x);
	}
	if (PixelTileSize.y * multiplier != this->pixelTileSize.y) {
		LuaError(l, "graphical tile size y %d must be a multiple of logical tile size %d" _C_ this->pixelTileSize.y _C_ PixelTileSize.y);
	}
	this->logicalTileToGraphicalTileMultiplier = multiplier;

	double logicalSizePowerX = std::log2(PixelTileSize.x);
	if (logicalSizePowerX != std::lround(logicalSizePowerX)) {
		LuaError(l, "logical tile size x %d must be a power of 2" _C_ PixelTileSize.x);
	}
	double logicalSizePowerY = std::log2(PixelTileSize.y);
	if (logicalSizePowerY != std::lround(logicalSizePowerY)) {
		LuaError(l, "logical tile size y %d must be a power of 2" _C_ PixelTileSize.y);
	}
	long logicalToGraphicalShift = std::lround(sizeShiftX - logicalSizePowerX);
	if (logicalToGraphicalShift < 0) {
		LuaError(l, "logical tile size must be smaller than graphical tile size");
	}
	if (std::lround(sizeShiftY - logicalSizePowerY) != logicalToGraphicalShift) {
		LuaError(l, "logical tile size x and y must be the same shiftable by the same amount to get to the graphical tile size");
	}
	this->logicalTileToGraphicalTileShift = logicalToGraphicalShift;

	for (int j = 1; j < args; ++j) {
		const char *value = LuaToString(l, j);
		++j;

		if (!strcmp(value, "name")) {
			// done
		} else if (!strcmp(value, "image")) {
			// done
		} else if (!strcmp(value, "size")) {
			// done
		} else if (!strcmp(value, "slots")) {
			if (!lua_istable(l, j)) {
				LuaError(l, "incorrect argument");
			}
			parseSlots(l, j);
		} else {
			LuaError(l, "Unsupported tag: %s" _C_ value);
		}
	}
}

void CTileset::buildTable(lua_State *l)
{
	//  Calculate number of tiles in graphic tile
	const size_t n = getTileCount();

	mixedLookupTable.clear();
	mixedLookupTable.resize(n, 0);
	//  Build the TileTypeTable
	TileTypeTable.resize(n, 0);

	for (auto &currTile : tiles) {		
		const graphic_index tile = currTile.tile;
		if (tile == 0) {
			continue;
		}
		const tile_flags flag = currTile.flag;
		if (flag & MapFieldWaterAllowed) {
			TileTypeTable[tile] = TileTypeWater;
		} else if (flag & MapFieldCoastAllowed) {
			TileTypeTable[tile] = TileTypeCoast;
		} else if (flag & MapFieldWall) {
			if (flag & MapFieldHuman) {
				TileTypeTable[tile] = TileTypeHumanWall;
			} else {
				TileTypeTable[tile] = TileTypeOrcWall;
			}
		} else if (flag & MapFieldRocks) {
			TileTypeTable[tile] = TileTypeRock;
		} else if (flag & MapFieldForest) {
			TileTypeTable[tile] = TileTypeWood;
		}
	}
	//  mark the special tiles
	if (topOneTreeTile) {
		TileTypeTable[topOneTreeTile] = TileTypeWood;
	}
	if (midOneTreeTile) {
		TileTypeTable[midOneTreeTile] = TileTypeWood;
	}
	if (botOneTreeTile) {
		TileTypeTable[botOneTreeTile] = TileTypeWood;
	}
	if (topOneRockTile) {
		TileTypeTable[topOneRockTile] = TileTypeRock;
	}
	if (midOneRockTile) {
		TileTypeTable[midOneRockTile] = TileTypeRock;
	}
	if (botOneRockTile) {
		TileTypeTable[botOneRockTile] = TileTypeRock;
	}

	//  Build wood removement table.
	tile_index solid = 0;
	tile_index mixed = 0;
	for (size_t i = 0; i < n;) {
		const CTile &tile = tiles[i];
		const CTileInfo &tileinfo = tile.tileinfo;
		if (tileinfo.BaseTerrain && tileinfo.MixTerrain) {
			if (tile.flag & MapFieldForest) {
				mixed = i;
			}
			i += 256;
		} else {
			if (tileinfo.BaseTerrain != 0 && tileinfo.MixTerrain == 0) {
				if (tile.flag & MapFieldForest) {
					solid = i;
				}
			}
			i += 16;
		}
	}
	woodTable[ 0] = -1;
	woodTable[ 1] = tiles[mixed + 0x30].tile;
	woodTable[ 2] = tiles[mixed + 0x70].tile;
	woodTable[ 3] = tiles[mixed + 0xB0].tile;
	woodTable[ 4] = tiles[mixed + 0x10].tile;
	woodTable[ 5] = tiles[mixed + 0x50].tile;
	woodTable[ 6] = tiles[mixed + 0x90].tile;
	woodTable[ 7] = tiles[mixed + 0xD0].tile;
	woodTable[ 8] = tiles[mixed + 0x00].tile;
	woodTable[ 9] = tiles[mixed + 0x40].tile;
	woodTable[10] = tiles[mixed + 0x80].tile;
	woodTable[11] = tiles[mixed + 0xC0].tile;
	woodTable[12] = tiles[mixed + 0x20].tile;
	woodTable[13] = tiles[mixed + 0x60].tile;
	woodTable[14] = tiles[mixed + 0xA0].tile;
	woodTable[15] = tiles[solid].tile;
	woodTable[16] = -1;
	woodTable[17] = botOneTreeTile;
	woodTable[18] = topOneTreeTile;
	woodTable[19] = midOneTreeTile;

	//Mark which corners of each tile has tree in it.
	//All corners for solid tiles. (Same for rocks)
	//1 Bottom Left
	//2 Bottom Right
	//4 Top Right
	//8 Top Left
	//16 Bottom Tree Tile
	//32 Top Tree Tile
	for (size_t i = solid; i < solid + 16; ++i) {
		mixedLookupTable[tiles[i].tile] = 15;
	}
	for (size_t i = mixed; i < mixed + 256; ++i) {
		int check = int((i - mixed) / 16);

		switch (check) {
			case 0: mixedLookupTable[tiles[i].tile] = 8; break;
			case 1: mixedLookupTable[tiles[i].tile] = 4; break;
			case 2: mixedLookupTable[tiles[i].tile] = 8 + 4; break;
			case 3: mixedLookupTable[tiles[i].tile] = 1; break;
			case 4: mixedLookupTable[tiles[i].tile] = 8 + 1; break;
			case 5: mixedLookupTable[tiles[i].tile] = 4 + 1; break;
			case 6: mixedLookupTable[tiles[i].tile] = 8 + 4 + 1; break;
			case 7: mixedLookupTable[tiles[i].tile] = 2; break;
			case 8: mixedLookupTable[tiles[i].tile] = 8 + 2; break;
			case 9: mixedLookupTable[tiles[i].tile] = 4 + 2; break;
			case 10: mixedLookupTable[tiles[i].tile] = 8 + 4 + 2; break;
			case 11: mixedLookupTable[tiles[i].tile] = 2 + 1; break;
			case 12: mixedLookupTable[tiles[i].tile] = 8 + 2 + 1; break;
			case 13:  mixedLookupTable[tiles[i].tile] = 4 + 2 + 1; break;
			default: mixedLookupTable[tiles[i].tile] = 0; break;
		}
	}
	//16 Bottom Tree Special
	//32 Top Tree Special
	//64 Mid tree special - differentiate with mixed tiles.
	mixedLookupTable[botOneTreeTile] = 12 + 16;
	mixedLookupTable[topOneTreeTile] = 3 + 32;
	mixedLookupTable[midOneTreeTile] = 15 + 48;

	//  Build rock removement table.
	mixed = 0;
	solid = 0;
	for (size_t i = 0; i < n;) {
		const CTile &tile = tiles[i];
		const CTileInfo &tileinfo = tile.tileinfo;
		if (tileinfo.BaseTerrain && tileinfo.MixTerrain) {
			if (tile.flag & MapFieldRocks) {
				mixed = i;
			}
			i += 256;
		} else {
			if (tileinfo.BaseTerrain != 0 && tileinfo.MixTerrain == 0) {
				if (tile.flag & MapFieldRocks) {
					solid = i;
				}
			}
			i += 16;
		}
	}

	//Mark which corners of each tile has rock in it.
	//All corners for solid tiles.
	//1 Bottom Left
	//2 Bottom Right
	//4 Top Right
	//8 Top Left
	for (size_t i = solid; i < solid + 16; ++i) {
		mixedLookupTable[tiles[i].tile] = 15;
	}
	for (size_t i = mixed; i < mixed + 256; ++i) {
		int check = int((i - mixed) / 16);
		switch (check) {
			case 0: mixedLookupTable[tiles[i].tile] = 8; break;
			case 1: mixedLookupTable[tiles[i].tile] = 4; break;
			case 2: mixedLookupTable[tiles[i].tile] = 8 + 4; break;
			case 3: mixedLookupTable[tiles[i].tile] = 1; break;
			case 4: mixedLookupTable[tiles[i].tile] = 8 + 1; break;
			case 5: mixedLookupTable[tiles[i].tile] = 4 + 1; break;
			case 6: mixedLookupTable[tiles[i].tile] = 8 + 4 + 1; break;
			case 7: mixedLookupTable[tiles[i].tile] = 2; break;
			case 8: mixedLookupTable[tiles[i].tile] = 8 + 2; break;
			case 9: mixedLookupTable[tiles[i].tile] = 4 + 2; break;
			case 10: mixedLookupTable[tiles[i].tile] = 8 + 4 + 2; break;
			case 11: mixedLookupTable[tiles[i].tile] = 2 + 1; break;
			case 12: mixedLookupTable[tiles[i].tile] = 8 + 2 + 1; break;
			case 13: mixedLookupTable[tiles[i].tile] = 4 + 2 + 1; break;
			default: mixedLookupTable[tiles[i].tile] = 0; break;
		}
	}

	mixedLookupTable[botOneRockTile] = 12 + 16;
	mixedLookupTable[topOneRockTile] = 3 + 32;
	mixedLookupTable[midOneRockTile] = 15 + 48;

	rockTable[ 0] = -1;
	rockTable[ 1] = tiles[mixed + 0x30].tile;
	rockTable[ 2] = tiles[mixed + 0x70].tile;
	rockTable[ 3] = tiles[mixed + 0xB0].tile;
	rockTable[ 4] = tiles[mixed + 0x10].tile;
	rockTable[ 5] = tiles[mixed + 0x50].tile;
	rockTable[ 6] = tiles[mixed + 0x90].tile;
	rockTable[ 7] = tiles[mixed + 0xD0].tile;
	rockTable[ 8] = tiles[mixed + 0x00].tile;
	rockTable[ 9] = tiles[mixed + 0x40].tile;
	rockTable[10] = tiles[mixed + 0x80].tile;
	rockTable[11] = tiles[mixed + 0xC0].tile;
	rockTable[12] = tiles[mixed + 0x20].tile;
	rockTable[13] = tiles[mixed + 0x60].tile;
	rockTable[14] = tiles[mixed + 0xA0].tile;
	rockTable[15] = tiles[solid].tile;
	rockTable[16] = -1;
	rockTable[17] = botOneRockTile;
	rockTable[18] = topOneRockTile;
	rockTable[19] = midOneRockTile;

	buildWallReplacementTable();
}

void CTileset::buildWallReplacementTable()
{
	// FIXME: Build wall replacement tables
	humanWallTable[ 0] = 0x090;
	humanWallTable[ 1] = 0x830;
	humanWallTable[ 2] = 0x810;
	humanWallTable[ 3] = 0x850;
	humanWallTable[ 4] = 0x800;
	humanWallTable[ 5] = 0x840;
	humanWallTable[ 6] = 0x820;
	humanWallTable[ 7] = 0x860;
	humanWallTable[ 8] = 0x870;
	humanWallTable[ 9] = 0x8B0;
	humanWallTable[10] = 0x890;
	humanWallTable[11] = 0x8D0;
	humanWallTable[12] = 0x880;
	humanWallTable[13] = 0x8C0;
	humanWallTable[14] = 0x8A0;
	humanWallTable[15] = 0x0B0;

	orcWallTable[ 0] = 0x0A0;
	orcWallTable[ 1] = 0x930;
	orcWallTable[ 2] = 0x910;
	orcWallTable[ 3] = 0x950;
	orcWallTable[ 4] = 0x900;
	orcWallTable[ 5] = 0x940;
	orcWallTable[ 6] = 0x920;
	orcWallTable[ 7] = 0x960;
	orcWallTable[ 8] = 0x970;
	orcWallTable[ 9] = 0x9B0;
	orcWallTable[10] = 0x990;
	orcWallTable[11] = 0x9D0;
	orcWallTable[12] = 0x980;
	orcWallTable[13] = 0x9C0;
	orcWallTable[14] = 0x9A0;
	orcWallTable[15] = 0x0C0;

	// Set destroyed walls to TileTypeUnknown
	for (int i = 0; i < 16; ++i) {
		int n = 0;
		tile_index tileIndex = humanWallTable[i];
		while (tiles[tileIndex].tile) { // Skip good tiles
			++tileIndex;
			++n;
		}
		while (!tiles[tileIndex].tile) { // Skip separator
			++tileIndex;
			++n;
		}
		while (tiles[tileIndex].tile) { // Skip good tiles
			++tileIndex;
			++n;
		}
		while (!tiles[tileIndex].tile) { // Skip separator
			++tileIndex;
			++n;
		}
		while (n < 16 && tiles[tileIndex].tile) {
			TileTypeTable[tiles[tileIndex].tile] = TileTypeUnknown;
			++tileIndex;
			++n;
		}
	}
}

/**
** 	Checks top argument in the lua state for number of layers to parse
**
**	@param luaStack		lua state
**/
uint16_t CTilesetGraphicGenerator::checkForLayers(lua_State *luaStack)
{
	bool isMultipleLayers = false;
	if (lua_istable(luaStack, -1)) {

		lua_rawgeti(luaStack, -1, 1);
		isMultipleLayers = lua_isstring(luaStack, -1) && std::string(LuaToString(luaStack, -1)) == "layers";
		lua_pop(luaStack, 1);

	} else if (!lua_isnumber(luaStack, -1)) {
		LuaError(luaStack, "incorrect argument");
	}
	return isMultipleLayers ? lua_rawlen(luaStack, -1) - 1
							: 1;
}

/**
** Parse top argument in the lua state for range of source indexes
**	
**	tile                          -- tile index (within main tileset) to get graphic from
**	{tile[, tile]...}}            -- set of tiles indexes (within main tileset) to get graphics from
**	{"img", image[, image]...}    -- set of numbers of frames from the "image" file.
**	{["img",] "range", from, to}  -- if "img" then from frame to frame (for "image"),
**								  -- otherwise indexes from tile to tile (within main tileset) to get graphics from
**	{"slot", slot_num}            -- f.e. {"slot", 0x0430} - to take graphics continuously from tiles with indexes of slot 0x0430
**
**	@param luaStack		lua state, top argument to be parsed
**	@param argPos		argument in the table to parse (or 0 if need to parse not a table but just a number)
**	@param isImg		if 'img' tag is exist then it will be setted by this function to true, false otherwise
**	@return 			vector of parsed indexes
**/
std::vector<tile_index> CTilesetGraphicGenerator::parseSrcRange(lua_State *luaStack, const int argPos, bool &isImg)
{
	isImg = false;

	std::vector<tile_index> result;

	if (argPos == 0 && lua_isnumber(luaStack, -1)) {
		result = CTilesetParser::parseTilesRange(luaStack);

	} else if (lua_istable(luaStack, -1)) {

		lua_rawgeti(luaStack, -1, argPos);
		int parseFrom = 1;
		/// check if "img" tag is present
		if (lua_istable(luaStack, -1)) {
			lua_rawgeti(luaStack, -1, 1);
			if (lua_isstring(luaStack, -1)) {
				const std::string parsedValue { LuaToString(luaStack, -1) };
				if (parsedValue == "img") {
					isImg = true;
					parseFrom++;
				}
			}
			lua_pop(luaStack, 1);
		}
		result = CTilesetParser::parseTilesRange(luaStack, parseFrom);
		lua_pop(luaStack, 1);
	} else {
		LuaError(luaStack, "incorrect argument");
	}
	return result;
}

/**
**
** {"do_something", parameter}
** where 'do_something':
** 	"remove"
** 	usage:		{"remove", colors[, colors]..} where 'color':
** 														color		-- single color
** 														{from, to}	-- range of colors
**/
void CTilesetGraphicGenerator::parseModifier(lua_State *luaStack, const int argPos, std::vector<SDL_Surface*> &images)
{
	lua_rawgeti(luaStack, -1, argPos);
	if (!lua_istable(luaStack, -1)) {
		LuaError(luaStack, "incorrect argument");
	}

	lua_pop(luaStack, 1);
}

/** 
**	Parse a layer of source graphics
**
** src_range
** or	
** { src_range [,{"do_something", parameter}...] }
**
**	@param	luaStack 	lua state, top argument to be parsed - it can be table of layers or single layer
**	@param	argPos		position of the layer to parse in the table of layers (or 0 in case of single layer)
**
**/
std::vector<SDL_Surface*> CTilesetGraphicGenerator::parseLayer(lua_State *luaStack, const int argPos)
{
	enum { cSrcIndexOnly = 0, cSrcRange = 1, cModifier = 2 };

	if (argPos != 0) {
		lua_rawgeti(luaStack, -1, argPos);
	}
	int arg = lua_istable(luaStack, -1) ? cSrcRange : cSrcIndexOnly;

	bool isImg {false};
	std::vector<tile_index> srcIndexes { parseSrcRange(luaStack, arg, isImg) };
	std::vector<SDL_Surface*> imgLayer;

	for (auto srcIndex : srcIndexes) {
		const SDL_Surface *srcSurface = SrcGraphic->Surface;
		
		SDL_Surface *img = SDL_CreateRGBSurface(srcSurface->flags,
												SrcTileset->getPixelTileSize().x,
												SrcTileset->getPixelTileSize().y,
                                             	srcSurface->format->BitsPerPixel,
												srcSurface->format->Rmask,
												srcSurface->format->Gmask,
												srcSurface->format->Bmask,
												srcSurface->format->Amask);
		const CGraphic *srcGraphic = isImg ? SrcImgGraphic 
										   : SrcGraphic;
		const graphic_index frame = isImg ? srcIndex 
										  : SrcTileset->tiles[srcIndex].tile;

		srcGraphic->DrawFrame(frame, 0, 0, img);
		imgLayer.push_back(img);
	}
	
	const uint16_t argsNum = lua_rawlen(luaStack, -1);
	arg = cModifier;
	while(arg <= argsNum) {
		parseModifier(luaStack, arg, imgLayer);
		arg++;	
	}
	if (argPos != 0) {
		lua_pop(luaStack, 1);
	}
	return imgLayer;
}

/**
** Parse top argument in the lua state.

	{ "layers",  { src_range [,{"do_something", parameter}...] }, -- layer 1
				 { src_range [,{"do_something", parameter}...] }, -- layer 2
				 ...
				 { src_range [,{"do_something", parameter}...] }  -- layer n
	}
	or
	{ src_range [,{"do_something", parameter}...] }
	or
	src_range

**/
void CTilesetGraphicGenerator::parseExtended(lua_State *luaStack)
{
	enum { cSinglelayer = 0, cFirstLayer = 2 };
	
	if (lua_isnumber(luaStack, -1) || lua_istable(luaStack, -1)) {
		const uint16_t layersNum = checkForLayers(luaStack);
		int arg = layersNum > 1 ? cFirstLayer : cSinglelayer;
		for (uint16_t layer = 1; layer <= layersNum; layer++) {
			SrcImgLayers.push_back(parseLayer(luaStack, arg));
			arg++;
		}
	} else {
		LuaError(luaStack, "incorrect argument");
	}
}

/** 
** Parse range of destination indexes
**
**	tile
**	{tile[, tile,] ...}
**	{"range", from, to}
**	{"slot", slot_num}
**
**	@param luaStack		lua state
**	@param tablePos		position of the table containing range to parse
**	@param argPos		argument in the table to parse
**	@return 			vector of parsed indexes
**
**/
std::vector<tile_index> CTilesetParser::parseDstRange(lua_State *luaStack, const int tablePos, const int argPos)
{
	if (!lua_istable(luaStack, tablePos)) {
		LuaError(luaStack, "incorrect argument");
	}
	lua_rawgeti(luaStack, tablePos, argPos);
	std::vector<tile_index> result { parseTilesRange(luaStack) };
	lua_pop(luaStack, 1);
	return result;
}

/**
**	Parse argument from top of the lua stack for range of tiles
**
**	@param luaStack		lua state
**	@param parseFromPos	if argument to parse is a table, then start to parse from this pos
**	@return 			vector of parsed indexes	
**/
std::vector<tile_index> CTilesetParser::parseTilesRange(lua_State *luaStack, const int parseFromPos/* = 1*/)
{
	std::vector<tile_index> resultSet;
	
	if (lua_isnumber(luaStack, -1)) { 
		/// tile|image
		resultSet.push_back(LuaToUnsignedNumber(luaStack, -1));

	} else if (lua_istable(luaStack, -1)) {
		/**
		{["img", ]tile|image[, tile|image] ...}
		{["img", ]"range", from, to}
		{"slot", slot_num}
		**/
		const uint16_t argsNum = lua_rawlen(luaStack, -1);
		if (argsNum == 0) {
			return resultSet;
		}
		lua_rawgeti(luaStack, -1, parseFromPos);

		if (lua_isnumber(luaStack, -1)) { 
			/// {["img", ]tile|image[, tile|image] ...}
			lua_pop(luaStack, 1);
			
			for (uint16_t arg = parseFromPos; arg <= argsNum; arg++) {
				resultSet.push_back(LuaToUnsignedNumber(luaStack, -1, arg));
			}

		} else if (lua_isstring(luaStack, -1)) {
			lua_pop(luaStack, 1);

			int arg = parseFromPos;
			const std::string rangeType { LuaToString(luaStack, -1, parseFromPos) };

			if (rangeType == "slot") { 
				/// {"slot", slot_num}
				if (argsNum != parseFromPos + 1) {
					LuaError(luaStack, "Tiles range: Wrong num of arguments in {\"slot\", slot_num} construct");
				}

				const tile_index slotNum = LuaToUnsignedNumber(luaStack, -1, ++arg);

				if (slotNum & tile_index(0xF)) {
					LuaError(luaStack, "Tiles range: In {\"slot\", slot_num} construct \'slot_num\' must end with 0");
				}

				resultSet.resize(16);
				ranges::iota(resultSet, slotNum); /// fill vector with incremented (slotNum++) values
					
			} else if (rangeType == "range") {
				/// {["img", ]"range", from, to}
				if (argsNum != parseFromPos + 2) {
					LuaError(luaStack, "Tiles range: Wrong num of arguments in {[\"img\", ]\"range\", from, to} construct");
				}

				const tile_index rangeFrom = LuaToUnsignedNumber(luaStack, -1, ++arg);
				const tile_index rangeTo   = LuaToUnsignedNumber(luaStack, -1, ++arg);

				if (rangeFrom >= rangeTo) {
					LuaError(luaStack, "Tiles range: In {[\"img\", ]\"range\", from, to} construct the condition \'from\' < \'to\' is not met");
				}

				resultSet.resize(rangeFrom - rangeTo + 1);
				ranges::iota(resultSet, rangeFrom); /// fill vector with incremented (rangeFrom++) values

			} else {
				LuaError(luaStack, "Tiles range: unsupported tag: %s" _C_ rangeType);
			}
		} else {
			LuaError(luaStack, "Unsupported tiles range format");
		}
	} else {
		LuaError(luaStack, "Unsupported tiles range format");
	}
	return resultSet;
}

/**
**
    {"terrain-name", ["terrain-name",] [list-of-flags-for-all-tiles-of-this-slot,]
        {dst, src[, additional-flags-list]}
        [, {dst, src[, additional-flags-list]}]
        ...
    }
**/
void CTilesetParser::parseExtendedSlot(lua_State *luaStack, const slot_type slotType)
{
	enum { cBase = 0, cMixed = 1 };
	enum { cDst = 1, cSrc = 2, cAddFlags = 3 };

	terrain_typeIdx terrainNameIdx[2] {0, 0};

	const uint16_t argsNum = lua_rawlen(luaStack, -1);
	int arg = 1;

	/// parse terrain name/names
	if (slotType == slot_type::cSolid) {
		terrainNameIdx[cBase]  = BaseTileset->getOrAddSolidTileIndexByName(LuaToString(luaStack, -1, arg));
	} else if (slotType == slot_type::cMixed) {
		terrainNameIdx[cBase]  =  BaseTileset->getOrAddSolidTileIndexByName(LuaToString(luaStack, -1, arg));
		terrainNameIdx[cMixed] =  BaseTileset->getOrAddSolidTileIndexByName(LuaToString(luaStack, -1, ++arg));
	} else {
		LuaError(luaStack, "Slots: unsupported slot type: %d" _C_ slotType);
	}
	if (BaseTileset->getTerrainName(terrainNameIdx[cBase]) == "unused") {
		return;
	}
	/// parse the flags that are common to every tiles in the slot
	const tile_flags flagsCommon = BaseTileset->parseTilesetTileFlags(luaStack, &arg);
	if (flagsCommon & MapFieldDecorative) {
		LuaError(luaStack, "cannot set a decorative flag / custom basename in the main set of flags");
	}

	while (arg < argsNum) {
		lua_rawgeti(luaStack, -1, ++arg);
		
		std::vector<tile_index> dstTileIndexes { parseDstRange(luaStack, -1, cDst) };
		
		/// load src-graphic-generator
		CTilesetGraphicGenerator srcGraphic(luaStack, -1, cSrc, BaseTileset, BaseGraphic, SrcImgGraphic);

		tile_flags flagsAdditional = 0;
		terrain_typeIdx baseTerrain = terrainNameIdx[cBase];

		if (lua_rawlen(luaStack, -1) > cSrc) {
			int tableArg = cAddFlags;
			flagsAdditional = CTileset::parseTilesetTileFlags(luaStack, &tableArg);
			if (flagsAdditional & MapFieldDecorative) {
				baseTerrain = BaseTileset->addDecoTerrainType();
			}
		}
	
		tile_index srcIndex = 0;
		for (tile_index &dstIndex: dstTileIndexes) {
			/// add new graphic tile into tileGraphic if needed
			SDL_Surface *graphic = srcGraphic.get(srcIndex);
			if (graphic) {
				ExtGraphic.push_back(graphic);
			}
			CTile newTile;
			newTile.tile = graphic ? ExtGraphic.size() - 1 + BaseGraphic->NumFrames
								   : srcGraphic.getIndex(srcIndex);
			newTile.flag = flagsCommon | flagsAdditional;
			newTile.tileinfo.BaseTerrain = baseTerrain;
			newTile.tileinfo.MixTerrain  = terrainNameIdx[cMixed];
			
			ExtTiles.insert({dstIndex, newTile});

			srcIndex++;
		}
		lua_pop(luaStack, 1);
	}
}

void CTilesetParser::parseExtendedSlots(lua_State *luaStack, int arg)
{
	enum { cSlotType = 1, cSlotDefinition = 2 };

	const uint16_t slotsNum = lua_rawlen(luaStack, arg) / 2; // "slot_type", {slot_definitions}

	for (int slot = 0; slot < slotsNum; slot++) {
		const uint16_t slotPos0 = slot * 2;
		const std::string slotType { LuaToString(luaStack, arg, slotPos0 + cSlotType) };
		const slot_type typeIdx = [&slotType]() {	if (slotType == "solid") return slot_type::cSolid;
													else if (slotType == "mixed") return slot_type::cMixed;
													else return slot_type::cUnsupported;
												}();
		lua_rawgeti(luaStack, arg, slotPos0 + cSlotDefinition);
		parseExtendedSlot(luaStack, typeIdx);
		lua_pop(luaStack, 1);
	}
}

/**
**  Parse the extended tileset definition with graphic generation 
**
**  @param luaStack        Lua state.
**
**
  "image", path-to-image-with-tileset-graphic, -- optional for extended tileset
  "slots", {
            slot-type, {"terrain-name", ["terrain-name",] [list-of-flags-for-all-tiles-of-this-slot,]
                         {dst, src[, additional-flags-list]}
                         [, {dst, src[, additional-flags-list]}]
                         ...
                        }
                        ...
          }
*/

void CTilesetParser::parseExtended(lua_State *luaStack)
{
	const int argsNum = lua_gettop(luaStack);
	for (int arg = 1; arg <= argsNum; ++arg) {
		const std::string parsedValue { LuaToString(luaStack, arg) };

		if (parsedValue == "image") {
			const std::string imageFile { LuaToString(luaStack, ++arg) };
			SrcImgGraphic = CGraphic::New(imageFile, BaseTileset->getPixelTileSize().x, 
													 BaseTileset->getPixelTileSize().y);
			SrcImgGraphic->Load();

		} else if (parsedValue == "slots") {
			if (!lua_istable(luaStack, ++arg)) {
				LuaError(luaStack, "incorrect argument");
			}
			parseExtendedSlots(luaStack, arg);
		} else {
			LuaError(luaStack, "Unsupported tag: %s" _C_ parsedValue);
		}
	}
}	

//@}
