/*
 * Copyright 2024 "Simon Sandström"
 * Copyright 2024 "John Högberg"
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

#ifndef PLAYER_RENDERING_H
#define PLAYER_RENDERING_H

#include <SDL.h>

#include "playback.h"
#include "renderer.h"
#include "canvas.h"

struct rendering {
    SDL_Window *SdlWindow;
    SDL_Renderer *SdlRenderer;

    struct trc_render_options RenderOptions;

    /* This texture is the same size as the window. It is static, so we render
     * it only once (during startup or window resize) */
    SDL_Texture *SdlTextureBackground;
    bool BackgroundRendered;

    /* This canvas and texture are always NATIVE_RESOLUTION */
    struct trc_canvas CanvasGamestate;
    SDL_Texture *SdlTextureGamestate;

    /* This canvas and texture are always the same size as the window */
    struct trc_canvas CanvasOutput;
    SDL_Texture *SdlTextureOutput;

    /* This rectangle represents where the gamestate texture should be rendered
     * on the output texture, including scaling */
    SDL_Rect OverlaySliceRect;

    uint32_t StatsLastUpdate;
    uint32_t StatsFramesSinceLastUpdate;
    double StatsFPS;
};

bool rendering_Init(struct rendering *rendering,
                    int windowWidth,
                    int windowHeight);
void rendering_Free(struct rendering *rendering);

bool rendering_HandleResize(struct rendering *rendering);
void rendering_Render(struct rendering *rendering, struct playback *playback);

#endif /* PLAYER_RENDERING_H */
