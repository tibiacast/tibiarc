#ifdef EMSCRIPTEN
#    include <emscripten.h>
#endif
#include <SDL2/SDL.h>

#include "playback.h"
#include "rendering.h"

static struct playback playback;
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
    handle_input();
    playback_ProcessPackets(&playback);
    rendering_Render(&rendering, &playback);
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "usage: %s RECORDING PIC SPR DAT\n", argv[0]);
        return 1;
    }

    trc_ChangeErrorReporting(TRC_ERROR_REPORT_MODE_TEXT);

    if (!rendering_Init(&rendering, 800, 600)) {
        return 1;
    }

    if (!playback_Init(&playback, argv[1], argv[2], argv[3], argv[4])) {
        return 1;
    }

    emscripten_set_main_loop(main_loop, 0, 1);

    // Deallocate
    playback_Free(&playback);
    rendering_Free(&rendering);

    return 0;
}
