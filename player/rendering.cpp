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

#include "rendering.hpp"

#include "versions.hpp"
#include "textrenderer.hpp"

#include <iostream>

namespace trc {
Rendering::Wrapper<SDL_Texture> Rendering::CreateTexture(int width,
                                                         int height) {
    SDL_Texture *texture = SDL_CreateTexture(SdlRenderer.get(),
                                             SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             width,
                                             height);

    AbortUnless(texture != nullptr);
    AbortUnless(!SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND));

    return Rendering::Wrapper<SDL_Texture>(texture, SDL_DestroyTexture);
}

static int formatTime(char *buffer, size_t bufferSize, uint32_t milliseconds) {
    /* TODO: move to utils.hpp or similar? */
    uint32_t seconds = milliseconds / 1000;
    milliseconds %= 1000;
    uint32_t minutes = seconds / 60;
    seconds %= 60;
    uint32_t hours = minutes / 60;
    minutes %= 60;

    return snprintf(buffer,
                    bufferSize,
                    "%02u:%02u:%02u.%03u",
                    hours,
                    minutes,
                    seconds,
                    milliseconds);
}

Rendering::Rendering(int width, int height) {
    int ret = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);

    if (ret != 0) {
        throw NotSupportedError();
    }

    SdlWindow = Wrapper<SDL_Window>(
            SDL_CreateWindow("player",
                             SDL_WINDOWPOS_UNDEFINED,
                             SDL_WINDOWPOS_UNDEFINED,
                             width,
                             height,
                             SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE),
            SDL_DestroyWindow);
    AbortUnless(SdlWindow != nullptr);

    SdlRenderer = Wrapper<SDL_Renderer>(
            SDL_CreateRenderer(SdlWindow.get(),
                               -1,
                               SDL_RENDERER_ACCELERATED |
                                       SDL_RENDERER_PRESENTVSYNC |
                                       SDL_RENDERER_TARGETTEXTURE),
            SDL_DestroyRenderer);
    AbortUnless(SdlRenderer != nullptr);

    AbortUnless(!SDL_SetRenderDrawBlendMode(SdlRenderer.get(),
                                            SDL_BLENDMODE_BLEND));
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "2");

    HandleResize();

    StatsLastUpdate = SDL_GetTicks();
    StatsFramesSinceLastUpdate = 0;
    StatsFPS = 0.0f;
}

void Rendering::HandleResize() {
    int width;
    int height;

    SDL_GetRendererOutputSize(SdlRenderer.get(), &width, &height);

    std::cout << "handle resize: " << width << "x" << height << std::endl;
    if (RenderOptions.Width == width && RenderOptions.Height == height) {
        std::cerr << "bogus resize event?" << std::endl;
        return;
    }

    /* Setup rendering */
    RenderOptions.Width = width;
    RenderOptions.Height = height;

    /* Create canvases, which are (going to be) backed by SDL_Textures */
    SdlTextureBackground = CreateTexture(width, height);
    BackgroundRendered = false;

    /* Gamestate canvas and texture are always the same size, so only need
     * to create them if they aren't already created */
    if (!SdlTextureGamestate) {
        CanvasGamestate = std::make_unique<Canvas>(Renderer::NativeResolutionX,
                                                   Renderer::NativeResolutionY,
                                                   Canvas::Type::External);
        SdlTextureGamestate = CreateTexture(Renderer::NativeResolutionX,
                                            Renderer::NativeResolutionY);
    }

    CanvasOutput = std::make_unique<Canvas>(RenderOptions.Width,
                                            RenderOptions.Height,
                                            Canvas::Type::External);
    SdlTextureOutput = CreateTexture(RenderOptions.Width, RenderOptions.Height);

    int max_x = RenderOptions.Width - 160;
    int max_y = RenderOptions.Height;
    float minScale = std::min(max_x / (float)Renderer::NativeResolutionX,
                              max_y / (float)Renderer::NativeResolutionY);
    OverlaySliceRect.x =
            (max_x - (int)(Renderer::NativeResolutionX * minScale)) / 2;
    OverlaySliceRect.y =
            (max_y - (int)(Renderer::NativeResolutionY * minScale)) / 2;
    OverlaySliceRect.w = (int)(Renderer::NativeResolutionX * minScale);
    OverlaySliceRect.h = (int)(Renderer::NativeResolutionY * minScale);
}

