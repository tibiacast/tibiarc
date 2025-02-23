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

extern "C" {
#include <SDL.h>
}

#include "playback.hpp"
#include "renderer.hpp"
#include "canvas.hpp"

#include <functional>
#include <memory>

namespace trc {
struct Rendering {
    template <typename T>
    using Wrapper = std::unique_ptr<T, std::function<void(T *)>>;

    Wrapper<SDL_Window> SdlWindow;
    Wrapper<SDL_Renderer> SdlRenderer;

    Renderer::Options RenderOptions;

    /* This texture is the same size as the window. It is static, so we render
     * it only once (during startup or window resize) */
    Wrapper<SDL_Texture> SdlTextureBackground;
    bool BackgroundRendered = false;

    /* This canvas and texture are always NATIVE_RESOLUTION */
    std::unique_ptr<Canvas> CanvasGamestate;
    Wrapper<SDL_Texture> SdlTextureGamestate;

    /* This canvas and texture are always the same size as the window */
    std::unique_ptr<Canvas> CanvasOutput;
    Wrapper<SDL_Texture> SdlTextureOutput;

    /* This rectangle represents where the gamestate texture should be rendered
     * on the output texture, including scaling */
    SDL_Rect OverlaySliceRect;

    uint32_t StatsLastUpdate = 0;
    uint32_t StatsFramesSinceLastUpdate = 0;
    double StatsFPS = 1.0;

    Rendering(int width, int height);

    void HandleResize();
    void Render(Playback &playback);

private:
    Wrapper<SDL_Texture> CreateTexture(int width, int height);
};
} // namespace trc

#endif /* PLAYER_RENDERING_H */
