#ifdef EMSCRIPTEN
#    include <emscripten.h>
#endif
#include <SDL2/SDL.h>

#include "playback.h"
#include "rendering.h"

static struct playback playback;
static struct rendering rendering;

// Stats (fps) stuff
static int stats_frames;
static double stats_frames_avg;
static uint32_t stats_last_print;

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

void handle_stats() {
    Uint32 current_tick = SDL_GetTicks();
    stats_frames += 1;
    if (current_tick - stats_last_print >= 5000) {
        double fps =
                stats_frames / ((current_tick - stats_last_print) / 1000.0);
        stats_frames_avg += fps;
        stats_frames_avg /= 2.0;
        printf("FPS: %.2f Average FPS: %.2f\n", fps, stats_frames_avg);

        stats_frames = 0;
        stats_last_print = current_tick;
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
    handle_input();
    playback_ProcessPackets(&playback);
    rendering_Render(&rendering, &playback);
    handle_stats();
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    trc_ChangeErrorReporting(TRC_ERROR_REPORT_MODE_TEXT);

    if (!rendering_Init(&rendering, 800, 600)) {
        return 1;
    }

    if (!playback_Init(&playback,
                       "files/test.trp",
                       "files/Tibia.pic",
                       "files/Tibia.spr",
                       "files/Tibia.dat")) {
        return 1;
    }

    // Init stats
    stats_frames = 0;
    stats_frames_avg = 0.0f;
    stats_last_print = playback.PlaybackStart;

    emscripten_set_main_loop(main_loop, 0, 1);

    // Deallocate
    playback_Free(&playback);
    rendering_Free(&rendering);

    return 0;
}
