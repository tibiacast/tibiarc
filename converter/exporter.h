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

#ifndef __TRC_EXPORTER_H__
#define __TRC_EXPORTER_H__

#include "encoding.h"
#include "recordings.h"
#include "renderer.h"

struct export_settings {
    struct trc_render_options RenderOptions;
    int FrameRate;
    int FrameSkip;
    int StartTime;
    int EndTime;

    enum TrcRecordingFormat InputFormat;
    enum TrcEncodeLibrary EncodeLibrary;
    const char *EncoderFlags;

    const char *OutputFormat;
    const char *OutputEncoding;

    struct {
        int Major, Minor, Preview;
    } DesiredTibiaVersion;
};

bool exporter_Export(const struct export_settings *settings,
                     const char *dataFolder,
                     const char *inputPath,
                     const char *outputPath);

#endif /* __TRC_EXPORTER_H__ */