void Rendering::Render(Playback &playback) {
    /* Render background (if not already done, as this only needs to be done
     * once */
    if (!BackgroundRendered) {
        Canvas background(RenderOptions.Width,
                          RenderOptions.Height,
                          Canvas::Type::External);

        AbortUnless(!SDL_LockTexture(SdlTextureBackground.get(),
                                     NULL,
                                     (void **)&background.Buffer,
                                     &background.Stride));

        Renderer::DrawClientBackground(*playback.Gamestate,
                                       background,
                                       0,
                                       0,
                                       background.Width,
                                       background.Height);
        SDL_UnlockTexture(SdlTextureBackground.get());
        BackgroundRendered = true;
    }

    /* Render gamestate */
    {
        AbortUnless(!SDL_LockTexture(SdlTextureGamestate.get(),
                                     NULL,
                                     (void **)&CanvasGamestate->Buffer,
                                     &CanvasGamestate->Stride));

        CanvasGamestate->Wipe();

        Renderer::DrawGamestate(RenderOptions,
                                *playback.Gamestate,
                                *CanvasGamestate);

        SDL_UnlockTexture(SdlTextureGamestate.get());
    }

    /* FIXME: C++ migration. */
    playback.Gamestate->Messages.Prune(playback.Gamestate->CurrentTick);

    /* Render the rest */
    {
        AbortUnless(!SDL_LockTexture(SdlTextureOutput.get(),
                                     NULL,
                                     (void **)&CanvasOutput->Buffer,
                                     &CanvasOutput->Stride));

        CanvasOutput->Wipe();

        auto overlay =
                CanvasOutput->Slice(OverlaySliceRect.x,
                                    OverlaySliceRect.y,
                                    OverlaySliceRect.x + OverlaySliceRect.w,
                                    OverlaySliceRect.y + OverlaySliceRect.h);
        Renderer::DrawOverlay(RenderOptions, *playback.Gamestate, overlay);

        int offsetX = CanvasOutput->Width - 160 + 12;
        int offsetY = 4;

        Renderer::DrawStatusBars(*playback.Gamestate,
                                 *CanvasOutput,
                                 offsetX,
                                 offsetY);

        Renderer::DrawInventoryArea(*playback.Gamestate,
                                    *CanvasOutput,
                                    offsetX,
                                    offsetY);

        if (playback.Gamestate->Version.Features.IconBar) {
            Renderer::DrawIconBar(*playback.Gamestate,
                                  *CanvasOutput,
                                  offsetX,
                                  offsetY);
        }

        int max_container_y = CanvasOutput->Height - 4 - 32;

        for (auto &[_, container] : playback.Gamestate->Containers) {
            Renderer::DrawContainer(*playback.Gamestate,
                                    *CanvasOutput,
                                    container,
                                    false,
                                    CanvasOutput->Width,
                                    max_container_y,
                                    offsetX,
                                    offsetY);
        }

        /* Render playback info */
        char text[64];
        int textLength = snprintf(text,
                                  sizeof(text) / sizeof(text[0]),
                                  "FPS: %.2f",
                                  StatsFPS);
        TextRenderer::Render(playback.Gamestate->Version.Fonts.Game,
                             TextAlignment::Left,
                             TextTransform::None,
                             Pixel(0xFF, 0xFF, 0xFF),
                             12,
                             14,
                             64,
                             std::string(text, textLength),
                             *CanvasOutput);

        textLength = formatTime(text,
                                sizeof(text) / sizeof(text[0]),
                                playback.GetPlaybackTick().count());
        TextRenderer::Render(playback.Gamestate->Version.Fonts.Game,
                             TextAlignment::Left,
                             TextTransform::None,
                             Pixel(0xFF, 0xFF, 0xFF),
                             12,
                             28,
                             64,
                             std::string(text, textLength),
                             *CanvasOutput);

        textLength = formatTime(text,
                                sizeof(text) / sizeof(text[0]),
                                playback.Recording->Runtime.count());
        TextRenderer::Render(playback.Gamestate->Version.Fonts.Game,
                             TextAlignment::Left,
                             TextTransform::None,
                             Pixel(0xFF, 0xFF, 0xFF),
                             12,
                             42,
                             64,
                             std::string(text, textLength),
                             *CanvasOutput);

        SDL_UnlockTexture(SdlTextureOutput.get());
    }

    /* Render textures to screen */
    AbortUnless(!SDL_SetRenderDrawColor(SdlRenderer.get(), 0, 0, 0, 255));
    AbortUnless(!SDL_RenderClear(SdlRenderer.get()));

    AbortUnless(!SDL_RenderCopy(SdlRenderer.get(),
                                SdlTextureBackground.get(),
                                NULL,
                                NULL));
    AbortUnless(!SDL_RenderCopy(SdlRenderer.get(),
                                SdlTextureGamestate.get(),
                                NULL,
                                &OverlaySliceRect));
    AbortUnless(!SDL_RenderCopy(SdlRenderer.get(),
                                SdlTextureOutput.get(),
                                NULL,
                                NULL));

    SDL_RenderPresent(SdlRenderer.get());

    /* FPS counter */
    uint32_t currentTick = SDL_GetTicks();
    StatsFramesSinceLastUpdate += 1;
    if (currentTick >= StatsLastUpdate + 1000) {
        StatsFPS += StatsFramesSinceLastUpdate;
        StatsFPS /= 2;
        StatsFramesSinceLastUpdate = 0;
        StatsLastUpdate = currentTick;
    }
}

}; // namespace trc
