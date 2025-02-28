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

#include <climits>
#include <iostream>

struct parsed_arguments {
    trc::Exporter::Settings Settings;
    std::string Paths[3];
};

static int parse_arguments(int argc, char **argv, struct parsed_arguments *out);

/* TODO: Turns out that making argp portable requires using autoconf, which is
 * about as fun as a root canal. Despite being more convenient, I now regret
 * picking it over getopt(3) as most users will probably be running Windows. */
#ifdef __GLIBC__
#    include <argp.h>

const char *argp_program_version = "tibiarc-converter 0.2";
const char *argp_program_bug_address = "git@hogberg.online";

enum Arguments {
    /* Setting this to 256 ensures that it won't be mistaken for printable
     * ASCII, and thus won't get a short option name. */
    ARGUMENT_END_TIME = 256,
    ARGUMENT_FRAME_RATE,
    ARGUMENT_FRAME_SKIP,
    ARGUMENT_INPUT_FORMAT,
    ARGUMENT_INPUT_PARTIAL,
    ARGUMENT_INPUT_VERSION,
    ARGUMENT_OUTPUT_BACKEND,
    ARGUMENT_OUTPUT_ENCODING,
    ARGUMENT_OUTPUT_FLAGS,
    ARGUMENT_OUTPUT_FORMAT,
    ARGUMENT_QUALITY_LEVEL,
    ARGUMENT_RESOLUTION,
    ARGUMENT_START_TIME,

    ARGUMENT_SKIP_RENDERING_CREATURE_HEALTH_BARS,
    ARGUMENT_SKIP_RENDERING_CREATURE_ICONS,
    ARGUMENT_SKIP_RENDERING_NON_PLAYER_NAMES,
    ARGUMENT_SKIP_RENDERING_CREATURES,
    ARGUMENT_SKIP_RENDERING_GRAPHICAL_EFFECTS,
    ARGUMENT_SKIP_RENDERING_HOTKEY_MESSAGES,
    ARGUMENT_SKIP_RENDERING_ICON_BAR,
    ARGUMENT_SKIP_RENDERING_INVENTORY,
    ARGUMENT_SKIP_RENDERING_ITEMS,
    ARGUMENT_SKIP_RENDERING_LOOT_MESSAGES,
    ARGUMENT_SKIP_RENDERING_MESSAGES,
    ARGUMENT_SKIP_RENDERING_MISSILES,
    ARGUMENT_SKIP_RENDERING_NUMERICAL_EFFECTS,
    ARGUMENT_SKIP_RENDERING_PLAYER_NAMES,
    ARGUMENT_SKIP_RENDERING_PRIVATE_MESSAGES,
    ARGUMENT_SKIP_RENDERING_STATUS_MESSAGES,
    ARGUMENT_SKIP_RENDERING_SPELL_MESSAGES,
    ARGUMENT_SKIP_RENDERING_STATUS_BARS,
    ARGUMENT_SKIP_RENDERING_UPPER_FLOORS,
    ARGUMENT_SKIP_RENDERING_YELLING_MESSAGES,
};

static struct argp_option options[] = {
        {"end-time",
         ARGUMENT_END_TIME,
         "end_ms",
         0,
         "when to stop encoding, in milliseconds relative to start",
         1},
        {"start-time",
         ARGUMENT_START_TIME,
         "start_ms",
         0,
         "when to start encoding, in milliseconds relative to start",
         1},

        {"frame-rate",
         ARGUMENT_FRAME_RATE,
         "frames_per_second",
         0,
         "the desired frame rate",
         2},
        {"frame-skip",
         ARGUMENT_FRAME_SKIP,
         "frames_to_skip",
         0,
         "only encode one of every 'X' frames",
         2},
        {"resolution",
         ARGUMENT_RESOLUTION,
         "resolution",
         0,
         "the game render resolution, excluding the sidebar of 160 pixels "
         "(for best results use a 15:11 aspect ratio)",
         2},

