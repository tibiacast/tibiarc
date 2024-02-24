#ifdef EMSCRIPTEN
#    include <emscripten.h>
#endif
#include <SDL2/SDL.h>

#include "playback.h"
#include "rendering.h"

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

void load_files(uintptr_t recording_data, int recording_length, uintptr_t pic_data, int pic_length,
                uintptr_t spr_data, int spr_length, uintptr_t dat_data, int dat_length) {

    if (playback_loaded) {
        // Unload the currently playing recording
        // TODO...
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

    playback_Init(&playback, &recording, &pic, &spr, &dat);

    free((void *) pic_data);
    free((void *) spr_data);
    free((void *) dat_data);

    playback_loaded = true;
}

int main(int argc, char *argv[]) {
#if 0
    if (argc != 5) {
        fprintf(stderr, "usage: %s RECORDING PIC SPR DAT\n", argv[0]);
        return 1;
    }

    trc_ChangeErrorReporting(TRC_ERROR_REPORT_MODE_TEXT);

    if (!rendering_Init(&rendering, 800, 600)) {
        return 1;
    }

// Load data files
struct memory_file file_pic;
struct trc_data_reader reader_pic;
if (!openMemoryFile(picFilename, &file_pic, &reader_pic)) {
    return false;
}

struct memory_file file_spr;
struct trc_data_reader reader_spr;
if (!openMemoryFile(sprFilename, &file_spr, &reader_spr)) {
    return false;
}

struct memory_file file_dat;
struct trc_data_reader reader_dat;
if (!openMemoryFile(datFilename, &file_dat, &reader_dat)) {
    return false;
}

// Load recording
if (!openMemoryFile(recordingFilename,
                    &playback->File,
                    &playback->Reader)) {
    return false;
}

    if (!playback_Init(&playback, argv[1], argv[2], argv[3], argv[4])) {
        return 1;
    }


    memoryfile_Close(&file_pic);
    memoryfile_Close(&file_spr);
    memoryfile_Close(&file_dat);

    emscripten_set_main_loop(main_loop, 0, 1);

    // Deallocate
    playback_Free(&playback);
    rendering_Free(&rendering);
#else
    rendering_Init(&rendering, 800, 600);
    emscripten_set_main_loop(main_loop, 0, 1);
#endif
    return 0;
}
