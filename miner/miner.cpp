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

#include <unordered_set>
#include <climits>

using namespace trc;

struct parsed_arguments {
    struct Serializer::Settings Settings;
    std::string Paths[3];
};

static int parse_arguments(int argc, char **argv, struct parsed_arguments *out);

/* TODO: Turns out that making argp portable requires using autoconf, which is
 * about as fun as a root canal. Despite being more convenient, I now regret
 * picking it over getopt(3) as most users will probably be running Windows. */
#ifdef __GLIBC__
#    include <argp.h>

const char *argp_program_version = "tibiarc-miner 0.2";
const char *argp_program_bug_address = "git@hogberg.online";

enum Arguments {
    /* Setting this to 256 ensures that it won't be mistaken for printable
     * ASCII, and thus won't get a short option name. */
    ARGUMENT_OUTPUT_BACKEND = 256,

    ARGUMENT_INPUT_FORMAT,
    ARGUMENT_INPUT_VERSION,

    ARGUMENT_SKIP_CREATURE_UPDATE_EVENTS,
    ARGUMENT_SKIP_EFFECT_EVENTS,
    ARGUMENT_SKIP_INVENTORY_EVENTS,
    ARGUMENT_SKIP_MESSAGE_EVENTS,
    ARGUMENT_SKIP_PLAYER_UPDATE_EVENTS,
    ARGUMENT_SKIP_CREATURE_PRESENCE_EVENTS,
    ARGUMENT_SKIP_TERRAIN_EVENTS,
};

static struct argp_option options[] = {
        {"input-format",
         ARGUMENT_INPUT_FORMAT,
         "format",
         0,
         "the format of the recording, 'cam', 'rec', 'tibiacast', 'tmv1', "
         "'tmv2', 'trp', or 'yatc'.",
         1},
        {"input-version",
         ARGUMENT_INPUT_VERSION,
         "tibia_version",
         0,
         "the Tibia version of the recording, in case the automatic "
         "detection doesn't work",
         1},

        {"skip-creature-presence",
         ARGUMENT_SKIP_CREATURE_PRESENCE_EVENTS,
         NULL,
         0,
         "skips creature presence events",
         5},

        {"skip-creature-updates",
         ARGUMENT_SKIP_CREATURE_UPDATE_EVENTS,
         NULL,
         0,
         "skips creature update events (e.g. movement, health)",
         5},

        {"skip-effects",
         ARGUMENT_SKIP_EFFECT_EVENTS,
         NULL,
         0,
         "skips effect events (e.g. missiles, poofs)",
         5},

        {"skip-inventory",
         ARGUMENT_SKIP_INVENTORY_EVENTS,
         NULL,
         0,
         "skips inventory events (e.g. containers)",
         5},

        {"skip-messages",
         ARGUMENT_SKIP_MESSAGE_EVENTS,
         NULL,
         0,
         "skips message events",
         5},

        {"skip-player-updates",
         ARGUMENT_SKIP_PLAYER_UPDATE_EVENTS,
         NULL,
         0,
         "skips player update events (e.g. movement, skills)",
         5},

        {"skip-terrain",
         ARGUMENT_SKIP_TERRAIN_EVENTS,
         NULL,
         0,
         "skips terrain events",
         5},

        {0}};

