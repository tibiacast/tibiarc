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

#ifndef DISABLE_SDL2

#    include "encoding.h"

#    define SDL_MAIN_HANDLED
#    include <SDL.h>

#    include "utils.h"

struct trc_encoder_sdl2 {
    struct trc_encoder Base;

    SDL_Window *Window;
    SDL_Renderer *Renderer;
    SDL_Texture *Texture;

    Uint64 Frequency;
    Uint64 LastPTS;
    Uint64 TargetDiff;

    int Initialized;
};

static bool sdl2_Open(struct trc_encoder_sdl2 *encoder,
                      const char *format,
                      const char *encoding,
                      const char *flags,
                      int xResolution,
                      int yResolution,
                      int frameRate,
                      const char *path) {
    SDL_SetMainReady();

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        return trc_ReportError("Failed to init SDL");
    }

    encoder->Initialized = 1;

    encoder->Frequency = SDL_GetPerformanceFrequency();
    encoder->LastPTS = SDL_GetPerformanceCounter();
    encoder->TargetDiff = encoder->Frequency / frameRate;

    encoder->Window = SDL_CreateWindow("tibiarc - player mode",
                                       SDL_WINDOWPOS_UNDEFINED,
                                       SDL_WINDOWPOS_UNDEFINED,
                                       xResolution,
                                       yResolution,
                                       SDL_WINDOW_SHOWN);
    if (encoder->Window == NULL) {
        return trc_ReportError("Failed to create window: %s", SDL_GetError());
    }

    encoder->Renderer =
            SDL_CreateRenderer(encoder->Window, -1, SDL_RENDERER_PRESENTVSYNC);
    if (encoder->Renderer == NULL) {
        return trc_ReportError("Failed to create renderer: %s", SDL_GetError());
    }

    encoder->Texture = SDL_CreateTexture(encoder->Renderer,
                                         SDL_PIXELFORMAT_RGBA32,
                                         SDL_TEXTUREACCESS_STREAMING,
                                         xResolution,
                                         yResolution);
    if (encoder->Texture == NULL) {
        return trc_ReportError("Failed to create target texture: %s",
                               SDL_GetError());
    }

    return true;
}

static bool sdl2_WriteFrame(struct trc_encoder_sdl2 *encoder,
                            struct trc_canvas *frame) {
    SDL_Event event;
    void *data;
    int pitch;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            exit(0);
        default:
            break;
        }
    }

    if (SDL_LockTexture(encoder->Texture, NULL, &data, &pitch)) {
        return trc_ReportError("Failed to lock target texture: %s",
                               SDL_GetError());
    }

    for (int y = 0; y < frame->Height; y++) {
        const struct trc_pixel *line = canvas_GetPixel(frame, 0, y);
        char *dstBuffer = (char *)data;

        memcpy(&dstBuffer[y * pitch],
               line,
               sizeof(struct trc_pixel) * frame->Width);
    }

    SDL_UnlockTexture(encoder->Texture);

    if (SDL_RenderCopy(encoder->Renderer, encoder->Texture, NULL, NULL)) {
        return trc_ReportError("Failed to copy texture to renderer: %s",
                               SDL_GetError());
    }

    SDL_RenderPresent(encoder->Renderer);

    Uint64 timeDelta = SDL_GetPerformanceCounter() - encoder->LastPTS;
    encoder->LastPTS += encoder->TargetDiff;

    if (timeDelta < encoder->TargetDiff) {
        Uint64 delayMs =
                ((encoder->TargetDiff - timeDelta) * 1000) / encoder->Frequency;

        if (delayMs > 0) {
            SDL_Delay(delayMs);
        }
    }

    return true;
}

static bool sdl2_Free(struct trc_encoder_sdl2 *encoder) {
    if (encoder->Texture) {
        SDL_DestroyTexture(encoder->Texture);
    }

    if (encoder->Renderer) {
        SDL_DestroyRenderer(encoder->Renderer);
    }

    if (encoder->Window) {
        SDL_DestroyWindow(encoder->Window);
    }

    if (encoder->Initialized) {
        SDL_Quit();
    }

    checked_deallocate(encoder);
    return true;
}

struct trc_encoder *sdl2_Create() {
    struct trc_encoder_sdl2 *encoder = (struct trc_encoder_sdl2 *)
            checked_allocate(1, sizeof(struct trc_encoder_sdl2));

    encoder->Base.Open = (bool (*)(struct trc_encoder *,
                                   const char *,
                                   const char *,
                                   const char *,
                                   int,
                                   int,
                                   int,
                                   const char *))sdl2_Open;
    encoder->Base.WriteFrame = (bool (*)(struct trc_encoder *,
                                         struct trc_canvas *))sdl2_WriteFrame;
    encoder->Base.Free = (void (*)(struct trc_encoder *))sdl2_Free;

    return (struct trc_encoder *)encoder;
}

#endif /* DISABLE_SDL2 */
