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
/**@name minimap.h - The minimap headerfile. */
//
//      (c) Copyright 1998-2005 by Lutz Sammer and Jimmy Salmon
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

#ifndef __MINIMAP_H__
#define __MINIMAP_H__

//@{

#include "color.h"
#include "vec2i.h"

class CViewport;

struct SDL_Surface;

/*----------------------------------------------------------------------------
--  Declarations
----------------------------------------------------------------------------*/

class CMinimap
{
	template <const int BPP>
	void UpdateMapTerrain(void *const mpixels, const int mpitch,
						  const void *const tpixels, const int tpitch);

	void UpdateTerrain();

	template <const int BPP>
	void UpdateSeen(void *const pixels, const int pitch);

public:
	CMinimap() : X(0), Y(0), W(0), H(0), XOffset(0), YOffset(0),
		WithTerrain(false), ShowSelected(false),
		Transparent(false), UpdateCache(false),
		MinimapScaleX(0), MinimapScaleY(0) {}

	void SetFogOpacityLevels(const uint8_t explored, const uint8_t revealed, const uint8_t unseen);

	void UpdateXY(const Vec2i &pos);
	void UpdateSeenXY(const Vec2i &) {}
	void Update();
	void Create();
	void Destroy();
	void Draw() const;
	void DrawViewportArea(const CViewport &viewport) const;
	void AddEvent(const Vec2i &pos, IntColor color);

	Vec2i ScreenToTilePos(const PixelPos &screenPos) const;
	PixelPos TilePosToScreenPos(const Vec2i &tilePos) const;

	bool Contains(const PixelPos &screenPos) const;
public:
	int X;
	int Y;
	int W;
	int H;
	int XOffset;
	int YOffset;
	bool WithTerrain;
	bool ShowSelected;
	bool Transparent;
	bool UpdateCache;
	
private:
	// MinimapScale:
	// 32x32 64x64 96x96 128x128 256x256 512x512 ...
	// *4 *2 *4/3   *1 *1/2 *1/4
	int MinimapScaleX;                  /// Minimap scale to fit into window
	int MinimapScaleY;                  /// Minimap scale to fit into window

private:
	struct MinimapSettings
	{
		/// used to draw fog in on the minimap
		uint8_t FogVisibleOpacity	{0x00};
		uint8_t FogExploredOpacity  {0x55};
		uint8_t FogRevealedOpacity  {0xAA};
		uint8_t FogUnseenOpacity 	{0xFF};		
	} Settings;
};

// Minimap surface with units (for software)
extern SDL_Surface *MinimapSurface;
// Minimap surface with terrain only (for software)
//extern SDL_Surface *MinimapTerrainSurface;

//@}

#endif // !__MINIMAP_H__