        {"input-format",
         ARGUMENT_INPUT_FORMAT,
         "format",
         0,
         "the format of the recording, 'cam', 'rec', 'tibiacast', 'tmv1', "
         "'tmv2', 'trp', or 'yatc'",
         3},
        {"input-partial",
         ARGUMENT_INPUT_PARTIAL,
         NULL,
         0,
         "treats the recording as if it ends normally at the first sign of "
         "corruption, instead of erroring out. If --end-time is specified, "
         "error out if the end time cannot be reached.",
         3},
        {"input-version",
         ARGUMENT_INPUT_VERSION,
         "tibia_version",
         0,
         "the Tibia version of the recording, in case the automatic "
         "detection doesn't work",
         3},

        {"output-backend",
         ARGUMENT_OUTPUT_BACKEND,
         "backend",
         0,
         "which output back-end to use, defaults to 'libav'.",
         4},
        {"output-encoding",
         ARGUMENT_OUTPUT_ENCODING,
         "encoding",
         0,
         "the encoding of the converted video",
         4},
        {"output-flags",
         ARGUMENT_OUTPUT_FLAGS,
         "ffmpeg_flags",
         0,
         "extra flags passed to the encoder, see ffmpeg documentation for "
         "more details",
         4},
        {"output-format",
         ARGUMENT_OUTPUT_FORMAT,
         "format",
         0,
         "the video format to convert to",
         4},

        {"skip-rendering-creature-health-bars",
         ARGUMENT_SKIP_RENDERING_CREATURE_HEALTH_BARS,
         NULL,
         0,
         "removes health bars above creatures",
         5},
        {"skip-rendering-creature-icons",
         ARGUMENT_SKIP_RENDERING_CREATURE_ICONS,
         NULL,
         0,
         "removes skulls, party symbols, et cetera",
         5},
        {"skip-rendering-creature-names",
         ARGUMENT_SKIP_RENDERING_NON_PLAYER_NAMES,
         NULL,
         0,
         "removes creature names, specifically",
         5},
        {"skip-rendering-creatures",
         ARGUMENT_SKIP_RENDERING_CREATURES,
         NULL,
         0,
         "removes creatures altogether",
         5},
        {"skip-rendering-graphical-effects",
         ARGUMENT_SKIP_RENDERING_GRAPHICAL_EFFECTS,
         NULL,
         0,
         "removes graphical effects like the explosion from a rune",
         5},
        {"skip-rendering-hotkey-messages",
         ARGUMENT_SKIP_RENDERING_HOTKEY_MESSAGES,
         NULL,
         0,
         "removes 'using one of ...' messages, specifically",
         5},
        {"skip-rendering-icon-bar",
         ARGUMENT_SKIP_RENDERING_ICON_BAR,
         NULL,
         0,
         "removes the player icon bar (PZ block et cetera)",
         5},
        {"skip-rendering-inventory",
         ARGUMENT_SKIP_RENDERING_INVENTORY,
         NULL,
         0,
         "removes the inventory sidebar",
         5},
        {"skip-rendering-items",
         ARGUMENT_SKIP_RENDERING_ITEMS,
         NULL,
         0,
         "removes items, except ground",
         5},
        {"skip-rendering-loot-messages",
         ARGUMENT_SKIP_RENDERING_LOOT_MESSAGES,
         NULL,
         0,
         "removes loot messages, specifically",
         5},
        {"skip-rendering-messages",
         ARGUMENT_SKIP_RENDERING_MESSAGES,
         NULL,
         0,
         "removes all messages",
         5},
        {"skip-rendering-missiles",
         ARGUMENT_SKIP_RENDERING_MISSILES,
         NULL,
         0,
         "removes missiles (runes, arrows, et cetera)",
         5},
        {"skip-rendering-numerical-effects",
         ARGUMENT_SKIP_RENDERING_NUMERICAL_EFFECTS,
         NULL,
         0,
         "removes the numbers that pop up when doing damage, experience "
         "gained, et cetera",
         5},
        {"skip-rendering-player-names",
         ARGUMENT_SKIP_RENDERING_PLAYER_NAMES,
         NULL,
         0,
         "removes player names, distinct from creature names",
         5},
        {"skip-rendering-private-messages",
         ARGUMENT_SKIP_RENDERING_PRIVATE_MESSAGES,
         NULL,
         0,
         "removes private messages, specifically",
         5},
        {"skip-rendering-status-messages",
         ARGUMENT_SKIP_RENDERING_STATUS_MESSAGES,
         NULL,
         0,
         "removes status messages, specifically",
         5},
        {"skip-rendering-spell-messages",
         ARGUMENT_SKIP_RENDERING_SPELL_MESSAGES,
         NULL,
         0,
         "removes spell messages, specifically",
         5},
        {"skip-rendering-status-bars",
         ARGUMENT_SKIP_RENDERING_STATUS_BARS,
         NULL,
         0,
         "removes the mana/health bars",
         5},
        {"skip-rendering-upper-floors",
         ARGUMENT_SKIP_RENDERING_UPPER_FLOORS,
         NULL,
         0,
         "only draw the current floor and those below",
         5},
        {"skip-rendering-yelling-messages",
         ARGUMENT_SKIP_RENDERING_YELLING_MESSAGES,
         NULL,
         0,
         "removes yelling, specifically",
         5},

