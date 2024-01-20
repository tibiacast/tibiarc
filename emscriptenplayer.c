#ifdef EMSCRIPTEN
#    include <emscripten.h>
#endif
#include <SDL2/SDL.h>

#include "lib/canvas.h"
#include "lib/datareader.h"
#include "lib/gamestate.h"
#include "lib/recordings.h"
#include "lib/renderer.h"
#include "lib/versions.h"
#include "memoryfile.h"

// tibiarc stuff
static struct memory_file file_trp;
static struct trc_data_reader reader_trp;

static struct memory_file file_pic;
static struct trc_data_reader reader_pic;

static struct memory_file file_spr;
static struct trc_data_reader reader_spr;

static struct memory_file file_dat;
static struct trc_data_reader reader_dat;

static struct trc_version *version;
static struct trc_recording *recording;
static struct trc_game_state *gamestate;
static struct trc_canvas canvas_gamestate;
static struct trc_canvas canvas_output;
static struct trc_canvas canvas_overlay_slice;
static struct trc_render_options render_options;

static int viewLeftX;
static int viewTopY;
static int viewRightX;
static int viewBottomY;

// SDL stuff
static SDL_Window *sdl_window;
static SDL_Renderer *sdl_renderer;
static SDL_Texture *sdl_texture_background;
static SDL_Texture *sdl_texture_gamestate;
static SDL_Texture *sdl_texture_output;

// Playback stuff
static Uint32 playback_start;
static bool playback_paused;
static Uint32 playback_pause_tick;

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
    if (playback_paused) {
        return playback_pause_tick;
    }
    return SDL_GetTicks() - playback_start;
}

void handle_input() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_MOUSEBUTTONUP:
            if (!playback_paused) {
                playback_pause_tick = get_playback_tick();
                puts("Playback paused");
            } else {
                playback_start = (SDL_GetTicks() - playback_pause_tick);
                puts("Playback resumed");
            }
            playback_paused = !playback_paused;
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
    while (recording->NextPacketTimestamp <= playback_tick) {
        if (!recording_ProcessNextPacket(recording, gamestate)) {
            fprintf(stderr, "Could not process packet: %s\n", get_last_error());
            abort();
        }
    }

    // Advance the gamestate
    gamestate->CurrentTick = playback_tick;
}

