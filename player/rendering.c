#include "rendering.h"

#include "versions.h"
#include "textrenderer.h"

static bool initSDL(struct rendering *rendering,
                    int windowWidth,
                    int windowHeight) {
    int ret = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER);
    if (ret != 0) {
        fprintf(stderr, "Could not initialize SDL: %s\n", SDL_GetError());
        return false;
    }

    rendering->SdlWindow =
            SDL_CreateWindow("player",
                             SDL_WINDOWPOS_UNDEFINED,
                             SDL_WINDOWPOS_UNDEFINED,
                             windowWidth,
                             windowHeight,
                             SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!rendering->SdlWindow) {
        fprintf(stderr, "Could not create window: %s\n", SDL_GetError());
        return false;
    }

    rendering->SdlRenderer = SDL_CreateRenderer(
            rendering->SdlWindow,
            -1,
            SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC |
                    SDL_RENDERER_TARGETTEXTURE);
    if (!rendering->SdlRenderer) {
        fprintf(stderr, "Could not create renderer: %s\n", SDL_GetError());
        return false;
    }
    SDL_SetRenderDrawBlendMode(rendering->SdlRenderer, SDL_BLENDMODE_BLEND);

    return true;
}

static SDL_Texture *create_texture(SDL_Renderer *SdlRenderer,
                                   int width,
                                   int height) {
    SDL_Texture *texture = SDL_CreateTexture(SdlRenderer,
                                             SDL_PIXELFORMAT_RGBA32,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             width,
                                             height);
    if (!texture) {
        fprintf(stderr, "Could not create texture: %s\n", SDL_GetError());
        return NULL;
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    return texture;
}

static int formatTime(char *buffer, size_t bufferSize, uint32_t milliseconds) {
    // TODO: move to utils.h or similar?
    uint32_t seconds = milliseconds / 1000;
    milliseconds %= 1000;
    uint32_t minutes = seconds / 60;
    seconds %= 60;
    uint32_t hours = minutes / 60;
    minutes %= 60;
    return snprintf(buffer, bufferSize, "%02u:%02u:%02u.%03u", hours, minutes, seconds, milliseconds);
}

bool rendering_Init(struct rendering *rendering,
                    int windowWidth,
                    int windowHeight) {
    if (!initSDL(rendering, windowWidth, windowHeight)) {
        return false;
    }

    return rendering_HandleResize(rendering);
}

void rendering_Free(struct rendering *rendering) {
    SDL_DestroyTexture(rendering->SdlTextureBackground);
    SDL_DestroyTexture(rendering->SdlTextureOutput);
    SDL_DestroyTexture(rendering->SdlTextureGamestate);
    SDL_DestroyRenderer(rendering->SdlRenderer);
    SDL_Quit();
}

bool rendering_HandleResize(struct rendering *rendering) {
    int width;
    int height;
    SDL_GetRendererOutputSize(rendering->SdlRenderer, &width, &height);
    printf("handle resize: %dx%d\n", width, height);
    if (rendering->RenderOptions.Width == width &&
        rendering->RenderOptions.Height == height) {
        fprintf(stderr, "bogus resize event?\n");
        return false;
    }

    // Setup rendering
    rendering->RenderOptions.Width = width;
    rendering->RenderOptions.Height = height;

    // Create canvases, which are (going to be) backed by SDL_Textures

    if (rendering->SdlTextureBackground != NULL) {
        SDL_DestroyTexture(rendering->SdlTextureBackground);
    }
    rendering->SdlTextureBackground =
            create_texture(rendering->SdlRenderer, width, height);
    if (rendering->SdlTextureBackground == NULL) {
        return false;
    }
    rendering->BackgroundRendered = false;

    // gamestate canvas and texture is always the same size, so only need
    // to create them if they aren't already created
    if (rendering->SdlTextureGamestate == NULL) {
        rendering->CanvasGamestate.Width = NATIVE_RESOLUTION_X;
        rendering->CanvasGamestate.Height = NATIVE_RESOLUTION_Y;
        rendering->SdlTextureGamestate = create_texture(rendering->SdlRenderer,
                                                        NATIVE_RESOLUTION_X,
                                                        NATIVE_RESOLUTION_Y);
        if (rendering->SdlTextureGamestate == NULL) {
            return false;
        }
    }

    if (rendering->SdlTextureOutput != NULL) {
        SDL_DestroyTexture(rendering->SdlTextureOutput);
    }
    rendering->CanvasOutput.Width = rendering->RenderOptions.Width;
    rendering->CanvasOutput.Height = rendering->RenderOptions.Height;
    rendering->SdlTextureOutput =
            create_texture(rendering->SdlRenderer,
                           rendering->RenderOptions.Width,
                           rendering->RenderOptions.Height);
    if (rendering->SdlTextureOutput == NULL) {
        return false;
    }

    int max_x = rendering->RenderOptions.Width - 160;
    int max_y = rendering->RenderOptions.Height;
    float minScale = MIN(max_x / (float)NATIVE_RESOLUTION_X,
                         max_y / (float)NATIVE_RESOLUTION_Y);
    rendering->OverlaySliceRect.x =
            (max_x - (int)(NATIVE_RESOLUTION_X * minScale)) / 2;
    rendering->OverlaySliceRect.y =
            (max_y - (int)(NATIVE_RESOLUTION_Y * minScale)) / 2;
    rendering->OverlaySliceRect.w = (int)(NATIVE_RESOLUTION_X * minScale);
    rendering->OverlaySliceRect.h = (int)(NATIVE_RESOLUTION_Y * minScale);

    return true;
}

void rendering_Render(struct rendering *rendering,
                      struct playback *playback) {
    // Render background (if not already done, as this only needs to be done
    // once)
    if (!rendering->BackgroundRendered) {
        struct trc_canvas canvas_background = {
                .Width = rendering->RenderOptions.Width,
                .Height = rendering->RenderOptions.Height};
        SDL_LockTexture(rendering->SdlTextureBackground,
                        NULL,
                        (void **)&canvas_background.Buffer,
                        &canvas_background.Stride);
        renderer_RenderClientBackground(playback->Gamestate,
                                        &canvas_background,
                                        0,
                                        0,
                                        canvas_background.Width,
                                        canvas_background.Height);
        SDL_UnlockTexture(rendering->SdlTextureBackground);
        rendering->BackgroundRendered = true;
    }

    // Render gamestate
    {
        SDL_LockTexture(rendering->SdlTextureGamestate,
                        NULL,
                        (void **)&rendering->CanvasGamestate.Buffer,
                        &rendering->CanvasGamestate.Stride);
        canvas_DrawRectangle(&rendering->CanvasGamestate,
                             &(struct trc_pixel){},
                             0,
                             0,
                             rendering->CanvasGamestate.Width,
                             rendering->CanvasGamestate.Height);
        if (!renderer_DrawGamestate(&rendering->RenderOptions,
                                    playback->Gamestate,
                                    &rendering->CanvasGamestate)) {
            fprintf(stderr, "Could not render gamestate\n");
            abort();
        }
        SDL_UnlockTexture(rendering->SdlTextureGamestate);
    }

    // Render the rest
    {
        SDL_LockTexture(rendering->SdlTextureOutput,
                        NULL,
                        (void **)&rendering->CanvasOutput.Buffer,
                        &rendering->CanvasOutput.Stride);

        canvas_DrawRectangle(&rendering->CanvasOutput,
                             &(struct trc_pixel){},
                             0,
                             0,
                             rendering->CanvasOutput.Width,
                             rendering->CanvasOutput.Height);

        struct trc_canvas overlay_slice;
        overlay_slice.Width = rendering->OverlaySliceRect.w;
        overlay_slice.Height = rendering->OverlaySliceRect.h;
        overlay_slice.Stride = rendering->CanvasOutput.Stride;
        overlay_slice.Buffer =
                (uint8_t *)canvas_GetPixel(&rendering->CanvasOutput,
                                           rendering->OverlaySliceRect.x,
                                           rendering->OverlaySliceRect.y);
        if (!renderer_DrawOverlay(&rendering->RenderOptions,
                                  playback->Gamestate,
                                  &overlay_slice)) {
            fprintf(stderr, "Could not render overlay\n");
            abort();
        }

        int offsetX = rendering->CanvasOutput.Width - 160 + 12;
        int offsetY = 4;

        if (!renderer_DrawStatusBars(&rendering->RenderOptions,
                                     playback->Gamestate,
                                     &rendering->CanvasOutput,
                                     &offsetX,
                                     &offsetY)) {
            fprintf(stderr, "Could not render status bars\n");
            abort();
        }

        if (!renderer_DrawInventoryArea(&rendering->RenderOptions,
                                        playback->Gamestate,
                                        &rendering->CanvasOutput,
                                        &offsetX,
                                        &offsetY)) {
            fprintf(stderr, "Could not render inventory\n");
            abort();
        }

        if (playback->Gamestate->Version->Features.IconBar) {
            if (!renderer_DrawIconBar(&rendering->RenderOptions,
                                      playback->Gamestate,
                                      &rendering->CanvasOutput,
                                      &offsetX,
                                      &offsetY)) {
                fprintf(stderr, "Could not render icon bar\n");
                abort();
            }
        }

        int max_container_y = rendering->CanvasOutput.Height - 4 - 32;
        for (struct trc_container *container_iterator =
                     playback->Gamestate->ContainerList;
             container_iterator != NULL && offsetY < max_container_y;
             container_iterator =
                     (struct trc_container *)container_iterator->hh.next) {
            if (!renderer_DrawContainer(&rendering->RenderOptions,
                                        playback->Gamestate,
                                        &rendering->CanvasOutput,
                                        container_iterator,
                                        false,
                                        rendering->CanvasOutput.Width,
                                        max_container_y,
                                        &offsetX,
                                        &offsetY)) {
                fprintf(stderr, "Could not render container\n");
                abort();
            }
        }

        // Render playback info
        char text[64];
        int textLength = formatTime(text, sizeof(text) / sizeof(text[0]), playback_GetPlaybackTick(playback));
        struct trc_pixel fontColor;
        pixel_SetRGB(&fontColor, 255, 255, 255);
        textrenderer_Render(&playback->Gamestate->Version->Fonts.GameFont,
                            TEXTALIGNMENT_LEFT,
                            TEXTTRANSFORM_NONE,
                            &fontColor,
                            12,
                            12,
                            64,
                            textLength,
                            text,
                            &rendering->CanvasOutput);

        textLength = formatTime(text, sizeof(text) / sizeof(text[0]), playback->Recording->Runtime);
        textrenderer_Render(&playback->Gamestate->Version->Fonts.GameFont,
                            TEXTALIGNMENT_LEFT,
                            TEXTTRANSFORM_NONE,
                            &fontColor,
                            12,
                            28,
                            64,
                            textLength,
                            text,
                            &rendering->CanvasOutput);


        SDL_UnlockTexture(rendering->SdlTextureOutput);
    }

    // Render textures to screen
    SDL_SetRenderDrawColor(rendering->SdlRenderer, 0, 0, 0, 255);
    SDL_RenderClear(rendering->SdlRenderer);
    if (SDL_RenderCopy(rendering->SdlRenderer,
                       rendering->SdlTextureBackground,
                       NULL,
                       NULL) != 0) {
        fprintf(stderr,
                "Could not render texture to screen: %s\n",
                SDL_GetError());
        abort();
    }
    if (SDL_RenderCopy(rendering->SdlRenderer,
                       rendering->SdlTextureGamestate,
                       NULL,
                       &rendering->OverlaySliceRect) != 0) {
        fprintf(stderr,
                "Could not render texture to screen: %s\n",
                SDL_GetError());
        abort();
    }
    if (SDL_RenderCopy(rendering->SdlRenderer,
                       rendering->SdlTextureOutput,
                       NULL,
                       NULL) != 0) {
        fprintf(stderr,
                "Could not render texture to screen: %s\n",
                SDL_GetError());
        abort();
    }
    SDL_RenderPresent(rendering->SdlRenderer);
}
