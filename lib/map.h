/*
 * Copyright 2011-2016 "Silver Squirrel Software Handelsbolag"
 * Copyright 2023-2024 "John Högberg"
 *
 * This file is part of tibiarc.
 *
 * tibiarc is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * tibiarc is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for more
 * details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with tibiarc. If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef __TRC_MAP_H__
#define __TRC_MAP_H__

#include <stdint.h>

#include "effect.h"
#include "position.h"
#include "tile.h"

#include "utils.h"

#define TILE_BUFFER_WIDTH 18
#define TILE_BUFFER_HEIGHT 14
#define TILE_BUFFER_DEPTH 8
#define TILE_BUFFER_SIZE                                                       \
    (TILE_BUFFER_WIDTH * TILE_BUFFER_HEIGHT * TILE_BUFFER_DEPTH)

#define RENDER_HEIGHTMAP_SIZE (TILE_BUFFER_WIDTH + 2) * (TILE_BUFFER_HEIGHT + 2)

struct trc_map {
    uint8_t LightIntensity;
    uint8_t LightColor;

    struct trc_position Position;
    struct trc_tile Tiles[TILE_BUFFER_SIZE];

    uint8_t RenderHeightMap[RENDER_HEIGHTMAP_SIZE];
};

TRC_UNUSED static struct trc_tile *map_GetTile(struct trc_map *map,
                                               int X,
                                               int Y,
                                               int Z) {
    X %= TILE_BUFFER_WIDTH;
    Y %= TILE_BUFFER_HEIGHT;
    Z %= TILE_BUFFER_DEPTH;

    return &map->Tiles[X + (Y + (Z * TILE_BUFFER_HEIGHT)) * TILE_BUFFER_WIDTH];
}

TRC_UNUSED static uint8_t map_GetRenderHeight(struct trc_map *map,
                                              int rX,
                                              int bY) {
    ASSERT((rX / 32) + ((bY / 32) * TILE_BUFFER_WIDTH) <=
           RENDER_HEIGHTMAP_SIZE);
    return map->RenderHeightMap[(rX / 32) + ((bY / 32) * TILE_BUFFER_WIDTH)];
}

TRC_UNUSED static void map_UpdateRenderHeight(struct trc_map *map,
                                              int rX,
                                              int bY,
                                              uint8_t Z) {
    ASSERT((rX / 32) + ((bY / 32) * TILE_BUFFER_WIDTH) <=
           RENDER_HEIGHTMAP_SIZE);
    map->RenderHeightMap[(rX / 32) + ((bY / 32) * TILE_BUFFER_WIDTH)] = Z;
}

#endif /* __TRC_MAP_H__ */
