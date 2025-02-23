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

#ifdef EMSCRIPTEN
#    include <emscripten.h>
#endif

#include <SDL2/SDL.h>
#include <iostream>

#include "playback.hpp"
#include "rendering.hpp"

#ifdef _WIN32
#    define DIR_SEP "\\"
#else
#    define DIR_SEP "/"
#endif

using namespace trc;

static std::unique_ptr<Playback> playback;
static Rendering rendering(800, 600);

void handle_input() {
    /* Coalesce skips for better rewind performance on long recordings. */
    int32_t skip_by = 0;
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                rendering.HandleResize();
            }
            break;
        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
            case SDLK_UP:
                if (playback->Scale < 1) {
                    playback->SetSpeed(playback->Scale * 2.0f);
                } else if (playback->Scale < 64) {
                    playback->SetSpeed(playback->Scale + 0.5f);
                }
                break;
            case SDLK_DOWN:
                if (playback->Scale > 1) {
                    playback->SetSpeed(playback->Scale - 0.5f);
                } else if (playback->Scale >= 0.005) {
                    playback->SetSpeed(playback->Scale / 2.0f);
                }
                break;
            case SDLK_BACKSPACE:
                playback->SetSpeed(1.0f);
                break;
            case SDLK_LEFT:
            case SDLK_RIGHT: {
                int32_t direction, magnitude;

                direction = (event.key.keysym.sym == SDLK_LEFT) ? -1 : 1;
                magnitude = (1 + !!(event.key.keysym.mod & KMOD_SHIFT) * 9);

                skip_by += direction * magnitude * 30000;
            }
            }
            break;
        case SDL_MOUSEBUTTONUP:
            playback->Toggle();
            break;
        case SDL_QUIT:
            exit(0);
        default:
            break;
        }
    }

    if (skip_by != 0) {
        playback->Skip(skip_by);
    }
}

#ifndef EMSCRIPTEN
void emscripten_set_main_loop(void (*main_loop)(),
                              int fps,
                              int simulate_infinite_loop) {
    if (fps == 0) {
        fps = 120;
    }

    Uint32 ms_per_iteration = 1000 / fps;
    while (true) {
        Uint32 start = SDL_GetTicks();
        main_loop();
        if (simulate_infinite_loop == 0) {
            break;
        }
        Uint32 elapsed = SDL_GetTicks() - start;
        if (elapsed < ms_per_iteration) {
            SDL_Delay(ms_per_iteration - elapsed);
        }
    }
}
#endif

void main_loop() {
    if (playback) {
        handle_input();
        playback->ProcessPackets();
        rendering.Render(*playback);
    } else {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                exit(0);
            }
        }

        SDL_RenderPresent(rendering.SdlRenderer.get());
    }
}

void load_files(uintptr_t recording_data,
                int recording_length,
                uintptr_t pic_data,
                int pic_length,
                uintptr_t spr_data,
                int spr_length,
                uintptr_t dat_data,
                int dat_length) {
    DataReader recording(recording_length, (const uint8_t *)recording_data);
    DataReader pic(pic_length, (const uint8_t *)pic_data);
    DataReader spr(spr_length, (const uint8_t *)spr_data);
    DataReader dat(dat_length, (const uint8_t *)dat_data);

    playback = std::make_unique<Playback>(recording, "", pic, spr, dat);
}

int main(int argc, char *argv[]) {
#ifndef EMSCRIPTEN
    int major = 0, minor = 0, preview = 0;

    if (argc < 3 || argc > 4) {
        fprintf(stderr, "usage: %s DATA_FOLDER RECORDING [VERSION]\n", argv[0]);
        return 1;
    }

    if (argc == 4) {
        if (sscanf(argv[3], "%u.%u.%u", &major, &minor, &preview) < 2) {
            fprintf(stderr,
                    "version must be in the format 'X.Y', e.g. '8.55'\n");
            return 1;
        }
    }

    try {
        const std::string dataFolder = argv[1], recordingName = argv[2];
        const MemoryFile pictures(dataFolder + DIR_SEP "Tibia.pic");
        const MemoryFile sprites(dataFolder + DIR_SEP "Tibia.spr");
        const MemoryFile types(dataFolder + DIR_SEP "Tibia.dat");
        const MemoryFile recording(recordingName);

        playback = std::make_unique<Playback>(recording.Reader(),
                                              recordingName,
                                              pictures.Reader(),
                                              sprites.Reader(),
                                              types.Reader(),
                                              major,
                                              minor,
                                              preview);
    } catch (const trc::ErrorBase &error) {
        std::cerr << "Unrecoverable error (" << error.Description() << ") at:\n"
                  << error.Stacktrace << std::endl;
        return 1;
    }

    emscripten_set_main_loop(main_loop, 0, 1);
#else
    emscripten_set_main_loop(main_loop, 0, 1);
#endif

    return 0;
}
