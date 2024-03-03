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

#include "playback.h"
#include "rendering.h"

#ifdef _WIN32
#    define DIR_SEP "\\"
#else
#    define DIR_SEP "/"
#endif

static struct playback playback;
static bool playback_loaded = false;
static struct rendering rendering;

void handle_input() {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                rendering_HandleResize(&rendering);
            }
            break;
        case SDL_KEYDOWN:
            switch (event.key.keysym.sym) {
            case SDLK_UP:
                if (playback.Scale < 1) {
                    playback_SetSpeed(&playback, playback.Scale * 2.0f);
                } else if (playback.Scale < 64) {
                    playback_SetSpeed(&playback, playback.Scale + 0.5f);
                }
                break;
            case SDLK_DOWN:
                if (playback.Scale > 1) {
                    playback_SetSpeed(&playback, playback.Scale - 0.5f);
                } else if (playback.Scale >= 0.005) {
                    playback_SetSpeed(&playback, playback.Scale / 2.0f);
                }
                break;
            case SDLK_BACKSPACE:
                playback_SetSpeed(&playback, 1.0f);
                break;
            case SDLK_LEFT:
                playback_Skip(&playback, -30000);
                break;
            case SDLK_RIGHT:
                playback_Skip(&playback, 30000);
                break;
            }
            break;
        case SDL_MOUSEBUTTONUP:
            playback_TogglePlayback(&playback);
            break;
        case SDL_QUIT:
            exit(0);
        default:
            break;
        }
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
    if (playback_loaded) {
        handle_input();
        playback_ProcessPackets(&playback);
        rendering_Render(&rendering, &playback);
    } else {
        SDL_RenderPresent(rendering.SdlRenderer);
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
    if (playback_loaded) {
        playback_Free(&playback);
    }

    struct trc_data_reader recording;
    recording.Position = 0;
    recording.Data = (const uint8_t *)recording_data;
    recording.Length = recording_length;

    struct trc_data_reader pic;
    pic.Position = 0;
    pic.Data = (const uint8_t *)pic_data;
    pic.Length = pic_length;

    struct trc_data_reader spr;
    spr.Position = 0;
    spr.Data = (const uint8_t *)spr_data;
    spr.Length = spr_length;

    struct trc_data_reader dat;
    dat.Position = 0;
    dat.Data = (const uint8_t *)dat_data;
    dat.Length = dat_length;

    playback_Init(&playback, "", &recording, &pic, &spr, &dat);

    free((void *)pic_data);
    free((void *)spr_data);
    free((void *)dat_data);

    playback_loaded = true;
}

struct player_data {
    struct memory_file PictureFile;
    struct memory_file SpriteFile;
    struct memory_file TypeFile;

    struct trc_data_reader Pictures;
    struct trc_data_reader Sprites;
    struct trc_data_reader Types;
};

static bool player_OpenData(const char *dataFolder, struct player_data *data) {
    char picturePath[FILENAME_MAX];
    char spritePath[FILENAME_MAX];
    char typePath[FILENAME_MAX];

    if (snprintf(picturePath,
                 FILENAME_MAX,
                 "%s%s",
                 dataFolder,
                 DIR_SEP "Tibia.pic") < 0) {
        (void)fprintf(stderr, "Could not merge data path with Tibia.pic");
        return false;
    }

    if (snprintf(spritePath,
                 FILENAME_MAX,
                 "%s%s",
                 dataFolder,
                 DIR_SEP "Tibia.spr") < 0) {
        (void)fprintf(stderr, "Could not merge data path with Tibia.spr");
        return false;
    }

    if (snprintf(typePath,
                 FILENAME_MAX,
                 "%s%s",
                 dataFolder,
                 DIR_SEP "Tibia.dat") < 0) {
        (void)fprintf(stderr, "Could not merge data path with Tibia.dat");
        return false;
    }

    if (!memoryfile_Open(picturePath, &data->PictureFile)) {
        (void)fprintf(stderr, "Failed to open Tibia.pic");
        return false;
    } else {
        if (!memoryfile_Open(spritePath, &data->SpriteFile)) {
            (void)fprintf(stderr, "Failed to open Tibia.spr");
            return false;
        } else {
            if (!memoryfile_Open(typePath, &data->TypeFile)) {
                (void)fprintf(stderr, "Failed to open Tibia.dat");
                return false;
            } else {
                data->Pictures.Data = data->PictureFile.View;
                data->Pictures.Length = data->PictureFile.Size;
                data->Pictures.Position = 0;

                data->Sprites.Data = data->SpriteFile.View;
                data->Sprites.Length = data->SpriteFile.Size;
                data->Sprites.Position = 0;

                data->Types.Data = data->TypeFile.View;
                data->Types.Length = data->TypeFile.Size;
                data->Types.Position = 0;

                return true;
            }

            memoryfile_Close(&data->SpriteFile);
        }

        memoryfile_Close(&data->PictureFile);
    }

    return false;
}

static void player_CloseData(struct player_data *data) {
    memoryfile_Close(&data->PictureFile);
    memoryfile_Close(&data->SpriteFile);
    memoryfile_Close(&data->TypeFile);
}

int main(int argc, char *argv[]) {
#ifndef EMSCRIPTEN
    if (argc != 3) {
        fprintf(stderr, "usage: %s DATA_FOLDER RECORDING \n", argv[0]);
        return 1;
    }

    trc_ChangeErrorReporting(TRC_ERROR_REPORT_MODE_TEXT);

    if (!rendering_Init(&rendering, 800, 600)) {
        return 1;
    }

    struct memory_file file_recording;
    struct player_data data;

    if (!player_OpenData(argv[1], &data) ||
        !memoryfile_Open(argv[2], &file_recording)) {
        return 1;
    }

    if (!playback_Init(&playback,
                       argv[2],
                       &(struct trc_data_reader){.Data = file_recording.View,
                                                 .Length = file_recording.Size,
                                                 .Position = 0},
                       &data.Pictures,
                       &data.Sprites,
                       &data.Types)) {
        return 1;
    }

    player_CloseData(&data);

    playback_loaded = true;
    emscripten_set_main_loop(main_loop, 0, 1);

    playback_Free(&playback);
    rendering_Free(&rendering);

    memoryfile_Close(&file_recording);
#else
    rendering_Init(&rendering, 800, 600);
    emscripten_set_main_loop(main_loop, 0, 1);
#endif
    return 0;
}
