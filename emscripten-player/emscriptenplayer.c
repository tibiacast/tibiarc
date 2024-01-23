#ifdef EMSCRIPTEN
#    include <emscripten.h>
#endif
#include <SDL2/SDL.h>

#include "canvas.h"
#include "datareader.h"
#include "gamestate.h"
#include "recordings.h"
#include "renderer.h"
#include "versions.h"
#include "memoryfile.h"

#if 1
#    define WINDOW_STARTUP_WIDTH 1280
#    define WINDOW_STARTUP_HEIGHT 720
#else
#    define WINDOW_STARTUP_WIDTH (NATIVE_RESOLUTION_X + 160)
#    define WINDOW_STARTUP_HEIGHT NATIVE_RESOLUTION_Y
#endif

static struct playback {
    struct memory_file file;
    struct trc_data_reader reader;
    struct trc_version *version;
    struct trc_recording *recording;
    struct trc_game_state *gamestate;

    Uint32 playback_start;
    bool playback_paused;
    Uint32 playback_pause_tick;
} playback;

static struct rendering {
    SDL_Window *sdl_window;
    SDL_Renderer *sdl_renderer;

    struct trc_render_options render_options;

    // This texture is the same size as the window
    // It is static, so we render it only once (during startup or window resize)
    SDL_Texture *sdl_texture_background;

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

Uint32 get_playback_tick() {
    if (playback.playback_paused) {
        return playback.playback_pause_tick;
    }
    return SDL_GetTicks() - playback.playback_start;
}

void handle_input() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_MOUSEBUTTONUP:
            if (!playback.playback_paused) {
                playback.playback_pause_tick = get_playback_tick();
                puts("Playback paused");
            } else {
                playback.playback_start =
                        (SDL_GetTicks() - playback.playback_pause_tick);
                puts("Playback resumed");
            }
            playback.playback_paused = !playback.playback_paused;
            break;
        case SDL_QUIT:
            exit(0);
        default:
            break;
        }
    }
}

