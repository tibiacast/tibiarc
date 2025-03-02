/*
 * Copyright 2011-2016 "Silver Squirrel Software Handelsbolag"
 * Copyright 2023-2025 "John Högberg"
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

#include "serializer.hpp"
#include "cli.hpp"

#include <unordered_set>
#include <climits>

using namespace trc;

int main(int argc, char **argv) {
    /* Set up some sane defaults. */
    Serializer::Settings settings = {.InputFormat = Recordings::Format::Unknown,
                                     .InputRecovery =
                                             Recordings::Recovery::None,

                                     .StartTime = 0,
                                     .EndTime = std::numeric_limits<int>::max(),

                                     .DryRun = false};

    auto paths = CLI::Process(
            argc,
            argv,
            "tibiarc-miner -- a program for converting Tibia packet captures "
            "to JSON",
            "tibiarc-miner 0.3",
            {"data_folder", "input_path"},
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
                     {"when to start encoding, in milliseconds relative "
                      "to start",
                      {"start_ms"},
                      [&](const CLI::Range &args) {
                          if (sscanf(args[0].c_str(),
                                     "%i",
                                     &settings.StartTime) != 1 ||
                              settings.StartTime < 0) {
                              throw "start-time must be a time in milliseconds";
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

                    {"skip-creature-presence",
                     {"skips creature presence events",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::CreatureSeen);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::CreatureRemoved);
                      }}},
                    {"skip-creature-updates",
                     {"skips creature update events (e.g. movement, health)",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::
                                          CreatureGuildMembersUpdated);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::CreatureHeadingUpdated);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::CreatureHealthUpdated);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::CreatureImpassableUpdated);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::CreatureLightUpdated);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::CreatureMoved);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::
                                          CreatureNPCCategoryUpdated);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::CreatureOutfitUpdated);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::CreaturePvPHelpersUpdated);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::CreatureShieldUpdated);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::CreatureSkullUpdated);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::CreatureSpeedUpdated);
                      }}},
                    {"skip-effects",
                     {"skips effect events (e.g. missiles, poofs)",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::GraphicalEffectPopped);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::NumberEffectPopped);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::MissileFired);
                      }}},
                    {"skip-inventory",
                     {"skips inventory events (e.g. containers)",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::ContainerAddedItem);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::ContainerClosed);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::ContainerOpened);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::ContainerRemovedItem);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::ContainerTransformedItem);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::PlayerInventoryUpdated);
                      }}},
                    {"skip-messages",
                     {"skips message events",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::ChannelClosed);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::ChannelListUpdated);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::ChannelOpened);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::CreatureSpoke);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::CreatureSpokeInChannel);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::CreatureSpokeOnMap);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::StatusMessageReceived);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::
                                          StatusMessageReceivedInChannel);
                      }}},
                    {"skip-player-updates",
                     {"skips player update events (e.g. movement, skills)",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::PlayerBlessingsUpdated);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::PlayerDataBasicUpdated);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::PlayerDataUpdated);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::PlayerHotkeyPresetUpdated);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::PlayerMoved);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::PlayerSkillsUpdated);
                      }}},
                    {"skip-player-terrain",
                     {"skips terrain events",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::TileUpdated);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::TileObjectAdded);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::TileObjectRemoved);
                          settings.SkippedEvents.insert(
                                  trc::Events::Type::TileObjectTransformed);
                      }}},
                    {"dry-run",
                     {"suppress output while still generating it. This is only"
                      "intended for testing",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.DryRun = true;
                      }}},
            });

    try {
        trc::Serializer::Serialize(settings, paths[0], paths[1], std::cout);
    } catch (const trc::ErrorBase &error) {
        std::cerr << "Unrecoverable error (" << error.Description() << ")"
                  << std::endl;
        return 1;
    }

    return 0;
}
