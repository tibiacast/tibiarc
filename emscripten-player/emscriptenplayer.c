#ifdef EMSCRIPTEN
#    include <emscripten.h>
#endif
#include <SDL2/SDL.h>

#include "playback.h"

#include "canvas.h"
#include "renderer.h"

#if 1
#    define WINDOW_STARTUP_WIDTH 1280
#    define WINDOW_STARTUP_HEIGHT 720
#else
#    define WINDOW_STARTUP_WIDTH (NATIVE_RESOLUTION_X + 160)
#    define WINDOW_STARTUP_HEIGHT NATIVE_RESOLUTION_Y
#endif

static struct playback playback;

static struct rendering {
    SDL_Window *sdl_window;
    SDL_Renderer *sdl_renderer;

    struct trc_render_options render_options;

    // This texture is the same size as the window
    // It is static, so we render it only once (during startup or window resize)
    SDL_Texture *sdl_texture_background;
    bool background_rendered;

    // This canvas and texture are always NATIVE_RESOLUTION
    struct trc_canvas canvas_gamestate;
    SDL_Texture *sdl_texture_gamestate;

    // This canvas and texture are always the same size as the window
    struct trc_canvas canvas_output;
    SDL_Texture *sdl_texture_output;

    // This rectangle represents where the gamestate texture
    // should be rendered on the output texture, including scaling
    SDL_Rect overlay_slice_rect;
} rendering;

// Stats (fps) stuff
static int stats_frames;
static double stats_frames_avg;
static uint32_t stats_last_print;

const char *get_last_error() {
    static char buffer[1024];
    trc_GetLastError(1024, buffer);
    return buffer;
}