static error_t parse_option(int key, char *arg, struct argp_state *state) {
    auto args = (struct parsed_arguments *)state->input;
    auto &settings = args->Settings;

    switch (key) {
    case ARGP_KEY_INIT:
        /* Defaults have already been set up in `main`. */
        break;
    case ARGP_KEY_FINI:
        break;
    case ARGP_KEY_ARG: {
        if (state->arg_num >= 2) {
            /* Too many args */
            argp_usage(state);
        }

        args->Paths[state->arg_num] = arg;
        break;
    }
    case ARGP_KEY_END: {
        if (state->arg_num < 2) {
            /* Too few args */
            argp_usage(state);
        }

        break;
    }
    case ARGP_KEY_SUCCESS:
        break;
    case ARGP_KEY_ERROR:
        break;
    case ARGUMENT_INPUT_FORMAT:
        /* input-format */
        if (!strcmp(arg, "cam")) {
            settings.InputFormat = Recordings::Format::Cam;
        } else if (!strcmp(arg, "rec")) {
            settings.InputFormat = Recordings::Format::Rec;
        } else if (!strcmp(arg, "tibiacast")) {
            settings.InputFormat = Recordings::Format::Tibiacast;
        } else if (!strcmp(arg, "tmv1")) {
            settings.InputFormat = Recordings::Format::TibiaMovie1;
        } else if (!strcmp(arg, "tmv2")) {
            settings.InputFormat = Recordings::Format::TibiaMovie2;
        } else if (!strcmp(arg, "trp")) {
            settings.InputFormat = Recordings::Format::TibiaReplay;
        } else if (!strcmp(arg, "ttm")) {
            settings.InputFormat = Recordings::Format::TibiaTimeMachine;
        } else if (!strcmp(arg, "yatc")) {
            settings.InputFormat = Recordings::Format::YATC;
        } else {
            argp_error(state,
                       "input-format must be 'cam', 'rec', 'tibiacast', "
                       "'tmv1', 'tmv2', 'trp', 'ttm', or 'yatc'");
        }

        break;
    case ARGUMENT_INPUT_VERSION:
        /* input-version */
        if (sscanf(arg,
                   "%u.%u.%u",
                   &settings.DesiredTibiaVersion.Major,
                   &settings.DesiredTibiaVersion.Minor,
                   &settings.DesiredTibiaVersion.Preview) < 2) {
            argp_error(state,
                       "input-version must be in the format 'X.Y', e.g. "
                       "'8.55'");
        }
        break;
    case ARGUMENT_SKIP_CREATURE_PRESENCE_EVENTS:
        settings.SkippedEvents.insert(trc::Events::Type::CreatureSeen);
        settings.SkippedEvents.insert(trc::Events::Type::CreatureRemoved);
        break;
    case ARGUMENT_SKIP_CREATURE_UPDATE_EVENTS:
        settings.SkippedEvents.insert(
                trc::Events::Type::CreatureGuildMembersUpdated);
        settings.SkippedEvents.insert(
                trc::Events::Type::CreatureHeadingUpdated);
        settings.SkippedEvents.insert(trc::Events::Type::CreatureHealthUpdated);
        settings.SkippedEvents.insert(
                trc::Events::Type::CreatureImpassableUpdated);
        settings.SkippedEvents.insert(trc::Events::Type::CreatureLightUpdated);
        settings.SkippedEvents.insert(trc::Events::Type::CreatureMoved);
        settings.SkippedEvents.insert(
                trc::Events::Type::CreatureNPCCategoryUpdated);
        settings.SkippedEvents.insert(trc::Events::Type::CreatureOutfitUpdated);
        settings.SkippedEvents.insert(
                trc::Events::Type::CreaturePvPHelpersUpdated);
        settings.SkippedEvents.insert(trc::Events::Type::CreatureShieldUpdated);
        settings.SkippedEvents.insert(trc::Events::Type::CreatureSkullUpdated);
        settings.SkippedEvents.insert(trc::Events::Type::CreatureSpeedUpdated);
        break;
    case ARGUMENT_SKIP_EFFECT_EVENTS:
        settings.SkippedEvents.insert(trc::Events::Type::GraphicalEffectPopped);
        settings.SkippedEvents.insert(trc::Events::Type::NumberEffectPopped);
        settings.SkippedEvents.insert(trc::Events::Type::MissileFired);
        break;
    case ARGUMENT_SKIP_INVENTORY_EVENTS:
        settings.SkippedEvents.insert(trc::Events::Type::ContainerAddedItem);
        settings.SkippedEvents.insert(trc::Events::Type::ContainerClosed);
        settings.SkippedEvents.insert(trc::Events::Type::ContainerOpened);
        settings.SkippedEvents.insert(trc::Events::Type::ContainerRemovedItem);
        settings.SkippedEvents.insert(
                trc::Events::Type::ContainerTransformedItem);
        settings.SkippedEvents.insert(
                trc::Events::Type::PlayerInventoryUpdated);
        break;
    case ARGUMENT_SKIP_MESSAGE_EVENTS:
        settings.SkippedEvents.insert(trc::Events::Type::ChannelClosed);
        settings.SkippedEvents.insert(trc::Events::Type::ChannelListUpdated);
        settings.SkippedEvents.insert(trc::Events::Type::ChannelOpened);
        settings.SkippedEvents.insert(trc::Events::Type::CreatureSpoke);
        settings.SkippedEvents.insert(
                trc::Events::Type::CreatureSpokeInChannel);
        settings.SkippedEvents.insert(trc::Events::Type::CreatureSpokeOnMap);
        settings.SkippedEvents.insert(trc::Events::Type::StatusMessageReceived);
        settings.SkippedEvents.insert(
                trc::Events::Type::StatusMessageReceivedInChannel);
        break;
    case ARGUMENT_SKIP_PLAYER_UPDATE_EVENTS:
        settings.SkippedEvents.insert(
                trc::Events::Type::PlayerBlessingsUpdated);
        settings.SkippedEvents.insert(
                trc::Events::Type::PlayerDataBasicUpdated);
        settings.SkippedEvents.insert(trc::Events::Type::PlayerDataUpdated);
        settings.SkippedEvents.insert(
                trc::Events::Type::PlayerHotkeyPresetUpdated);
        settings.SkippedEvents.insert(trc::Events::Type::PlayerMoved);
        settings.SkippedEvents.insert(trc::Events::Type::PlayerSkillsUpdated);
        break;
    case ARGUMENT_SKIP_TERRAIN_EVENTS:
        settings.SkippedEvents.insert(trc::Events::Type::TileUpdated);
        settings.SkippedEvents.insert(trc::Events::Type::TileObjectAdded);
        settings.SkippedEvents.insert(trc::Events::Type::TileObjectRemoved);
        settings.SkippedEvents.insert(trc::Events::Type::TileObjectTransformed);
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

static struct argp argp = {options,
                           parse_option,
                           "data_folder input_path output_path",
                           "tibiarc-miner -- a program for converting Tibia "
                           "packet captures to JSON"};

static int parse_arguments(int argc,
                           char **argv,
                           struct parsed_arguments *out) {
    if (argp_parse(&argp, argc, argv, 0, NULL, out) != 0) {
        return 1;
    }

    return 0;
}
#else /* !defined(__GLIBC__)*/
static int parse_arguments(int argc,
                           char **argv,
                           struct parsed_arguments *out) {
    if (argc == 4) {
        out->Paths[0] = argv[1];
        out->Paths[1] = argv[2];
        out->Paths[2] = argv[3];
        return 0;
    }

    return 1;
}
#endif

int main(int argc, char **argv) {
    /* Set up some sane defaults. */
    struct parsed_arguments parsed = {
            .Settings = {.InputFormat = trc::Recordings::Format::Unknown}};

    if (parse_arguments(argc, argv, &parsed)) {
        return 1;
    }

    try {
        trc::Serializer::Serialize(parsed.Settings,
                                   parsed.Paths[0],
                                   parsed.Paths[1],
                                   std::cout);
    } catch (const trc::ErrorBase &error) {
        std::cerr << "Unrecoverable error (" << error.Description() << ") at:\n"
                  << error.Stacktrace << std::endl;
        return 1;
    }

    return 0;
}
