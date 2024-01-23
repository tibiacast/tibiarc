#ifndef PLAYER_RENDERING_H
#define PLAYER_RENDERING_H

#include <SDL.h>

#include "playback.h"
#include "renderer.h"
#include "canvas.h"

struct rendering {
    SDL_Window *SdlWindow;
    SDL_Renderer *SdlRenderer;

    struct trc_render_options RenderOptions;

    // This texture is the same size as the window
    // It is static, so we render it only once (during startup or window resize)
    SDL_Texture *SdlTextureBackground;
    bool BackgroundRendered;

    // This canvas and texture are always NATIVE_RESOLUTION
    struct trc_canvas CanvasGamestate;
    SDL_Texture *SdlTextureGamestate;

    // This canvas and texture are always the same size as the window
    struct trc_canvas CanvasOutput;
    SDL_Texture *SdlTextureOutput;

    // This rectangle represents where the gamestate texture
    // should be rendered on the output texture, including scaling
    SDL_Rect OverlaySliceRect;
};

bool rendering_Init(struct rendering *rendering,
                    int windowWidth,
                    int windowHeight);
void rendering_Free(struct rendering *rendering);

bool rendering_HandleResize(struct rendering *rendering);
void rendering_Render(struct rendering *rendering,
                      struct playback *playback);

#endif // PLAYER_RENDERING_H
