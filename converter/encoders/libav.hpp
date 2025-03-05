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

#ifndef __TRC_LIBAV_HPP__
#define __TRC_LIBAV_HPP__

#ifndef DISABLE_LIBAV

#    include "encoding.hpp"

#    include <filesystem>
#    include <functional>
#    include <memory>
#    include <string>

extern "C" {
#    include <libavcodec/avcodec.h>
#    include <libavformat/avformat.h>
#    include <libavutil/imgutils.h>
#    include <libavutil/pixdesc.h>
#    include <libswscale/swscale.h>
}

namespace trc {
namespace Encoding {

struct EncodeError : public ErrorBase {
    EncodeError() : ErrorBase() {
    }
};

class LibAV : public Encoder {
    template <typename T>
    using Wrapper = std::unique_ptr<T, std::function<void(T *)>>;

    Wrapper<AVFormatContext> Format;
    Wrapper<AVCodecContext> Codec;
    Wrapper<AVPacket> Packet;
    Wrapper<AVFrame> Frame;
    Wrapper<struct SwsContext> Transcoder;
    Wrapper<AVFrame> TranscodeFrame;

    /* Owned by format, does not need wrapping. */
    AVStream *Stream = nullptr;
    int64_t PTS = 0;

public:
    LibAV(const std::string &format,
          const std::string &encoding,
          const std::string &flags,
          int width,
          int height,
          int frameRate,
          const std::filesystem::path &path);
    ~LibAV();

    virtual void WriteFrame(const Canvas &frame);
    virtual void Flush();
};
} // namespace Encoding
} // namespace trc

#endif /* DISABLE_LIBAV */

#endif