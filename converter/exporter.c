/*
 * Copyright 2011-2016 "Silver Squirrel Software Handelsbolag"
 * Copyright 2023-2024 "John Högberg"
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

#include "exporter.h"

#include "datareader.h"
#include "encoding.h"
#include "memoryfile.h"
#include "recordings.h"
#include "renderer.h"
#include "versions.h"

#include <math.h>

#include "utils.h"

#ifdef _WIN32
#    define DIR_SEP "\\"
#else
#    define DIR_SEP "/"
#endif

#define SIDEBAR_WIDTH 160

static uint32_t exporter_FrameTimestamp(uint32_t frameNumber,
                                        uint32_t frameRate) {
    return (frameNumber * 1000) / frameRate;
}

static bool exporter_DrawInterface(const struct trc_render_options *options,
                                   struct trc_game_state *gamestate,
                                   struct trc_canvas *canvas) {
    int offsetX, offsetY;

    if (!options->SkipRenderingInventory) {
        offsetX = (canvas->Width - SIDEBAR_WIDTH) + 12;
        offsetY = 4;
    } else {
        offsetX = 4;
        offsetY = 4;
    }

    if (!options->SkipRenderingStatusBars) {
        renderer_DrawStatusBars(options, gamestate, canvas, &offsetX, &offsetY);
    }

    if (!options->SkipRenderingInventory) {
        renderer_DrawInventoryArea(options,
                                   gamestate,
                                   canvas,
                                   &offsetX,
                                   &offsetY);
    }

    if (!options->SkipRenderingIconBar &&
        (gamestate->Version)->Features.IconBar) {
        if (!renderer_DrawIconBar(options,
                                  gamestate,
                                  canvas,
                                  &offsetX,
                                  &offsetY)) {
            return trc_ReportError("Failed to render the icon bar");
        }
    }

    if (!options->SkipRenderingInventory) {
        struct trc_container *containerIterator;
        int maxContainerY;

        maxContainerY = (canvas->Height - 4) - 32;

        for (containerIterator = gamestate->ContainerList;
             containerIterator != NULL;
             containerIterator =
                     (struct trc_container *)(containerIterator->hh.next)) {
            renderer_DrawContainer(options,
                                   gamestate,
                                   canvas,
                                   containerIterator,
                                   false,
                                   canvas->Width,
                                   maxContainerY,
                                   &offsetX,
                                   &offsetY);
            if (offsetY >= maxContainerY) {
                break;
            }
        }
    }

    /* TODO: Render battle list? */
    return true;
}

static void exporter_CopyRect(struct trc_canvas *canvas,
                              int dX,
                              int dY,
                              const struct trc_canvas *from,
                              int sX,
                              int sY,
                              int width,
                              int height) {
    int lineOffset, lineWidth;

    if (!((dX < canvas->Width) && (dY < canvas->Height) && (dX + width >= 0) &&
          (dY + height >= 0))) {
        return;
    }

    height = MIN(MIN(height, canvas->Height - dY), from->Height - sY);

    lineWidth = MIN(MIN(width, from->Width - sX), canvas->Width - dX);
    lineOffset = MAX(MAX(-sY, -dY), 0);

    for (; lineOffset < height; lineOffset++) {
        memcpy(canvas_GetPixel(canvas, dX, dY + lineOffset),
               canvas_GetPixel(from, sX, sY + lineOffset),
               lineWidth * sizeof(struct trc_pixel));
    }
}

/* FIXME: Naive and hysterically slow bilinear rescaler, we'll move this to
 * libswscale once the redesigned player has been merged and we can chuck
 * SDL2, leaving only the libav backend; having to support the two in a common
 * API is too annoying at the moment. */