        {"input-version",
         ARGUMENT_INPUT_VERSION,
         "tibia_version",
         0,
         "the Tibia version of the recording, in case the automatic "
         "detection doesn't work",
         3},

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
        unsigned max_args;

        switch (settings.EncodeBackend) {
        case trc::Encoding::Backend::Inert:
        case trc::Encoding::Backend::LibAV:
            max_args = 3;
            break;
        }

        if (state->arg_num >= max_args) {
            /* Too many args */
            argp_usage(state);
        }

        args->Paths[state->arg_num] = arg;
        break;
    }
    case ARGP_KEY_END: {
        unsigned min_args;

        switch (settings.EncodeBackend) {
        case trc::Encoding::Backend::Inert:
            min_args = 2;
            break;
        case trc::Encoding::Backend::LibAV:
            min_args = 3;
            break;
        }

        if (state->arg_num < min_args) {
            /* Too few args */
            argp_usage(state);
        }

        break;
    }
    case ARGP_KEY_SUCCESS:
        break;
    case ARGP_KEY_ERROR:
        break;
    case ARGUMENT_START_TIME:
        /* start-time */
        if (sscanf(arg, "%i", &settings.StartTime) != 1 ||
            settings.StartTime < 0) {
            argp_error(state, "start-time must be a time in milliseconds");
        }
        break;
    case ARGUMENT_END_TIME:
        /* end-time */
        if (sscanf(arg, "%i", &settings.EndTime) != 1 || settings.EndTime < 0) {
            argp_error(state, "end-time must be a time in milliseconds");
        }
        break;
    case ARGUMENT_RESOLUTION:
        /* resolution */
        if (sscanf(arg,
                   "%ix%i",
                   &settings.RenderOptions.Width,
                   &settings.RenderOptions.Height) != 2) {
            argp_error(state,
                       "resolution must be written as 'WidthxHeight', e.g. "
                       "'480x352'");
        } else if ((settings.RenderOptions.Width < 32) ||
                   (settings.RenderOptions.Height < 32)) {
            argp_error(state, "resolution cannot be unreasonably small");
        } else if (settings.RenderOptions.Width > (1 << 15) ||
                   (settings.RenderOptions.Height > (1 << 15))) {
            argp_error(state, "resolution cannot be unreasonably large");
        }

        break;
    case ARGUMENT_FRAME_RATE:
        /* frame-rate */
        if (sscanf(arg, "%i", &settings.FrameRate) != 1 ||
            settings.FrameRate < 1) {
            argp_error(state, "frame-rate must be a positive integer");
        }
        break;
    case ARGUMENT_FRAME_SKIP:
        /* frame-skip */
        if (sscanf(arg, "%i", &settings.FrameSkip) != 1 ||
            settings.FrameSkip < 1) {
            argp_error(state, "frame-skip must be a positive integer");
        }
        break;
    case ARGUMENT_INPUT_FORMAT:
        /* input-format */
        if (!strcmp(arg, "cam")) {
            settings.InputFormat = trc::Recordings::Format::Cam;
        } else if (!strcmp(arg, "rec")) {
            settings.InputFormat = trc::Recordings::Format::Rec;
        } else if (!strcmp(arg, "tibiacast")) {
            settings.InputFormat = trc::Recordings::Format::Tibiacast;
        } else if (!strcmp(arg, "tmv1")) {
            settings.InputFormat = trc::Recordings::Format::TibiaMovie1;
        } else if (!strcmp(arg, "tmv2")) {
            settings.InputFormat = trc::Recordings::Format::TibiaMovie2;
        } else if (!strcmp(arg, "trp")) {
            settings.InputFormat = trc::Recordings::Format::TibiaReplay;
        } else if (!strcmp(arg, "ttm")) {
            settings.InputFormat = trc::Recordings::Format::TibiaTimeMachine;
        } else if (!strcmp(arg, "yatc")) {
            settings.InputFormat = trc::Recordings::Format::YATC;
        } else {
            argp_error(state,
                       "input-format must be 'cam', 'rec', 'tibiacast', "
                       "'tmv1', 'tmv2', 'trp', 'ttm', or 'yatc'");
        }

        break;
    case ARGUMENT_INPUT_PARTIAL:
        /* input-partial */
        settings.InputRecovery = trc::Recordings::Recovery::PartialReturn;
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
    case ARGUMENT_OUTPUT_BACKEND:
        /* output-backend */
        if (!strcmp(arg, "libav")) {
#    ifndef DISABLE_LIBAV
            settings.EncodeBackend = trc::Encoding::Backend::LibAV;
#    else
            argp_error(state, "'libav' backend has been explicitly disabled");
#    endif
        } else if (!strcmp(arg, "inert")) {
            settings.EncodeBackend = trc::Encoding::Backend::Inert;
        } else {
            /* Deliberately don't mention 'inert', it's only for debugging. */
            argp_error(state, "output-backend must be 'libav'");
        }

        break;
    case ARGUMENT_OUTPUT_ENCODING:
        /* output-encoding */
        settings.OutputEncoding = arg;
        break;
    case ARGUMENT_OUTPUT_FLAGS:
        /* output-flags */
        settings.EncoderFlags = arg;
        break;
    case ARGUMENT_OUTPUT_FORMAT:
        /* output-format */
        settings.OutputFormat = arg;
        break;

    case ARGUMENT_SKIP_RENDERING_CREATURE_HEALTH_BARS:
        /* skip-rendering-creature-health-bars */
        settings.RenderOptions.SkipRenderingCreatureHealthBars = true;
        break;
    case ARGUMENT_SKIP_RENDERING_CREATURE_ICONS:
        /* skip-rendering-creature-icons */
        settings.RenderOptions.SkipRenderingCreatureIcons = true;
        break;
    case ARGUMENT_SKIP_RENDERING_NON_PLAYER_NAMES:
        /* skip-rendering-non-player-names */
        settings.RenderOptions.SkipRenderingNonPlayerNames = true;
        break;
    case ARGUMENT_SKIP_RENDERING_CREATURES:
        /* skip-rendering-creatures */
        settings.RenderOptions.SkipRenderingCreatures = true;
        break;
    case ARGUMENT_SKIP_RENDERING_GRAPHICAL_EFFECTS:
        /* skip-rendering-graphical-effects */
        settings.RenderOptions.SkipRenderingGraphicalEffects = true;
        break;
    case ARGUMENT_SKIP_RENDERING_HOTKEY_MESSAGES:
        /* skip-rendering-hotkey-messages */
        settings.RenderOptions.SkipRenderingHotkeyMessages = true;
        break;
    case ARGUMENT_SKIP_RENDERING_ICON_BAR:
        /* skip-rendering-icon-bar */
        settings.RenderOptions.SkipRenderingIconBar = true;
        break;
    case ARGUMENT_SKIP_RENDERING_INVENTORY:
        /* skip-rendering-inventory */
        settings.RenderOptions.SkipRenderingInventory = true;
        break;
    case ARGUMENT_SKIP_RENDERING_ITEMS:
        /* skip-rendering-items */
        settings.RenderOptions.SkipRenderingItems = true;
        break;
    case ARGUMENT_SKIP_RENDERING_LOOT_MESSAGES:
        /* skip-rendering-loot-messages */
        settings.RenderOptions.SkipRenderingLootMessages = true;
        break;
    case ARGUMENT_SKIP_RENDERING_MESSAGES:
        /* skip-rendering-messages */
        settings.RenderOptions.SkipRenderingMessages = true;
        break;
    case ARGUMENT_SKIP_RENDERING_MISSILES:
        /* skip-rendering-missiles */
        settings.RenderOptions.SkipRenderingMissiles = true;
        break;
    case ARGUMENT_SKIP_RENDERING_NUMERICAL_EFFECTS:
        /* skip-rendering-numerical-effects */
        settings.RenderOptions.SkipRenderingNumericalEffects = true;
        break;
    case ARGUMENT_SKIP_RENDERING_PLAYER_NAMES:
        /* skip-rendering-player-names */
        settings.RenderOptions.SkipRenderingPlayerNames = true;
        break;
    case ARGUMENT_SKIP_RENDERING_PRIVATE_MESSAGES:
        /* skip-rendering-private-messages */
        settings.RenderOptions.SkipRenderingPrivateMessages = true;
        break;
    case ARGUMENT_SKIP_RENDERING_STATUS_MESSAGES:
        /* skip-rendering-status-messages */
        settings.RenderOptions.SkipRenderingStatusMessages = true;
        break;
    case ARGUMENT_SKIP_RENDERING_SPELL_MESSAGES:
        /* skip-rendering-spell-messages */
        settings.RenderOptions.SkipRenderingSpellMessages = true;
        break;
    case ARGUMENT_SKIP_RENDERING_STATUS_BARS:
        /* skip-rendering-status-bars */
        settings.RenderOptions.SkipRenderingStatusBars = true;
        break;
    case ARGUMENT_SKIP_RENDERING_UPPER_FLOORS:
        /* skip-rendering-upper-floors */
        settings.RenderOptions.SkipRenderingUpperFloors = true;
        break;
    case ARGUMENT_SKIP_RENDERING_YELLING_MESSAGES:
        /* skip-rendering-yelling-messages */
        settings.RenderOptions.SkipRenderingYellingMessages = true;
        break;
    default:
        return ARGP_ERR_UNKNOWN;
    }

    return 0;
}

static struct argp argp = {
        options,
        parse_option,
        "data_folder input_path output_path",
        "tibiarc-converter -- a program for converting Tibia "
        "packet captures to video"};

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

#include "utils.hpp"

int main(int argc, char **argv) {
    /* Set up some sane defaults. */
    struct parsed_arguments parsed = {
            .Settings = {.RenderOptions = {.Width = 640, .Height = 352},

                         .InputFormat = trc::Recordings::Format::Unknown,
                         .InputRecovery = trc::Recordings::Recovery::None,

                         .EncodeBackend = trc::Encoding::Backend::LibAV,

                         .FrameRate = 25,
                         .FrameSkip = 1,

                         .StartTime = 0,
                         .EndTime = std::numeric_limits<int>::max()}};

    if (parse_arguments(argc, argv, &parsed)) {
        return 1;
    }

    try {
        trc::Exporter::Export(parsed.Settings,
                              parsed.Paths[0],
                              parsed.Paths[1],
                              parsed.Paths[2]);
    } catch (const trc::ErrorBase &error) {
        std::cerr << "Unrecoverable error (" << error.Description() << ")"
                  << std::endl;
        return 1;
    }

    return 0;
}
