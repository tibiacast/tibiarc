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

#ifndef __TRC_RENDERER_HPP__
#define __TRC_RENDERER_HPP__

#include <cstdint>

#include "canvas.hpp"
#include "gamestate.hpp"

namespace trc {
namespace Renderer {

static constexpr int NativeResolutionX = 480;
static constexpr int NativeResolutionY = 352;

struct Options {
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

/* FIXME: C++ migration, `noexcept` specifiers are there as a shorthand to
 * std::terminate() on data errors, which should've been caught by the
 * parser. */

void DrawInventoryArea(Gamestate &gamestate,
                       Canvas &canvas,
                       int &offsetX,
                       int &offsetY) noexcept;

void DrawIconBar(Gamestate &gamestate,
                 Canvas &canvas,
                 int &offsetX,
                 int &offsetY) noexcept;

void DrawStatusBars(Gamestate &gamestate,
                    Canvas &canvas,
                    int &offsetX,
                    int &offsetY) noexcept;

void DrawContainer(Gamestate &gamestate,
                   Canvas &canvas,
                   Container &container,
                   bool collapsed,
                   int maxX,
                   int maxY,
                   int &offsetX,
                   int &offsetY) noexcept;

void DrawClientBackground(Gamestate &gamestate,
                          Canvas &canvas,
                          int topX,
                          int topY,
                          int rightX,
                          int rightY) noexcept;

/* ************************************************************************* */

void DrawGamestate(const Options &options,
                   Gamestate &gamestate,
                   Canvas &canvas) noexcept;

void DrawOverlay(const Options &options,
                 Gamestate &gamestate,
                 Canvas &canvas) noexcept;

void DumpItem(Version &version, uint16_t item, Canvas &canvas) noexcept;
} // namespace Renderer
} // namespace trc

#endif /* __TRC_RENDERER_HPP__ */
