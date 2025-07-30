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
    Serializer::Settings settings = {
            .InputFormat = Recordings::Format::Unknown,
            .InputRecovery = Recordings::Recovery::None,

            .StartTime = std::chrono::milliseconds::zero(),
            .EndTime = std::chrono::milliseconds::max(),

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
                          uint32_t time;
                          if (sscanf(args[0].c_str(), "%i", &time) != 1 ||
                              time < 0) {
                              throw "end-time must be a time in milliseconds";
                          }

                          settings.EndTime = std::chrono::milliseconds(time);
                      }}},
                    {"start-time",
                     {"when to start encoding, in milliseconds relative "
                      "to start",
                      {"start_ms"},
                      [&](const CLI::Range &args) {
                          uint32_t time;
                          if (sscanf(args[0].c_str(), "%i", &time) != 1 ||
                              time < 0) {
                              throw "start-time must be a time in milliseconds";
                          }

                          settings.StartTime = std::chrono::milliseconds(time);
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
                                  Events::Type::CreatureSeen);
                          settings.SkippedEvents.insert(
                                  Events::Type::CreatureRemoved);
                      }}},
                    {"skip-creature-updates",
                     {"skips creature update events (e.g. movement, health)",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.SkippedEvents.insert(
                                  Events::Type::CreatureGuildMembersUpdated);
                          settings.SkippedEvents.insert(
                                  Events::Type::CreatureHeadingUpdated);
                          settings.SkippedEvents.insert(
                                  Events::Type::CreatureHealthUpdated);
                          settings.SkippedEvents.insert(
                                  Events::Type::CreatureImpassableUpdated);
                          settings.SkippedEvents.insert(
                                  Events::Type::CreatureLightUpdated);
                          settings.SkippedEvents.insert(
                                  Events::Type::CreatureMoved);
                          settings.SkippedEvents.insert(
                                  Events::Type::CreatureNPCCategoryUpdated);
                          settings.SkippedEvents.insert(
                                  Events::Type::CreatureOutfitUpdated);
                          settings.SkippedEvents.insert(
                                  Events::Type::CreaturePvPHelpersUpdated);
                          settings.SkippedEvents.insert(
                                  Events::Type::CreatureShieldUpdated);
                          settings.SkippedEvents.insert(
                                  Events::Type::CreatureSkullUpdated);
                          settings.SkippedEvents.insert(
                                  Events::Type::CreatureSpeedUpdated);
                      }}},
                    {"skip-effects",
                     {"skips effect events (e.g. missiles, poofs)",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.SkippedEvents.insert(
                                  Events::Type::GraphicalEffectPopped);
                          settings.SkippedEvents.insert(
                                  Events::Type::NumberEffectPopped);
                          settings.SkippedEvents.insert(
                                  Events::Type::MissileFired);
                      }}},
                    {"skip-inventory",
                     {"skips inventory events (e.g. containers)",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.SkippedEvents.insert(
                                  Events::Type::ContainerAddedItem);
                          settings.SkippedEvents.insert(
                                  Events::Type::ContainerClosed);
                          settings.SkippedEvents.insert(
                                  Events::Type::ContainerOpened);
                          settings.SkippedEvents.insert(
                                  Events::Type::ContainerRemovedItem);
                          settings.SkippedEvents.insert(
                                  Events::Type::ContainerTransformedItem);
                          settings.SkippedEvents.insert(
                                  Events::Type::PlayerInventoryUpdated);
                      }}},
                    {"skip-messages",
                     {"skips message events",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.SkippedEvents.insert(
                                  Events::Type::ChannelClosed);
                          settings.SkippedEvents.insert(
                                  Events::Type::ChannelListUpdated);
                          settings.SkippedEvents.insert(
                                  Events::Type::ChannelOpened);
                          settings.SkippedEvents.insert(
                                  Events::Type::CreatureSpoke);
                          settings.SkippedEvents.insert(
                                  Events::Type::CreatureSpokeInChannel);
                          settings.SkippedEvents.insert(
                                  Events::Type::CreatureSpokeOnMap);
                          settings.SkippedEvents.insert(
                                  Events::Type::StatusMessageReceived);
                          settings.SkippedEvents.insert(
                                  Events::Type::StatusMessageReceivedInChannel);
                      }}},
                    {"skip-player-updates",
                     {"skips player update events (e.g. movement, skills)",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.SkippedEvents.insert(
                                  Events::Type::PlayerBlessingsUpdated);
                          settings.SkippedEvents.insert(
                                  Events::Type::PlayerDataBasicUpdated);
                          settings.SkippedEvents.insert(
                                  Events::Type::PlayerDataUpdated);
                          settings.SkippedEvents.insert(
                                  Events::Type::PlayerHotkeyPresetUpdated);
                          settings.SkippedEvents.insert(
                                  Events::Type::PlayerMoved);
                          settings.SkippedEvents.insert(
                                  Events::Type::PlayerSkillsUpdated);
                      }}},
                    {"skip-terrain",
                     {"skips terrain events",
                      {},
                      [&]([[maybe_unused]] const CLI::Range &args) {
                          settings.SkippedEvents.insert(
                                  Events::Type::TileUpdated);
                          settings.SkippedEvents.insert(
                                  Events::Type::TileObjectAdded);
                          settings.SkippedEvents.insert(
                                  Events::Type::TileObjectRemoved);
                          settings.SkippedEvents.insert(
                                  Events::Type::TileObjectTransformed);
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
        Serializer::Serialize(settings, paths[0], paths[1], std::cout);
    } catch (const ErrorBase &error) {
        std::cerr << "Unrecoverable error (" << error.Description() << ")"
                  << std::endl;
        return 1;
    }

    return 0;
}
