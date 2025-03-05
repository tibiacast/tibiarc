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

#include "exporter.hpp"

#include "datareader.hpp"
#include "encoding.hpp"
#include "memoryfile.hpp"
#include "recordings.hpp"
#include "renderer.hpp"
#include "versions.hpp"

#include <algorithm>
#include <iostream>

#include "utils.hpp"

#define SIDEBAR_WIDTH 160

namespace trc {
namespace Exporter {

static void DrawInterface(const Renderer::Options &options,
                          Gamestate &gamestate,
                          Canvas &canvas) {
    int offsetX, offsetY;

    if (!options.SkipRenderingInventory) {
        offsetX = (canvas.Width - SIDEBAR_WIDTH) + 12;
        offsetY = 4;
    } else {
        offsetX = 4;
        offsetY = 4;
    }

    if (!options.SkipRenderingStatusBars) {
        Renderer::DrawStatusBars(gamestate, canvas, offsetX, offsetY);
    }

    if (!options.SkipRenderingInventory) {
        Renderer::DrawInventoryArea(gamestate, canvas, offsetX, offsetY);
    }

    if (!options.SkipRenderingIconBar && gamestate.Version.Features.IconBar) {
        Renderer::DrawIconBar(gamestate, canvas, offsetX, offsetY);
    }

    if (!options.SkipRenderingInventory) {
        int maxContainerY = (canvas.Height - 4) - 32;

        for (auto &[_, container] : gamestate.Containers) {
            Renderer::DrawContainer(gamestate,
                                    canvas,
                                    container,
                                    false,
                                    canvas.Width,
                                    maxContainerY,
                                    offsetX,
                                    offsetY);
            if (offsetY >= maxContainerY) {
                break;
            }
        }
    }
}

static void CopyRect(Canvas &canvas,
                     int dX,
                     int dY,
                     const Canvas &from,
                     int sX,
                     int sY,
                     int width,
                     int height) {
    int lineOffset, lineWidth;

    if (!((dX < canvas.Width) && (dY < canvas.Height) && (dX + width >= 0) &&
          (dY + height >= 0))) {
        return;
    }

    height = std::min(std::min(height, canvas.Height - dY), from.Height - sY);

    lineWidth = std::min(std::min(width, from.Width - sX), canvas.Width - dX);
    lineOffset = std::max(std::max(-sY, -dY), 0);

    for (; lineOffset < height; lineOffset++) {
        memcpy(&canvas.GetPixel(dX, dY + lineOffset),
               &from.GetPixel(sX, sY + lineOffset),
               lineWidth * sizeof(Pixel));
    }
}

/* FIXME: Naive and hysterically slow bilinear rescaler, we'll move this to
 * libswscale once the redesigned player has been merged and we can chuck
 * SDL2, leaving only the libav backend; having to support the two in a common
 * API is too annoying at the moment. */
static void RescaleClone(Canvas &canvas,
                         int leftX,
                         int topY,
                         int rightX,
                         int bottomY,
                         const Canvas &from) {
    float scaleFactorX, scaleFactorY;
    int width, height;

    width = rightX - leftX;
    height = bottomY - topY;
    Assert(width >= 0 && height >= 0);

    scaleFactorX = (float)width / (float)from.Width;
    scaleFactorY = (float)height / (float)from.Height;

    if (scaleFactorX == 1 && scaleFactorY == 1) {
        /* Hacky fast path for when we don't need rescaling, speeds up testing
         * quite a lot. */
        CopyRect(canvas, leftX, topY, from, 0, 0, from.Width, from.Height);
        return;
    }

#ifdef _OPENMP
#    pragma omp parallel for
#endif
    for (int toY = 0; toY < height; toY++) {
        const int fromY0 = std::min(int(toY / scaleFactorY), from.Height - 1);
        const int fromY1 = std::min(fromY0 + 1, from.Height - 1);
        const float subOffsY = (toY / scaleFactorY) - fromY0;

        for (int toX = 0; toX < width; toX++) {
            const int fromX0 =
                    std::min(int(toX / scaleFactorX), from.Width - 1);
            const int fromX1 = std::min(fromX0 + 1, from.Width - 1);
            const float subOffsX = (toX / scaleFactorX) - fromX0;

            const Pixel srcPixels[4] = {from.GetPixel(fromX0, fromY0),
                                        from.GetPixel(fromX1, fromY0),
                                        from.GetPixel(fromX0, fromY1),
                                        from.GetPixel(fromX1, fromY1)};
            Pixel &dstPixel = canvas.GetPixel(toX + leftX, toY + topY);

            float outRed, outGreen, outBlue, outAlpha;

            outRed = srcPixels[0].Red * (1 - subOffsX) * (1 - subOffsY);
            outGreen = srcPixels[0].Green * (1 - subOffsX) * (1 - subOffsY);
            outBlue = srcPixels[0].Blue * (1 - subOffsX) * (1 - subOffsY);
            outAlpha = srcPixels[0].Alpha * (1 - subOffsX) * (1 - subOffsY);

            outRed += srcPixels[1].Red * (subOffsX) * (1 - subOffsY);
            outGreen += srcPixels[1].Green * (subOffsX) * (1 - subOffsY);
            outBlue += srcPixels[1].Blue * (subOffsX) * (1 - subOffsY);
            outAlpha += srcPixels[1].Alpha * (subOffsX) * (1 - subOffsY);

            outRed += srcPixels[2].Red * (1 - subOffsX) * (subOffsY);
            outGreen += srcPixels[2].Green * (1 - subOffsX) * (subOffsY);
            outBlue += srcPixels[2].Blue * (1 - subOffsX) * (subOffsY);
            outAlpha += srcPixels[2].Alpha * (1 - subOffsX) * (subOffsY);

            outRed += srcPixels[3].Red * (subOffsX) * (subOffsY);
            outGreen += srcPixels[3].Green * (subOffsX) * (subOffsY);
            outBlue += srcPixels[3].Blue * (subOffsX) * (subOffsY);
            outAlpha += srcPixels[3].Alpha * (subOffsX) * (subOffsY);

            dstPixel.Red = (uint8_t)(outRed + 0.5);
            dstPixel.Green = (uint8_t)(outGreen + 0.5);
            dstPixel.Blue = (uint8_t)(outBlue + 0.5);
            dstPixel.Alpha = (uint8_t)(outAlpha + 0.5);
        }
    }
}

static void ConvertVideo(
        const Settings &settings,
        const std::unique_ptr<Recordings::Recording> &recording,
        Gamestate &&gamestate,
        Encoding::Encoder &encoder,
        Canvas &mapCanvas,
        Canvas &outputCanvas,
        uint32_t frameRate,
        uint32_t startTime,
        uint32_t endTime) {
    const Renderer::Options &renderOptions = settings.RenderOptions;
    int viewLeftX, viewTopY, viewRightX, viewBottomY;
    uint32_t frameNumber = 0, frameTimestamp = 0;

    {
        /* Determine the bounds of the viewport, maintaning the aspect ratio
         * similar to how Tibia does it. */
        float maxX = outputCanvas.Width;
        float maxY = outputCanvas.Height;

        if (!renderOptions.SkipRenderingInventory) {
            maxX -= SIDEBAR_WIDTH;
        }

        if (maxX <= 0 || maxY <= 0) {
            throw InvalidDataError();
        }

        float minScale = std::min(maxX / Renderer::NativeResolutionX,
                                  maxY / Renderer::NativeResolutionY);

        viewLeftX = (maxX - (Renderer::NativeResolutionX * minScale)) / 2;
        viewTopY = (maxY - (Renderer::NativeResolutionY * minScale)) / 2;
        viewRightX = viewLeftX + (Renderer::NativeResolutionX * minScale),
        viewBottomY = viewTopY + (Renderer::NativeResolutionY * minScale);
    }

    auto overlaySlice =
            outputCanvas.Slice(viewLeftX, viewTopY, viewRightX, viewBottomY);

    /* Clip start/end to recording bounds, allowing another second in case of
     * an abrupt end to the recording. */
    startTime = std::min(startTime, recording->Runtime);
    endTime = std::min(endTime, recording->Runtime + 1000);

    auto currentFrame = recording->Frames.cbegin();
    uint32_t nextTimestamp = 0;

    /* Fast-forward until the game state is sufficiently initialized. */
    while (!gamestate.Creatures.contains(gamestate.Player.Id) &&
           currentFrame != recording->Frames.cend()) {
        for (auto &event : currentFrame->Events) {
            event->Update(gamestate);
        }
        currentFrame = std::next(currentFrame);
    }

    if (currentFrame != recording->Frames.cend()) {
        nextTimestamp = currentFrame->Timestamp;
    }

    while (frameTimestamp <= endTime) {
        while (nextTimestamp <= frameTimestamp &&
               currentFrame != recording->Frames.cend()) {
            for (const auto &event : currentFrame->Events) {
                event->Update(gamestate);
            }

            currentFrame = std::next(currentFrame);
        }

        if (currentFrame != recording->Frames.cend()) {
            nextTimestamp = currentFrame->Timestamp;
        }

        do {
            frameNumber++;

            frameTimestamp = (frameNumber * 1000) / frameRate;
            gamestate.CurrentTick = frameTimestamp;

            if ((frameTimestamp < startTime) ||
                (frameNumber % settings.FrameSkip)) {
                continue;
            }

            /* Wipe the background in the same manner Tibia does it, leaving
             * empty spots on the map black. */
            mapCanvas.DrawRectangle(Pixel(0, 0, 0),
                                    0,
                                    0,
                                    Renderer::NativeResolutionX,
                                    Renderer::NativeResolutionY);

            Renderer::DrawClientBackground(gamestate,
                                           outputCanvas,
                                           0,
                                           0,
                                           outputCanvas.Width,
                                           outputCanvas.Height);

            Renderer::DrawGamestate(renderOptions, gamestate, mapCanvas);

            RescaleClone(outputCanvas,
                         viewLeftX,
                         viewTopY,
                         viewRightX,
                         viewBottomY,
                         mapCanvas);

            /* FIXME: C++ migration. */
            gamestate.Messages.Prune(gamestate.CurrentTick);

            Renderer::DrawOverlay(renderOptions, gamestate, overlaySlice);
            DrawInterface(renderOptions, gamestate, outputCanvas);

            encoder.WriteFrame(outputCanvas);

            if ((frameTimestamp % 500) == 0) {
                std::cout << "progress: " << frameTimestamp << " / "
                          << startTime << " / " << endTime << std::endl;
            }
        } while (frameTimestamp <= std::min(nextTimestamp, endTime));
    }

    encoder.Flush();
}

static auto Open(const Settings &settings,
                 const std::filesystem::path &dataFolder,
                 const std::filesystem::path &path,
                 const DataReader &reader) {
    auto inputFormat = settings.InputFormat;
    int major, minor, preview;

    if (inputFormat == Recordings::Format::Unknown) {
        inputFormat = Recordings::GuessFormat(path, reader);
        std::cerr << "warning: Unknown recording format, guessing "
                  << Recordings::FormatName(inputFormat) << std::endl;
    }

    major = settings.DesiredTibiaVersion.Major;
    minor = settings.DesiredTibiaVersion.Minor;
    preview = settings.DesiredTibiaVersion.Preview;

    /* Ask the file for the version if none was provided. */
    if ((major | minor | preview) == 0) {
        if (!Recordings::QueryTibiaVersion(inputFormat,
                                           reader,
                                           major,
                                           minor,
                                           preview)) {
            throw InvalidDataError();
        }

        std::cerr << "warning: Unknown recording version, guessing " << major
                  << "." << minor << "(" << preview << ")" << std::endl;
    }

    const MemoryFile pictures(dataFolder / "Tibia.pic");
    const MemoryFile sprites(dataFolder / "Tibia.spr");
    const MemoryFile types(dataFolder / "Tibia.dat");

    auto version = std::make_unique<Version>(major,
                                             minor,
                                             preview,
                                             pictures.Reader(),
                                             sprites.Reader(),
                                             types.Reader());
    auto [recording, partial] = Recordings::Read(inputFormat,
                                                 reader,
                                                 *version,
                                                 settings.InputRecovery);

    if (partial) {
        throw InvalidDataError();
    }

    return std::make_tuple(std::move(recording), std::move(version));
}

void Export(const Settings &settings,
            const std::filesystem::path &dataFolder,
            const std::filesystem::path &inputPath,
            const std::filesystem::path &outputPath) {
    const MemoryFile file(inputPath);

    auto [recording, version] =
            Open(settings, dataFolder, inputPath, file.Reader());

    Canvas mapCanvas(Renderer::NativeResolutionX, Renderer::NativeResolutionY);
    Canvas outputCanvas(settings.RenderOptions.Width,
                        settings.RenderOptions.Height);

    auto encoder = trc::Encoding::Open(settings.EncodeBackend,
                                       settings.OutputFormat,
                                       settings.OutputEncoding,
                                       settings.EncoderFlags,
                                       outputCanvas.Width,
                                       outputCanvas.Height,
                                       settings.FrameRate,
                                       outputPath);
    ConvertVideo(settings,
                 recording,
                 Gamestate(*version),
                 *encoder,
                 mapCanvas,
                 outputCanvas,
                 settings.FrameRate,
                 settings.StartTime,
                 settings.EndTime);
}
} // namespace Exporter
} // namespace trc