SDL_Texture *create_texture(int width, int height) {
    SDL_Texture *texture = SDL_CreateTexture(rendering.sdl_renderer,
                                             SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             width,
                                             height);
    if (!texture) {
        fprintf(stderr, "Could not create texture: %s\n", SDL_GetError());
        abort();
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    return texture;
}

void handle_resize() {
    int width;
    int height;
    SDL_GetRendererOutputSize(rendering.sdl_renderer, &width, &height);
    printf("handle resize: %dx%d\n", width, height);
    if (rendering.render_options.Width == width && rendering.render_options.Height == height) {
        fprintf(stderr, "bogus resize event?\n");
        return;
    }

    // Setup rendering
    rendering.render_options.Width = width;
    rendering.render_options.Height = height;

    // Create canvases, which are (going to be) backed by SDL_Textures

    if (rendering.sdl_texture_background != NULL) {
        SDL_DestroyTexture(rendering.sdl_texture_background);
    }
    rendering.sdl_texture_background = create_texture(width, height);
    rendering.background_rendered = false;

    // gamestate canvas and texture is always the same size, so only need
    // to create them if they aren't already created
    if (rendering.sdl_texture_gamestate == NULL) {
        rendering.canvas_gamestate.Width = NATIVE_RESOLUTION_X;
        rendering.canvas_gamestate.Height = NATIVE_RESOLUTION_Y;
        rendering.sdl_texture_gamestate = create_texture(NATIVE_RESOLUTION_X, NATIVE_RESOLUTION_Y);
    }

    if (rendering.sdl_texture_output != NULL) {
        SDL_DestroyTexture(rendering.sdl_texture_output);
    }
    rendering.canvas_output.Width = rendering.render_options.Width;
    rendering.canvas_output.Height = rendering.render_options.Height;
    rendering.sdl_texture_output =
            create_texture(rendering.render_options.Width,
                           rendering.render_options.Height);

    {
        int max_x = rendering.render_options.Width - 160;
        int max_y = rendering.render_options.Height;
        float minScale = MIN(max_x / (float)NATIVE_RESOLUTION_X,
                             max_y / (float)NATIVE_RESOLUTION_Y);
        rendering.overlay_slice_rect.x =
                (max_x - (int)(NATIVE_RESOLUTION_X * minScale)) / 2;
        rendering.overlay_slice_rect.y =
                (max_y - (int)(NATIVE_RESOLUTION_Y * minScale)) / 2;
        rendering.overlay_slice_rect.w = (int)(NATIVE_RESOLUTION_X * minScale);
        rendering.overlay_slice_rect.h = (int)(NATIVE_RESOLUTION_Y * minScale);
    }
}

void handle_input() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_WINDOWEVENT:
            if (event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                handle_resize();
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

void handle_render() {
    // Render background (if not already done, as this only needs to be done
    // once)
    if (!rendering.background_rendered) {
        struct trc_canvas canvas_background = {
                .Width = rendering.render_options.Width,
                .Height = rendering.render_options.Height};
        SDL_LockTexture(rendering.sdl_texture_background,
                        NULL,
                        (void **)&canvas_background.Buffer,
                        &canvas_background.Stride);
        renderer_RenderClientBackground(playback.Gamestate,
                                        &canvas_background,
                                        0,
                                        0,
                                        canvas_background.Width,
                                        canvas_background.Height);
        SDL_UnlockTexture(rendering.sdl_texture_background);
        rendering.background_rendered = true;
    }

    // Render gamestate
    {
        SDL_LockTexture(rendering.sdl_texture_gamestate,
                        NULL,
                        (void **)&rendering.canvas_gamestate.Buffer,
                        &rendering.canvas_gamestate.Stride);
        canvas_DrawRectangle(&rendering.canvas_gamestate,
                             &(struct trc_pixel){},
                             0,
                             0,
                             rendering.canvas_gamestate.Width,
                             rendering.canvas_gamestate.Height);
        if (!renderer_DrawGamestate(&rendering.render_options,
                                    playback.Gamestate,
                                    &rendering.canvas_gamestate)) {
            fprintf(stderr, "Could not render gamestate\n");
            abort();
        }
        SDL_UnlockTexture(rendering.sdl_texture_gamestate);
    }

    // Render the rest
    {
        SDL_LockTexture(rendering.sdl_texture_output,
                        NULL,
                        (void **)&rendering.canvas_output.Buffer,
                        &rendering.canvas_output.Stride);

        canvas_DrawRectangle(&rendering.canvas_output,
                             &(struct trc_pixel){},
                             0,
                             0,
                             rendering.canvas_output.Width,
                             rendering.canvas_output.Height);

        struct trc_canvas overlay_slice;
        overlay_slice.Width = rendering.overlay_slice_rect.w;
        overlay_slice.Height = rendering.overlay_slice_rect.h;
        overlay_slice.Stride = rendering.canvas_output.Stride;
        overlay_slice.Buffer =
                (uint8_t *)canvas_GetPixel(&rendering.canvas_output,
                                           rendering.overlay_slice_rect.x,
                                           rendering.overlay_slice_rect.y);
        if (!renderer_DrawOverlay(&rendering.render_options,
                                  playback.Gamestate,
                                  &overlay_slice)) {
            fprintf(stderr, "Could not render overlay\n");
            abort();
        }

        int offsetX = rendering.canvas_output.Width - 160 + 12;
        int offsetY = 4;

        if (!renderer_DrawStatusBars(&rendering.render_options,
                                     playback.Gamestate,
                                     &rendering.canvas_output,
                                     &offsetX,
                                     &offsetY)) {
            fprintf(stderr, "Could not render status bars\n");
            abort();
        }

        if (!renderer_DrawInventoryArea(&rendering.render_options,
                                        playback.Gamestate,
                                        &rendering.canvas_output,
                                        &offsetX,
                                        &offsetY)) {
            fprintf(stderr, "Could not render inventory\n");
            abort();
        }

        if (playback.Gamestate->Version->Features.IconBar) {
            if (!renderer_DrawIconBar(&rendering.render_options,
                                      playback.Gamestate,
                                      &rendering.canvas_output,
                                      &offsetX,
                                      &offsetY)) {
                fprintf(stderr, "Could not render icon bar\n");
                abort();
            }
        }

        int max_container_y = rendering.canvas_output.Height - 4 - 32;
        for (struct trc_container *container_iterator =
                     playback.Gamestate->ContainerList;
             container_iterator != NULL && offsetY < max_container_y;
             container_iterator =
                     (struct trc_container *)container_iterator->hh.next) {
            if (!renderer_DrawContainer(&rendering.render_options,
                                        playback.Gamestate,
                                        &rendering.canvas_output,
                                        container_iterator,
                                        false,
                                        rendering.canvas_output.Width,
                                        max_container_y,
                                        &offsetX,
                                        &offsetY)) {
                fprintf(stderr, "Could not render container\n");
                abort();
            }
        }
        SDL_UnlockTexture(rendering.sdl_texture_output);
    }

    // Render textures to screen
    SDL_SetRenderDrawColor(rendering.sdl_renderer, 0, 0, 0, 255);
    SDL_RenderClear(rendering.sdl_renderer);
    if (SDL_RenderCopy(rendering.sdl_renderer,
                       rendering.sdl_texture_background,
                       NULL,
                       NULL) != 0) {
        fprintf(stderr,
                "Could not render texture to screen: %s\n",
                SDL_GetError());
        abort();
    }
    if (SDL_RenderCopy(rendering.sdl_renderer,
                       rendering.sdl_texture_gamestate,
                       NULL,
                       &rendering.overlay_slice_rect) != 0) {
        fprintf(stderr,
                "Could not render texture to screen: %s\n",
                SDL_GetError());
        abort();
    }
    if (SDL_RenderCopy(rendering.sdl_renderer,
                       rendering.sdl_texture_output,
                       NULL,
                       NULL) != 0) {
        fprintf(stderr,
                "Could not render texture to screen: %s\n",
                SDL_GetError());
        abort();
    }
    SDL_RenderPresent(rendering.sdl_renderer);
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
    handle_render();
    handle_stats();
}


bool init_sdl() {
    int ret = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    if (ret != 0) {
        fprintf(stderr, "Could not initialize SDL: %s\n", SDL_GetError());
        return false;
    }

    rendering.sdl_window = SDL_CreateWindow("emscriptenplayer",
                                            SDL_WINDOWPOS_UNDEFINED,
                                            SDL_WINDOWPOS_UNDEFINED,
                                            WINDOW_STARTUP_WIDTH,
                                            WINDOW_STARTUP_HEIGHT,
                                            SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!rendering.sdl_window) {
        fprintf(stderr, "Could not create window: %s\n", SDL_GetError());
        return false;
    }

    rendering.sdl_renderer =
            SDL_CreateRenderer(rendering.sdl_window,
                               -1,
                               SDL_RENDERER_ACCELERATED |
                                       /*SDL_RENDERER_PRESENTVSYNC |*/
                                       SDL_RENDERER_TARGETTEXTURE);
    if (!rendering.sdl_renderer) {
        fprintf(stderr, "Could not create renderer: %s\n", SDL_GetError());
        return false;
    }
    SDL_SetRenderDrawBlendMode(rendering.sdl_renderer, SDL_BLENDMODE_BLEND);

    return true;
}

void free_sdl() {
    SDL_DestroyRenderer(rendering.sdl_renderer);
    SDL_Quit();
}

void free_rendering() {
    SDL_DestroyTexture(rendering.sdl_texture_background);
    SDL_DestroyTexture(rendering.sdl_texture_output);
    SDL_DestroyTexture(rendering.sdl_texture_gamestate);
}

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    trc_ChangeErrorReporting(TRC_ERROR_REPORT_MODE_TEXT);

    if (!init_sdl()) {
        return 1;
    }

    if (!playback_Init(&playback,
                       "files/test.trp",
                       "files/Tibia.pic",
                       "files/Tibia.spr",
                       "files/Tibia.dat")) {
        return 1;
    }

    // Init rendering by faking a resize event
    handle_resize();

    // Init stats
    stats_frames = 0;
    stats_frames_avg = 0.0f;
    stats_last_print = playback.PlaybackStart;

    emscripten_set_main_loop(main_loop, 0, 1);

    // Deallocate
    free_rendering();
    playback_Free(&playback);
    free_sdl();

    return 0;
}
