#include <emscripten.h>
#include <SDL2/SDL.h>

#include "lib/canvas.h"
#include "lib/datareader.h"
#include "lib/gamestate.h"
#include "lib/recordings.h"
#include "lib/renderer.h"
#include "lib/versions.h"
#include "memoryfile.h"

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
static struct trc_canvas *canvas_gamestate;
static struct trc_canvas *canvas_output;
static struct trc_canvas canvas_overlay_slice;
static struct trc_render_options render_options;

static int viewLeftX;
static int viewTopY;
static int viewRightX;
static int viewBottomY;

static SDL_Window *sdl_window;
static SDL_Renderer *sdl_renderer;
static SDL_Texture *sdl_texture;

static uint32_t playback_start;

static int stats_frames;
static float stats_frames_avg;
static uint32_t stats_last_print;

void main_loop() {
    uint32_t current_tick = SDL_GetTicks();

    /* Read input */
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            exit(0);
        default:
            break;
        }
    }

    /* Calculate elapsed playback time and process packets until we have caught
     * up  */
    uint32_t playback_elapsed = current_tick - playback_start;
    while (recording->NextPacketTimestamp <= playback_elapsed) {
        if (!recording_ProcessNextPacket(recording, gamestate)) {
            fprintf(stderr, "Could not process packet\n");
            abort();
        }
    }

    /* Render to canvas */
    gamestate->CurrentTick = playback_elapsed;

    canvas_DrawRectangle(canvas_gamestate,
                         &(struct trc_pixel){},
                         0,
                         0,
                         canvas_gamestate->Width,
                         canvas_gamestate->Height);

    renderer_RenderClientBackground(gamestate,
                                    canvas_output,
                                    0,
                                    0,
                                    canvas_output->Width,
                                    canvas_output->Height);

    if (!renderer_DrawGamestate(&render_options, gamestate, canvas_gamestate)) {
        fprintf(stderr, "Could not render gamestate\n");
        abort();
    }

    canvas_RescaleClone(canvas_output,
                        viewLeftX,
                        viewTopY,
                        viewRightX,
                        viewBottomY,
                        canvas_gamestate);

    if (!renderer_DrawOverlay(&render_options,
                              gamestate,
                              &canvas_overlay_slice)) {
        fprintf(stderr, "Could not render overlay\n");
        abort();
    }

    int offsetX = canvas_output->Width - 160 + 12;
    int offsetY = 4;

    if (!renderer_DrawStatusBars(&render_options,
                                 gamestate,
                                 canvas_output,
                                 &offsetX,
                                 &offsetY)) {
        fprintf(stderr, "Could not render status bars\n");
        abort();
    }

    if (!renderer_DrawInventoryArea(&render_options,
                                    gamestate,
                                    canvas_output,
                                    &offsetX,
                                    &offsetY)) {
        fprintf(stderr, "Could not render inventory\n");
        abort();
    }

    if (gamestate->Version->Features.IconBar) {
        if (!renderer_DrawIconBar(&render_options,
                                  gamestate,
                                  canvas_output,
                                  &offsetX,
                                  &offsetY)) {
            fprintf(stderr, "Could not render icon bar\n");
            abort();
        }
    }

    int max_container_y = canvas_output->Height - 4 - 32;
    for (struct trc_container *container_iterator = gamestate->ContainerList;
         container_iterator != NULL && offsetY < max_container_y;
         container_iterator =
                 (struct trc_container *)container_iterator->hh.next) {
        if (!renderer_DrawContainer(&render_options,
                                    gamestate,
                                    canvas_output,
                                    container_iterator,
                                    false,
                                    canvas_output->Width,
                                    max_container_y,
                                    &offsetX,
                                    &offsetY)) {
            fprintf(stderr, "Could not render container\n");
            abort();
        }
    }

    /* Copy canvas to SDL texture */
    void *data;
    int pitch;
    if (SDL_LockTexture(sdl_texture, NULL, &data, &pitch) != 0) {
        fprintf(stderr, "Could not lock texture: %s\n", SDL_GetError());
        abort();
    }
    char *texture_data = (char *)data;

    for (int y = 0; y < canvas_output->Height; ++y) {
        struct trc_pixel *line = canvas_GetPixel(canvas_output, 0, y);
        memcpy(texture_data + (y * pitch),
               line,
               sizeof(struct trc_pixel) * canvas_output->Width);
    }

    SDL_UnlockTexture(sdl_texture);

    /* Render texture to screen */
    if (SDL_RenderCopy(sdl_renderer, sdl_texture, NULL, NULL) != 0) {
        fprintf(stderr,
                "Could not render texture to screen: %s\n",
                SDL_GetError());
        abort();
    }
    SDL_RenderPresent(sdl_renderer);

    /* Stats */
    stats_frames += 1;
    if (current_tick - stats_last_print >= 5000) {
        float fps = stats_frames / ((current_tick - stats_last_print) / 1000.0);
        stats_frames_avg += fps;
        stats_frames_avg /= 2.0;
        printf("FPS: %.2f Average FPS: %.2f\n", fps, stats_frames_avg);

        stats_frames = 0;
        stats_last_print = current_tick;
    }
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

