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

#ifndef __TRC_RENDERER_H__
#define __TRC_RENDERER_H__

#include <stdbool.h>
#include <stdint.h>

#include "canvas.h"
#include "gamestate.h"

#define NATIVE_RESOLUTION_X 480
#define NATIVE_RESOLUTION_Y 352

struct trc_render_options {
    /* Game render resolution, excluding side-bar area. */
    int Width;
    int Height;

    bool SkipRenderingCreatures : 1;
    bool SkipRenderingItems : 1;

    bool SkipRenderingGraphicalEffects : 1;
    bool SkipRenderingNumericalEffects : 1;
    bool SkipRenderingMissiles : 1;

    bool SkipRenderingCreatureHealthBars : 1;
    bool SkipRenderingCreatureIcons : 1;
    bool SkipRenderingNonPlayerNames : 1;
    bool SkipRenderingPlayerNames : 1;

    bool SkipRenderingYellingMessages : 1;
    bool SkipRenderingMessages : 1;

    bool SkipRenderingUpperFloors : 1;
    bool SkipRenderingStatusBars : 1;

    bool SkipRenderingPrivateMessages : 1;
    bool SkipRenderingHotkeyMessages : 1;
    bool SkipRenderingStatusMessages : 1;
    bool SkipRenderingSpellMessages : 1;
    bool SkipRenderingLootMessages : 1;

    bool SkipRenderingInventory : 1;
    bool SkipRenderingIconBar : 1;
};

bool renderer_DrawInventoryArea(const struct trc_render_options *options,
                                struct trc_game_state *gamestate,
                                struct trc_canvas *canvas,
                                int *offsetX,
                                int *offsetY);

bool renderer_DrawIconBar(const struct trc_render_options *options,
                          struct trc_game_state *gamestate,
                          struct trc_canvas *canvas,
                          int *offsetX,
                          int *offsetY);

bool renderer_DrawStatusBars(const struct trc_render_options *options,
                             struct trc_game_state *gamestate,
                             struct trc_canvas *canvas,
                             int *offsetX,
                             int *offsetY);

bool renderer_DrawContainer(const struct trc_render_options *options,
                            struct trc_game_state *gamestate,
                            struct trc_canvas *canvas,
                            struct trc_container *container,
                            bool collapsed,
                            int maxX,
                            int maxY,
                            int *offsetX,
                            int *offsetY);

void renderer_RenderClientBackground(struct trc_game_state *gamestate,
                                     struct trc_canvas *canvas,
                                     int topX,
                                     int topY,
                                     int rightX,
                                     int rightY);

/* ************************************************************************* */

bool renderer_DrawGamestate(const struct trc_render_options *options,
                            struct trc_game_state *gamestate,
                            struct trc_canvas *canvas);

bool renderer_DrawOverlay(const struct trc_render_options *options,
                          struct trc_game_state *gamestate,
                          struct trc_canvas *canvas);

bool renderer_DumpItem(struct trc_version *version,
                       uint16_t item,
                       struct trc_canvas *canvas);

#endif /* __TRC_RENDERER_H__ */