void handle_render() {
    // Render gamestate
    {
        SDL_LockTexture(sdl_texture_gamestate,
                        NULL,
                        (void **)&canvas_gamestate.Buffer,
                        &canvas_gamestate.Stride);
        canvas_DrawRectangle(&canvas_gamestate,
                             &(struct trc_pixel){},
                             0,
                             0,
                             canvas_gamestate.Width,
                             canvas_gamestate.Height);
        if (!renderer_DrawGamestate(&render_options,
                                    gamestate,
                                    &canvas_gamestate)) {
            fprintf(stderr, "Could not render gamestate\n");
            abort();
        }
        SDL_UnlockTexture(sdl_texture_gamestate);
    }

    // Render the rest
    {
        SDL_LockTexture(sdl_texture_output,
                        NULL,
                        (void **)&canvas_output.Buffer,
                        &canvas_output.Stride);
        canvas_overlay_slice.Stride = canvas_output.Stride;
        canvas_overlay_slice.Buffer =
                (uint8_t *)canvas_GetPixel(&canvas_output, viewLeftX, viewTopY);
        canvas_DrawRectangle(&canvas_output,
                             &(struct trc_pixel){},
                             0,
                             0,
                             canvas_output.Width,
                             canvas_output.Height);

        if (!renderer_DrawOverlay(&render_options,
                                  gamestate,
                                  &canvas_overlay_slice)) {
            fprintf(stderr, "Could not render overlay\n");
            abort();
        }

        int offsetX = canvas_output.Width - 160 + 12;
        int offsetY = 4;

        if (!renderer_DrawStatusBars(&render_options,
                                     gamestate,
                                     &canvas_output,
                                     &offsetX,
                                     &offsetY)) {
            fprintf(stderr, "Could not render status bars\n");
            abort();
        }

        if (!renderer_DrawInventoryArea(&render_options,
                                        gamestate,
                                        &canvas_output,
                                        &offsetX,
                                        &offsetY)) {
            fprintf(stderr, "Could not render inventory\n");
            abort();
        }

        if (gamestate->Version->Features.IconBar) {
            if (!renderer_DrawIconBar(&render_options,
                                      gamestate,
                                      &canvas_output,
                                      &offsetX,
                                      &offsetY)) {
                fprintf(stderr, "Could not render icon bar\n");
                abort();
            }
        }

        int max_container_y = canvas_output.Height - 4 - 32;
        for (struct trc_container *container_iterator =
                     gamestate->ContainerList;
             container_iterator != NULL && offsetY < max_container_y;
             container_iterator =
                     (struct trc_container *)container_iterator->hh.next) {
            if (!renderer_DrawContainer(&render_options,
                                        gamestate,
                                        &canvas_output,
                                        container_iterator,
                                        false,
                                        canvas_output.Width,
                                        max_container_y,
                                        &offsetX,
                                        &offsetY)) {
                fprintf(stderr, "Could not render container\n");
                abort();
            }
        }
        SDL_UnlockTexture(sdl_texture_output);
    }

    // Render textures to screen
    SDL_SetRenderDrawColor(sdl_renderer, 0, 0, 0, 255);
    SDL_RenderClear(sdl_renderer);
    if (SDL_RenderCopy(sdl_renderer, sdl_texture_background, NULL, NULL) != 0) {
        fprintf(stderr,
                "Could not render texture to screen: %s\n",
                SDL_GetError());
        abort();
    }
    SDL_Rect dest = {
            .x = viewLeftX,
            .y = viewTopY,
            .w = (viewRightX - viewLeftX),
            .h = (viewBottomY - viewTopY)
    };
    if (SDL_RenderCopy(sdl_renderer, sdl_texture_gamestate, NULL, &dest) != 0) {
        fprintf(stderr,
                "Could not render texture to screen: %s\n",
                SDL_GetError());
        abort();
    }
    if (SDL_RenderCopy(sdl_renderer, sdl_texture_output, NULL, NULL) != 0) {
        fprintf(stderr,
                "Could not render texture to screen: %s\n",
                SDL_GetError());
        abort();
    }
    SDL_RenderPresent(sdl_renderer);
}