static void exporter_RescaleClone(struct trc_canvas *canvas,
                                  int leftX,
                                  int topY,
                                  int rightX,
                                  int bottomY,
                                  const struct trc_canvas *from) {
    float scaleFactorX, scaleFactorY;
    int width, height;

    width = rightX - leftX;
    height = bottomY - topY;
    ASSERT(width >= 0 && height >= 0);

    scaleFactorX = (float)width / (float)from->Width;
    scaleFactorY = (float)height / (float)from->Height;

    if (scaleFactorX == 1 && scaleFactorY == 1) {
        /* Hacky fast path for when we don't need rescaling, speeds up testing
         * quite a lot. */
        exporter_CopyRect(canvas,
                          leftX,
                          topY,
                          from,
                          0,
                          0,
                          from->Width,
                          from->Height);
        return;
    }

#ifdef _OPENMP
#    pragma omp parallel for
#endif
    for (int toY = 0; toY < height; toY++) {
        const int fromY0 = MIN(toY / scaleFactorY, from->Height - 1);
        const int fromY1 = MIN(fromY0 + 1, from->Height - 1);
        const float subOffsY = (toY / scaleFactorY) - fromY0;

        for (int toX = 0; toX < width; toX++) {
            const int fromX0 = MIN(toX / scaleFactorX, from->Width - 1);
            const int fromX1 = MIN(fromX0 + 1, from->Width - 1);
            const float subOffsX = (toX / scaleFactorX) - fromX0;

            const struct trc_pixel *srcPixels[4];
            struct trc_pixel *dstPixel;

            float outRed, outGreen, outBlue, outAlpha;

            srcPixels[0] = canvas_GetPixel(from, fromX0, fromY0);
            srcPixels[1] = canvas_GetPixel(from, fromX1, fromY0);
            srcPixels[2] = canvas_GetPixel(from, fromX0, fromY1);
            srcPixels[3] = canvas_GetPixel(from, fromX1, fromY1);

            outRed = srcPixels[0]->Red * (1 - subOffsX) * (1 - subOffsY);
            outGreen = srcPixels[0]->Green * (1 - subOffsX) * (1 - subOffsY);
            outBlue = srcPixels[0]->Blue * (1 - subOffsX) * (1 - subOffsY);
            outAlpha = srcPixels[0]->Alpha * (1 - subOffsX) * (1 - subOffsY);

            outRed += srcPixels[1]->Red * (subOffsX) * (1 - subOffsY);
            outGreen += srcPixels[1]->Green * (subOffsX) * (1 - subOffsY);
            outBlue += srcPixels[1]->Blue * (subOffsX) * (1 - subOffsY);
            outAlpha += srcPixels[1]->Alpha * (subOffsX) * (1 - subOffsY);

            outRed += srcPixels[2]->Red * (1 - subOffsX) * (subOffsY);
            outGreen += srcPixels[2]->Green * (1 - subOffsX) * (subOffsY);
            outBlue += srcPixels[2]->Blue * (1 - subOffsX) * (subOffsY);
            outAlpha += srcPixels[2]->Alpha * (1 - subOffsX) * (subOffsY);

            outRed += srcPixels[3]->Red * (subOffsX) * (subOffsY);
            outGreen += srcPixels[3]->Green * (subOffsX) * (subOffsY);
            outBlue += srcPixels[3]->Blue * (subOffsX) * (subOffsY);
            outAlpha += srcPixels[3]->Alpha * (subOffsX) * (subOffsY);

            dstPixel = canvas_GetPixel(canvas, toX + leftX, toY + topY);

            dstPixel->Red = (uint8_t)(outRed + 0.5);
            dstPixel->Green = (uint8_t)(outGreen + 0.5);
            dstPixel->Blue = (uint8_t)(outBlue + 0.5);
            dstPixel->Alpha = (uint8_t)(outAlpha + 0.5);
        }
    }
}

