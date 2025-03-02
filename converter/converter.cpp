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

#include "encoding.hpp"
#include "exporter.hpp"
#include "renderer.hpp"
#include "cli.hpp"

#include <limits>

using namespace trc;

#include "utils.hpp"

int main(int argc, char **argv) {
    Exporter::Settings settings = {
            .RenderOptions = {.Width = 640, .Height = 352},

            .InputFormat = Recordings::Format::Unknown,
            .InputRecovery = Recordings::Recovery::None,

            .EncodeBackend = Encoding::Backend::LibAV,

            .FrameRate = 25,
            .FrameSkip = 1,

            .StartTime = 0,
            .EndTime = std::numeric_limits<int>::max()};

    auto paths = CLI::Process(
            argc,
            argv,
            "tibiarc-converter -- a program for converting Tibia packet "
            "captures to video",
            "tibiarc-converter 0.3",
            {"data_folder", "input_path", "output_path"},
            {
                    {"end-time",
                     {"when to stop encoding, in milliseconds relative to "
                      "start",
                      {"end_ms"},
                      [&](const CLI::Range &args) {
                          if (sscanf(args[0].c_str(),
                                     "%i",
                                     &settings.EndTime) != 1 ||
                              settings.EndTime < 0) {
                              throw "end-time must be a time in milliseconds";
                          }
                      }}},
                    {"start-time",
                     {"when to stop encoding, in milliseconds relative to "
                      "start",
                      {"start_ms"},
                      [&](const CLI::Range &args) {
                          if (sscanf(args[0].c_str(),
                                     "%i",
                                     &settings.StartTime) != 1 ||
                              settings.StartTime < 0) {
                              throw "start-time must be a time in milliseconds";
                          }
                      }}},

                    {"frame-rate",
                     {"the desired frame rate",
                      {"frames_per_second"},
                      [&](const CLI::Range &args) {
                          if (sscanf(args[0].c_str(),
                                     "%i",
                                     &settings.FrameRate) != 1 ||
                              settings.FrameRate < 1) {
                              throw "frame-rate must be a positive integer";
                          }
                      }}},
                    {"frame-skip",
                     {"only encode one of every 'X' frames",
                      {"frames_to_skip"},
                      [&](const CLI::Range &args) {
                          if (sscanf(args[0].c_str(),
                                     "%i",
                                     &settings.FrameSkip) != 1 ||
                              settings.FrameSkip < 1) {
                              throw "frame-skip must be a positive integer";
                          }
                      }}},

                    {"resolution",
                     {"the game render resolution, excluding the sidebar of "
                      "160 pixels (for best results use a 15:11 aspect ratio)",
                      {"width", "height"},
                      [&](const CLI::Range &args) {
                          const auto &width = args[0], &height = args[1];
                          if (sscanf(width.c_str(),
                                     "%i",
                                     &settings.RenderOptions.Width) != 1 ||
                              settings.RenderOptions.Width < 32 ||
                              settings.RenderOptions.Width > (1 << 15)) {

                              throw "resolution width must be an integer "
                                    "between 32 and 32768";
                          }

                          if (sscanf(height.c_str(),
                                     "%i",
                                     &settings.RenderOptions.Height) != 1 ||
                              settings.RenderOptions.Height < 32 ||
                              settings.RenderOptions.Height > (1 << 15)) {

                              throw "resolution height must be an integer "
                                    "between 32 and 32768";
                          }
                      }}},

                    {"input-format",
                     {"the format of the recording, 'cam', 'rec', 'tibiacast', "
                      "'tmv1', 'tmv2', 'trp', or 'yatc'.",
                      {"format"},
                      [&](const CLI::Range &args) {
                          const auto &format = args[0];

                          if (format == "cam") {
                              settings.InputFormat = Recordings::Format::Cam;
                          } else if (format == "rec") {
                              settings.InputFormat = Recordings::Format::Rec;
                          } else if (format == "tibiacast") {
                              settings.InputFormat =
                                      Recordings::Format::Tibiacast;
                          } else if (format == "tmv1") {
                              settings.InputFormat =
                                      Recordings::Format::TibiaMovie1;
                          } else if (format == "tmv2") {
                              settings.InputFormat =
                                      Recordings::Format::TibiaMovie2;
                          } else if (format == "trp") {
                              settings.InputFormat =
                                      Recordings::Format::TibiaReplay;
                          } else if (format == "ttm") {
                              settings.InputFormat =
                                      Recordings::Format::TibiaTimeMachine;
                          } else if (format == "yatc") {
                              settings.InputFormat = Recordings::Format::YATC;
                          } else {
                              throw "input-format must be 'cam', 'rec', "
                                    "'tibiacast', 'tmv1', 'tmv2', 'trp', "
                                    "'ttm', "
                                    "or 'yatc'";
                          }
                      }}},
                    {"input-partial",
                     {"treats the recording as if it ends normally at "
                      "the first sign of corruption, instead of "
                      "erroring out. If --end-time is specified, error "
                      "out if the end time cannot be reached.",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.InputRecovery =
                                  Recordings::Recovery::PartialReturn;
                      }}},
                    {"input-version",
                     {"the Tibia version of the recording, in case the "
                      "automatic detection doesn't work",
                      {"tibia_version"},
                      [&](const CLI::Range &args) {
                          if (sscanf(args[0].c_str(),
                                     "%u.%u.%u",
                                     &settings.DesiredTibiaVersion.Major,
                                     &settings.DesiredTibiaVersion.Minor,
                                     &settings.DesiredTibiaVersion.Preview) <
                              2) {
                              throw "input-version must be in the format "
                                    "'X.Y', e.g. '8.55'";
                          }
                      }}},

                    {"output-backend",
                     {"which output back-end to use, defaults to 'libav'.",
                      {"backend"},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          if (args[0] == "libav") {
#ifndef DISABLE_LIBAV
                              settings.EncodeBackend =
                                      trc::Encoding::Backend::LibAV;
#else
                              throw "'libav' backend has been explicitly "
                                    "disabled";
#endif
                              return;
                          } else if (args[0] == "inert") {
                              settings.EncodeBackend =
                                      trc::Encoding::Backend::Inert;
                              return;
                          }

                          /* Deliberately don't mention 'inert', it's only for
                           * debugging. */
                          throw "output-backend must be 'libav'";
                      }}},
                    {"output-encoding",
                     {"the encoding of the converted video",
                      {"encoding"},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.OutputEncoding = args[0];
                      }}},
                    {"output-flags",
                     {"extra flags passed to the encoder, see ffmpeg "
                      "documentation for more details",
                      {"flags"},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.EncoderFlags = args[0];
                      }}},
                    {"output-format",
                     {"the video format to convert to",
                      {"format"},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.OutputFormat = args[0];
                      }}},

                    {"skip-rendering-creature-health-bars",
                     {"removes health bars above creatures",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.RenderOptions
                                  .SkipRenderingCreatureHealthBars = true;
                      }}},
                    {"skip-rendering-creature-icons",
                     {"removes skulls, party symbols, et cetera",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.RenderOptions.SkipRenderingCreatureIcons =
                                  true;
                      }}},
                    {"skip-rendering-creature-names",
                     {"removes creature names, specifically",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.RenderOptions.SkipRenderingNonPlayerNames =
                                  true;
                      }}},
                    {"skip-rendering-creatures",
                     {"removes creatures altogether",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.RenderOptions.SkipRenderingCreatures = true;
                      }}},
                    {"skip-rendering-graphical-effects",
                     {"removes graphical effects like the explosion from a "
                      "rune",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.RenderOptions.SkipRenderingGraphicalEffects =
                                  true;
                      }}},
                    {"skip-rendering-hotkey-messages",
                     {"removes 'using one of ...' messages, specifically",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.RenderOptions.SkipRenderingHotkeyMessages =
                                  true;
                      }}},
                    {"skip-rendering-icon-bar",
                     {"removes the player icon bar (PZ block et cetera)",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.RenderOptions.SkipRenderingIconBar = true;
                      }}},
                    {"skip-rendering-inventory",
                     {"removes the inventory sidebar",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.RenderOptions.SkipRenderingInventory = true;
                      }}},
                    {"skip-rendering-items",
                     {"removes items, except ground",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.RenderOptions.SkipRenderingItems = true;
                      }}},
                    {"skip-rendering-loot-messages",
                     {"removes loot messages, specifically",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.RenderOptions.SkipRenderingLootMessages =
                                  true;
                      }}},
                    {"skip-rendering-messages",
                     {"removes all messages",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.RenderOptions.SkipRenderingMessages = true;
                      }}},
                    {"skip-rendering-missiles",
                     {"removes missiles (runes, arrows, et cetera)",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.RenderOptions.SkipRenderingMissiles = true;
                      }}},
                    {"skip-rendering-numerical-effects",
                     {"removes the numbers that pop up when doing damage, "
                      "experience gained, et cetera",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.RenderOptions.SkipRenderingNumericalEffects =
                                  true;
                      }}},
                    {"skip-rendering-player-names",
                     {"removes player names, distinct from creature names",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.RenderOptions.SkipRenderingPlayerNames =
                                  true;
                      }}},
                    {"skip-rendering-private-messages",
                     {"removes private messages, specifically",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.RenderOptions.SkipRenderingPrivateMessages =
                                  true;
                      }}},
                    {"skip-rendering-status-messages",
                     {"removes status messages, specifically",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.RenderOptions.SkipRenderingStatusMessages =
                                  true;
                      }}},
                    {"skip-rendering-spell-messages",
                     {"removes spell messages, specifically",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.RenderOptions.SkipRenderingSpellMessages =
                                  true;
                      }}},
                    {"skip-rendering-status-bars",
                     {"removes the mana/health bars",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.RenderOptions.SkipRenderingStatusBars = true;
                      }}},
                    {"skip-rendering-upper-floors",
                     {"only draw the current floor and those below",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.RenderOptions.SkipRenderingUpperFloors =
                                  true;
                      }}},
                    {"skip-rendering-yelling-messages",
                     {"removes yelling, specifically",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.RenderOptions.SkipRenderingYellingMessages =
                                  true;
                      }}},
            });

    try {
        trc::Exporter::Export(settings, paths[0], paths[1], paths[2]);
    } catch (const trc::ErrorBase &error) {
        std::cerr << "Unrecoverable error (" << error.Description() << ")"
                  << std::endl;
        return 1;
    }

    return 0;
}
