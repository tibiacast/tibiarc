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

#ifndef __TRC_EXPORTER_HPP__
#define __TRC_EXPORTER_HPP__

#include "encoding.hpp"
#include "renderer.hpp"
#include "recordings.hpp"

namespace trc {
namespace Exporter {
struct Settings {
    struct trc::Renderer::Options RenderOptions;
    int FrameRate;
    int FrameSkip;
    int StartTime;
    int EndTime;

    trc::Recordings::Format InputFormat;
    trc::Encoding::Backend EncodeBackend;
    std::string EncoderFlags;

    std::string OutputFormat;
    std::string OutputEncoding;

    struct {
        int Major, Minor, Preview;
    } DesiredTibiaVersion;
};

void Export(const Settings &settings,
            const std::string &dataFolder,
            const std::string &inputPath,
            const std::string &outputPath);
} // namespace Exporter
} // namespace trc

#endif /* __TRC_EXPORTER_HPP__ */