int main(int argc, char *argv[]) {
    /* Load files */
    if (!open_memory_file("test.trp", &file_trp, &reader_trp)) {
        return 1;
    }

    if (!open_memory_file("Tibia.pic", &file_pic, &reader_pic)) {
        return 1;
    }

    if (!open_memory_file("Tibia.spr", &file_spr, &reader_spr)) {
        return 1;
    }

    if (!open_memory_file("Tibia.dat", &file_dat, &reader_dat)) {
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

    /* Load recording */
    recording = recording_Create(RECORDING_FORMAT_TRP);
    if (!recording_Open(recording, &reader_trp, version)) {
        fprintf(stderr, "Could not load recording\n");
        return 1;
    }

    /* Create gamestate */
    gamestate = gamestate_Create(recording->Version);

    /* Setup rendering */
#ifdef NDEBUG
    render_options.Width = 1280;
    render_options.Height = 720;
#else
    render_options.Width = NATIVE_RESOLUTION_X + 160; // sidebar width = 160
    render_options.Height = NATIVE_RESOLUTION_Y;
#endif

    canvas_gamestate = canvas_Create(NATIVE_RESOLUTION_X, NATIVE_RESOLUTION_Y);
    canvas_output = canvas_Create(render_options.Width, render_options.Height);
    float minScale = MIN(canvas_output->Width / (float)NATIVE_RESOLUTION_X,
                         canvas_output->Height / (float)NATIVE_RESOLUTION_Y);
    viewLeftX = (canvas_output->Width - (NATIVE_RESOLUTION_X * minScale)) / 2;
    viewTopY = (canvas_output->Height - (NATIVE_RESOLUTION_Y * minScale)) / 2;
    viewRightX = viewLeftX + (NATIVE_RESOLUTION_X * minScale);
    viewBottomY = viewTopY + (NATIVE_RESOLUTION_Y * minScale);
    canvas_Slice(canvas_output,
                 viewLeftX,
                 viewTopY,
                 viewRightX,
                 viewBottomY,
                 &canvas_overlay_slice);

    /* Init SDL */
    int ret = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    if (ret != 0) {
        fprintf(stderr, "Could not initialize SDL: %s\n", SDL_GetError());
        return 1;
    }

    sdl_window = SDL_CreateWindow("emscriptenplayer",
                                  SDL_WINDOWPOS_UNDEFINED,
                                  SDL_WINDOWPOS_UNDEFINED,
                                  canvas_output->Width,
                                  canvas_output->Height,
                                  SDL_WINDOW_SHOWN);
    if (!sdl_window) {
        fprintf(stderr, "Could not create window: %s\n", SDL_GetError());
        return 1;
    }

    sdl_renderer = SDL_CreateRenderer(sdl_window, -1, SDL_RENDERER_SOFTWARE);
    if (!sdl_renderer) {
        fprintf(stderr, "Could not create renderer: %s\n", SDL_GetError());
        return 1;
    }

    sdl_texture = SDL_CreateTexture(sdl_renderer,
                                    SDL_PIXELFORMAT_RGBA32,
                                    SDL_TEXTUREACCESS_STREAMING,
                                    canvas_output->Width,
                                    canvas_output->Height);
    if (!sdl_texture) {
        fprintf(stderr, "Could not create texture: %s\n", SDL_GetError());
        return 1;
    }

    /* Start playback (assume only first packet has time = 0) */
    playback_start = SDL_GetTicks();
    if (!recording_ProcessNextPacket(recording, gamestate)) {
        fprintf(stderr, "Could not process packet\n");
        return 1;
    }

    /* Init stats */
    stats_frames = 0;
    stats_frames_avg = 0.0f;
    stats_last_print = playback_start;

    emscripten_set_main_loop(main_loop, 0, 1);

    /* Deallocate */
    canvas_Free(canvas_output);
    canvas_Free(canvas_gamestate);
    gamestate_Free(gamestate);
    recording_Free(recording);
    version_Free(version);
    memoryfile_Close(&file_trp);

    return 0;
}