static bool exporter_ConvertVideo(const struct export_settings *settings,
                                  struct trc_recording *recording,
                                  struct trc_game_state *gamestate,
                                  struct trc_encoder *encoder,
                                  struct trc_canvas *mapCanvas,
                                  struct trc_canvas *outputCanvas,
                                  uint32_t frameRate,
                                  uint32_t startTime,
                                  uint32_t endTime) {
    const struct trc_render_options *renderOptions = &settings->RenderOptions;
    int viewLeftX, viewTopY, viewRightX, viewBottomY;
    uint32_t frameNumber = 0, frameTimestamp;
    struct trc_canvas overlaySlice;

    {
        /* Determine the bounds of the viewport, maintaning the aspect ratio
         * similar to how Tibia does it. */
        float maxX = outputCanvas->Width;
        float maxY = outputCanvas->Height;

        if (!renderOptions->SkipRenderingInventory) {
            maxX -= SIDEBAR_WIDTH;
        }

        if (maxX <= 0 || maxY <= 0) {
            return trc_ReportError("Resolution too small to fit inventory");
        }

        float minScale =
                MIN(maxX / NATIVE_RESOLUTION_X, maxY / NATIVE_RESOLUTION_Y);

        viewLeftX = (maxX - (NATIVE_RESOLUTION_X * minScale)) / 2;
        viewTopY = (maxY - (NATIVE_RESOLUTION_Y * minScale)) / 2;
        viewRightX = viewLeftX + (NATIVE_RESOLUTION_X * minScale),
        viewBottomY = viewTopY + (NATIVE_RESOLUTION_Y * minScale);
    }

    canvas_Slice(outputCanvas,
                 viewLeftX,
                 viewTopY,
                 viewRightX,
                 viewBottomY,
                 &overlaySlice);

    /* Clip start/end to recording bounds, allowing another second in case of
     * an abrupt end to the recording. */
    startTime = MIN(startTime, recording->Runtime);
    endTime = MIN(endTime, recording->Runtime + 1000);

    do {
        if (!recording_ProcessNextPacket(recording, gamestate)) {
            return trc_ReportError("Failed to read packet");
        }

        ASSERT(gamestate->Player.Id != 0);

        do {
            frameTimestamp = exporter_FrameTimestamp(frameNumber++, frameRate);
            gamestate->CurrentTick = frameTimestamp;

            if ((frameTimestamp < startTime) ||
                (frameNumber % settings->FrameSkip)) {
                continue;
            }

            /* Wipe the background in the same manner Tibia does it, leaving
             * empty spots on the map black. */
            canvas_DrawRectangle(mapCanvas,
                                 &(struct trc_pixel){},
                                 0,
                                 0,
                                 NATIVE_RESOLUTION_X,
                                 NATIVE_RESOLUTION_Y);

            renderer_RenderClientBackground(gamestate,
                                            outputCanvas,
                                            0,
                                            0,
                                            outputCanvas->Width,
                                            outputCanvas->Height);

            if (!renderer_DrawGamestate(renderOptions, gamestate, mapCanvas)) {
                return trc_ReportError("Could not draw gamestate");
            }

            exporter_RescaleClone(outputCanvas,
                                  viewLeftX,
                                  viewTopY,
                                  viewRightX,
                                  viewBottomY,
                                  mapCanvas);

            if (!renderer_DrawOverlay(renderOptions,
                                      gamestate,
                                      &overlaySlice)) {
                return trc_ReportError("Could not draw game overlay");
            } else if (!exporter_DrawInterface(renderOptions,
                                               gamestate,
                                               outputCanvas)) {
                return trc_ReportError("Could not draw interface area");
            } else if (!encoder_WriteFrame(encoder, outputCanvas)) {
                return trc_ReportError("Could not encode/write frame");
            } else if ((frameTimestamp % 500) == 0) {
                fprintf(stdout,
                        "progress: %u / %u / %u\n",
                        frameTimestamp,
                        startTime,
                        endTime);
                fflush(stdout);
            }
        } while (frameTimestamp <=
                 MIN(recording->NextPacketTimestamp, endTime));
    } while (frameTimestamp <= endTime && !recording->HasReachedEnd);

    return true;
}

struct trc_exporter_data {
    struct memory_file PictureFile;
    struct memory_file SpriteFile;
    struct memory_file TypeFile;

    struct trc_data_reader Pictures;
    struct trc_data_reader Sprites;
    struct trc_data_reader Types;
};