void handle_stats() {
    Uint32 current_tick = SDL_GetTicks();
    stats_frames += 1;
    if (current_tick - stats_last_print >= 5000) {
        double fps = stats_frames / ((current_tick - stats_last_print) / 1000.0);
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
    reader->Data = file->View;
    reader->Length = file->Size;
    return true;
}

SDL_Texture *create_texture(int width, int height) {
    SDL_Texture *texture = SDL_CreateTexture(sdl_renderer,
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

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    trc_ChangeErrorReporting(TRC_ERROR_REPORT_MODE_TEXT);

    // Load files
    if (!open_memory_file("files/test.trp", &file_trp, &reader_trp)) {
        return 1;
    }

    if (!open_memory_file("files/Tibia.pic", &file_pic, &reader_pic)) {
        return 1;
    }

    if (!open_memory_file("files/Tibia.spr", &file_spr, &reader_spr)) {
        return 1;
    }

    if (!open_memory_file("files/Tibia.dat", &file_dat, &reader_dat)) {
        return 1;
    }

    if (!version_Load(7,
                      41,
                      0,
                      &reader_pic,
                      &reader_spr,
                      &reader_dat,
                      &version)) {
        fprintf(stderr, "Could not load files\n");
        return 1;
    }

    memoryfile_Close(&file_pic);
    memoryfile_Close(&file_spr);
    memoryfile_Close(&file_dat);

    // Load recording
    recording = recording_Create(RECORDING_FORMAT_TRP);
    if (!recording_Open(recording, &reader_trp, version)) {
        fprintf(stderr, "Could not load recording\n");
        return 1;
    }

    // Create gamestate
    gamestate = gamestate_Create(recording->Version);

    // Setup rendering
#ifdef NDEBUG
    render_options.Width = 1280;
    render_options.Height = 720;
#else
    render_options.Width = NATIVE_RESOLUTION_X + 160; // sidebar width = 160
    render_options.Height = NATIVE_RESOLUTION_Y;
#endif

    // Init SDL
    int ret = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    if (ret != 0) {
        fprintf(stderr, "Could not initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    sdl_window = SDL_CreateWindow("emscriptenplayer",
                                  SDL_WINDOWPOS_UNDEFINED,
                                  SDL_WINDOWPOS_UNDEFINED,
                                  render_options.Width,
                                  render_options.Height,
                                  SDL_WINDOW_SHOWN);
    if (!sdl_window) {
        fprintf(stderr, "Could not create window: %s\n", SDL_GetError());
        return 1;
    }

    sdl_renderer = SDL_CreateRenderer(sdl_window,
                                      -1,
                                      SDL_RENDERER_ACCELERATED |
                                              /*SDL_RENDERER_PRESENTVSYNC |*/
                                              SDL_RENDERER_TARGETTEXTURE);
    if (!sdl_renderer) {
        fprintf(stderr, "Could not create renderer: %s\n", SDL_GetError());
        return 1;
    }

    SDL_SetRenderDrawBlendMode(sdl_renderer, SDL_BLENDMODE_BLEND);

    // Create canvases, which are (going to be) backed by SDL_Textures
    canvas_gamestate.Width = NATIVE_RESOLUTION_X;
    canvas_gamestate.Height = NATIVE_RESOLUTION_Y;
    sdl_texture_gamestate = create_texture(canvas_gamestate.Width, canvas_gamestate.Height);

    canvas_output.Width = render_options.Width;
    canvas_output.Height = render_options.Height;
    sdl_texture_output = create_texture(canvas_output.Width, canvas_output.Height);

    {
        int max_x = render_options.Width - 160;
        int max_y = render_options.Height;
        float minScale = MIN(max_x / (float)NATIVE_RESOLUTION_X,
                             max_y / (float)NATIVE_RESOLUTION_Y);
        viewLeftX = (max_x - (int)(NATIVE_RESOLUTION_X * minScale)) / 2;
        viewTopY = (max_y - (int)(NATIVE_RESOLUTION_Y * minScale)) / 2;
        viewRightX = viewLeftX + (int)(NATIVE_RESOLUTION_X * minScale);
        viewBottomY = viewTopY + (int)(NATIVE_RESOLUTION_Y * minScale);
        canvas_overlay_slice.Width = (viewRightX - viewLeftX);
        canvas_overlay_slice.Height = (viewBottomY - viewTopY);
    }

    // Start playback (assume only first packet has time = 0)
    playback_start = SDL_GetTicks();
    gamestate->CurrentTick = get_playback_tick();
    if (!recording_ProcessNextPacket(recording, gamestate)) {
        fprintf(stderr, "Could not process packet\n");
        return 1;
    }

    // Render the background texture, as that is static and only needs to be done once
    struct trc_canvas canvas_background = {
            .Width = render_options.Width,
            .Height = render_options.Height
    };
    sdl_texture_background = create_texture(canvas_background.Width, canvas_background.Height);
    SDL_LockTexture(sdl_texture_background, NULL, (void **)&canvas_background.Buffer, &canvas_background.Stride);
    renderer_RenderClientBackground(gamestate,
                                    &canvas_background,
                                    0,
                                    0,
                                    canvas_background.Width,
                                    canvas_background.Height);
    SDL_UnlockTexture(sdl_texture_background);

    // Init stats
    stats_frames = 0;
    stats_frames_avg = 0.0f;
    stats_last_print = playback_start;

    playback_paused = false;

    emscripten_set_main_loop(main_loop, 0, 1);

    // Deallocate
    SDL_DestroyTexture(sdl_texture_background);
    SDL_DestroyTexture(sdl_texture_output);
    SDL_DestroyTexture(sdl_texture_gamestate);
    SDL_DestroyRenderer(sdl_renderer);
    SDL_Quit();
    gamestate_Free(gamestate);
    recording_Free(recording);
    version_Free(version);
    memoryfile_Close(&file_trp);

    return 0;
}