void handle_logic() {
    // Process packets until we have caught up
    Uint32 playback_tick = get_playback_tick();
    while (playback.recording->NextPacketTimestamp <= playback_tick) {
        if (!recording_ProcessNextPacket(playback.recording,
                                         playback.gamestate)) {
            fprintf(stderr, "Could not process packet: %s\n", get_last_error());
            abort();
        }
    }

    // Advance the gamestate
    playback.gamestate->CurrentTick = playback_tick;
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

void handle_render() {
    // Render background (if not already done, as this only needs to be done
    // once)
    if (rendering.sdl_texture_background == NULL) {
        // Render the background texture, as that is static and only needs to be
        // done once
        struct trc_canvas canvas_background = {
                .Width = rendering.render_options.Width,
                .Height = rendering.render_options.Height};
        rendering.sdl_texture_background =
                create_texture(canvas_background.Width,
                               canvas_background.Height);
        SDL_LockTexture(rendering.sdl_texture_background,
                        NULL,
                        (void **)&canvas_background.Buffer,
                        &canvas_background.Stride);
        renderer_RenderClientBackground(playback.gamestate,
                                        &canvas_background,
                                        0,
                                        0,
                                        canvas_background.Width,
                                        canvas_background.Height);
        SDL_UnlockTexture(rendering.sdl_texture_background);
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
                                    playback.gamestate,
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
                                  playback.gamestate,
                                  &overlay_slice)) {
            fprintf(stderr, "Could not render overlay\n");
            abort();
        }

        int offsetX = rendering.canvas_output.Width - 160 + 12;
        int offsetY = 4;

        if (!renderer_DrawStatusBars(&rendering.render_options,
                                     playback.gamestate,
                                     &rendering.canvas_output,
                                     &offsetX,
                                     &offsetY)) {
            fprintf(stderr, "Could not render status bars\n");
            abort();
        }

        if (!renderer_DrawInventoryArea(&rendering.render_options,
                                        playback.gamestate,
                                        &rendering.canvas_output,
                                        &offsetX,
                                        &offsetY)) {
            fprintf(stderr, "Could not render inventory\n");
            abort();
        }

        if (playback.gamestate->Version->Features.IconBar) {
            if (!renderer_DrawIconBar(&rendering.render_options,
                                      playback.gamestate,
                                      &rendering.canvas_output,
                                      &offsetX,
                                      &offsetY)) {
                fprintf(stderr, "Could not render icon bar\n");
                abort();
            }
        }

        int max_container_y = rendering.canvas_output.Height - 4 - 32;
        for (struct trc_container *container_iterator =
                     playback.gamestate->ContainerList;
             container_iterator != NULL && offsetY < max_container_y;
             container_iterator =
                     (struct trc_container *)container_iterator->hh.next) {
            if (!renderer_DrawContainer(&rendering.render_options,
                                        playback.gamestate,
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
    handle_logic();
    handle_render();
    handle_stats();
}

bool open_memory_file(const char *filename,
                      struct memory_file *file,
                      struct trc_data_reader *reader) {
    if (!memoryfile_Open(filename, file)) {
        fprintf(stderr, "Could not open file: %s\n", filename);
        return false;
    }
    reader->Position = 0;
    reader->Data = file->View;
    reader->Length = file->Size;
    return true;
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
                                            SDL_WINDOW_SHOWN);
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

bool init_playback(const char *recording_filename,
                   const char *pic_filename,
                   const char *spr_filename,
                   const char *dat_filename) {
    // Load data files
    struct memory_file file_pic;
    struct trc_data_reader reader_pic;
    if (!open_memory_file(pic_filename, &file_pic, &reader_pic)) {
        return false;
    }

    struct memory_file file_spr;
    struct trc_data_reader reader_spr;
    if (!open_memory_file(spr_filename, &file_spr, &reader_spr)) {
        return false;
    }

    struct memory_file file_dat;
    struct trc_data_reader reader_dat;
    if (!open_memory_file(dat_filename, &file_dat, &reader_dat)) {
        return false;
    }

    if (!version_Load(7,
                      41,
                      0,
                      &reader_pic,
                      &reader_spr,
                      &reader_dat,
                      &playback.version)) {
        fprintf(stderr, "Could not load files\n");
        return false;
    }

    memoryfile_Close(&file_pic);
    memoryfile_Close(&file_spr);
    memoryfile_Close(&file_dat);

    // Load recording
    if (!open_memory_file(recording_filename,
                          &playback.file,
                          &playback.reader)) {
        return false;
    }

    playback.recording = recording_Create(RECORDING_FORMAT_TRP);
    if (!recording_Open(playback.recording,
                        &playback.reader,
                        playback.version)) {
        fprintf(stderr, "Could not load recording\n");
        return false;
    }

    // Create gamestate
    playback.gamestate = gamestate_Create(playback.version);

    // Initialize the playback by processing the first packet (assumes only
    // first packet has time = 0)
    playback.playback_start = SDL_GetTicks();
    playback.playback_paused = false;
    playback.gamestate->CurrentTick = get_playback_tick();
    if (!recording_ProcessNextPacket(playback.recording, playback.gamestate)) {
        fprintf(stderr, "Could not process packet\n");
        return false;
    }

    return true;
}

void free_playback() {
    recording_Free(playback.recording);
    version_Free(playback.version);
    memoryfile_Close(&playback.file);
}

void init_rendering() {
    // Setup rendering
    rendering.render_options.Width = WINDOW_STARTUP_WIDTH;
    rendering.render_options.Height = WINDOW_STARTUP_HEIGHT;

    // Create canvases, which are (going to be) backed by SDL_Textures
    rendering.canvas_gamestate.Width = NATIVE_RESOLUTION_X;
    rendering.canvas_gamestate.Height = NATIVE_RESOLUTION_Y;
    rendering.sdl_texture_gamestate =
            create_texture(rendering.canvas_gamestate.Width,
                           rendering.canvas_gamestate.Height);

    rendering.canvas_output.Width = rendering.render_options.Width;
    rendering.canvas_output.Height = rendering.render_options.Height;
    rendering.sdl_texture_output =
            create_texture(rendering.canvas_output.Width,
                           rendering.canvas_output.Height);

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

    if (!init_playback("files/test.trp",
                       "files/Tibia.pic",
                       "files/Tibia.spr",
                       "files/Tibia.dat")) {
        return 1;
    }

    init_rendering();

    // Init stats
    stats_frames = 0;
    stats_frames_avg = 0.0f;
    stats_last_print = playback.playback_start;

    emscripten_set_main_loop(main_loop, 0, 1);

    // Deallocate
    free_rendering();
    free_playback();
    free_sdl();

    return 0;
}