static bool exporter_OpenData(const char *dataFolder,
                              struct trc_exporter_data *data) {
    char picturePath[FILENAME_MAX];
    char spritePath[FILENAME_MAX];
    char typePath[FILENAME_MAX];

    if (snprintf(picturePath,
                 FILENAME_MAX,
                 "%s%s",
                 dataFolder,
                 DIR_SEP "Tibia.pic") < 0) {
        return trc_ReportError("Could not merge data path with Tibia.pic");
    }

    if (snprintf(spritePath,
                 FILENAME_MAX,
                 "%s%s",
                 dataFolder,
                 DIR_SEP "Tibia.spr") < 0) {
        return trc_ReportError("Could not merge data path with Tibia.spr");
    }

    if (snprintf(typePath,
                 FILENAME_MAX,
                 "%s%s",
                 dataFolder,
                 DIR_SEP "Tibia.dat") < 0) {
        return trc_ReportError("Could not merge data path with Tibia.dat");
    }

    if (!memoryfile_Open(picturePath, &data->PictureFile)) {
        (void)trc_ReportError("Failed to open Tibia.pic");
    } else {
        if (!memoryfile_Open(spritePath, &data->SpriteFile)) {
            (void)trc_ReportError("Failed to open Tibia.spr");
        } else {
            if (!memoryfile_Open(typePath, &data->TypeFile)) {
                (void)trc_ReportError("Failed to open Tibia.dat");
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

static void exporter_CloseData(struct trc_exporter_data *data) {
    memoryfile_Close(&data->PictureFile);
    memoryfile_Close(&data->SpriteFile);
    memoryfile_Close(&data->TypeFile);
}

static bool exporter_OpenRecording(const struct export_settings *settings,
                                   const char *dataFolder,
                                   const char *path,
                                   struct trc_data_reader *reader,
                                   struct trc_recording **out) {
    enum TrcRecordingFormat inputFormat = settings->InputFormat;
    struct trc_recording *recording;
    int major, minor, preview;

    if (inputFormat == RECORDING_FORMAT_UNKNOWN) {
        inputFormat = recording_GuessFormat(path, reader);
        fprintf(stdout,
                "warning: Unknown recording format, guessing %s\n",
                recording_FormatName(inputFormat));
        fflush(stdout);
    }

    recording = recording_Create(inputFormat);

    major = settings->DesiredTibiaVersion.Major;
    minor = settings->DesiredTibiaVersion.Minor;
    preview = settings->DesiredTibiaVersion.Preview;

    /* Ask the file for the version if none was provided. */
    if ((major | minor | preview) > 0 ||
        recording_QueryTibiaVersion(recording,
                                    reader,
                                    &major,
                                    &minor,
                                    &preview)) {
        struct trc_exporter_data data = {0};

        if (exporter_OpenData(dataFolder, &data)) {
            struct trc_version *version;

            bool success = version_Load(major,
                                        minor,
                                        preview,
                                        &data.Pictures,
                                        &data.Sprites,
                                        &data.Types,
                                        &version);
            exporter_CloseData(&data);

            if (success) {
                if (recording_Open(recording, reader, version)) {
                    *out = recording;
                    return true;
                }

                version_Free(version);
            }
        }
    }

    recording_Free(recording);

    return false;
}

bool exporter_Export(const struct export_settings *settings,
                     const char *dataFolder,
                     const char *inputPath,
                     const char *outputPath) {
    struct trc_canvas *mapCanvas, *outputCanvas;
    struct trc_recording *recording;
    struct memory_file file;
    bool result = false;

    if (!memoryfile_Open(inputPath, &file)) {
        return trc_ReportError("Failed to open input recording");
    }

    mapCanvas = canvas_Create(NATIVE_RESOLUTION_X, NATIVE_RESOLUTION_Y);
    outputCanvas = canvas_Create(settings->RenderOptions.Width,
                                 settings->RenderOptions.Height);

    struct trc_data_reader reader = {.Length = file.Size, .Data = file.View};
    if (exporter_OpenRecording(settings,
                               dataFolder,
                               inputPath,
                               &reader,
                               &recording)) {
        struct trc_encoder *encoder = encoder_Create(settings->EncodeLibrary);

        if (encoder_Open(encoder,
                         settings->OutputFormat,
                         settings->OutputEncoding,
                         settings->EncoderFlags,
                         outputCanvas->Width,
                         outputCanvas->Height,
                         settings->FrameRate,
                         outputPath)) {
            struct trc_game_state *gamestate =
                    gamestate_Create(recording->Version);

            result = exporter_ConvertVideo(settings,
                                           recording,
                                           gamestate,
                                           encoder,
                                           mapCanvas,
                                           outputCanvas,
                                           settings->FrameRate,
                                           settings->StartTime,
                                           settings->EndTime);

            gamestate_Free(gamestate);
        }

        version_Free(recording->Version);
        recording_Free(recording);
        encoder_Free(encoder);
    }

    canvas_Free(mapCanvas);
    canvas_Free(outputCanvas);
    memoryfile_Close(&file);

    return result;
}
