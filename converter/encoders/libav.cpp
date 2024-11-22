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

#ifndef DISABLE_LIBAV

#    include "libav.hpp"

namespace trc {
namespace Encoding {

/* Try to find the best possible image format/range we can get when converting
 * from our native BGR24. */
static void SetBestPixelFormat(const AVCodec *codec, AVCodecContext *context) {
    enum AVPixelFormat best_format = AV_PIX_FMT_NONE;

    for (int i = 0; codec->pix_fmts[i] != AV_PIX_FMT_NONE; i++) {
        enum AVPixelFormat candidate_format = codec->pix_fmts[i];
        int loss;

        best_format = av_find_best_pix_fmt_of_2(best_format,
                                                candidate_format,
                                                AV_PIX_FMT_BGR24,
                                                0,
                                                &loss);
        (void)loss;
    }

    /* Convert deprecated JPEG-specific formats to their corresponding general
     * format with the JPEG color range. */
    switch (best_format) {
    case AV_PIX_FMT_YUVJ420P:
        context->color_range = AVCOL_RANGE_JPEG;
        best_format = AV_PIX_FMT_YUV420P;
        break;
    case AV_PIX_FMT_YUVJ422P:
        context->color_range = AVCOL_RANGE_JPEG;
        best_format = AV_PIX_FMT_YUV422P;
        break;
    case AV_PIX_FMT_YUVJ444P:
        context->color_range = AVCOL_RANGE_JPEG;
        best_format = AV_PIX_FMT_YUV444P;
        break;
    case AV_PIX_FMT_YUVJ440P:
        context->color_range = AVCOL_RANGE_JPEG;
        best_format = AV_PIX_FMT_YUV440P;
        break;
    default:
        break;
    }

    context->pix_fmt = best_format;
    if (best_format == AV_PIX_FMT_NONE) {
        throw NotSupportedError();
    }
}

LibAV::LibAV(const std::string &format,
             const std::string &encoding,
             const std::string &flags,
             int width,
             int height,
             int frameRate,
             const std::string &path) {
    struct AVDictionaryWrapper {
        AVDictionary *Dictionary = nullptr;

        ~AVDictionaryWrapper() {
            av_dict_free(&Dictionary);
        }
    } options;

    int result;

    if (!flags.empty()) {
        result = av_dict_parse_string(&options.Dictionary,
                                      flags.c_str(),
                                      "=",
                                      ":",
                                      0);

        if (result < 0) {
            throw NotSupportedError();
        }
    }

    {
        AVFormatContext *ptr;

        result = avformat_alloc_output_context2(&ptr,
                                                nullptr,
                                                format.empty() ? nullptr
                                                               : format.c_str(),
                                                path.c_str());
        if (result < 0) {
            throw NotSupportedError();
        }

        Format = Wrapper<AVFormatContext>(ptr, [](auto *p) {
            if (!((p->oformat)->flags & AVFMT_NOFILE)) {
                avio_closep(&p->pb);
            }

            avformat_free_context(p);
        });
    }

    const AVOutputFormat *outputFormat = Format->oformat;
    const AVCodec *codec;

    if (encoding.empty()) {
        codec = avcodec_find_encoder(outputFormat->video_codec);
    } else {
        codec = avcodec_find_encoder_by_name(encoding.c_str());
    }

    if (codec == NULL) {
        throw NotSupportedError();
    }

    Codec = Wrapper<AVCodecContext>(avcodec_alloc_context3(codec),
                                    [](auto *p) { avcodec_free_context(&p); });

    Codec->width = width;
    Codec->height = height;

    if (outputFormat->flags & AVFMT_GLOBALHEADER) {
        Codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    SetBestPixelFormat(codec, Codec.get());

    Codec->time_base = AVRational{1, frameRate};

    result = avcodec_open2(Codec.get(), codec, &options.Dictionary);

    if (result < 0) {
        throw NotSupportedError();
    }

    Stream = avformat_new_stream(Format.get(), NULL);
    Stream->id = 0;
    Stream->time_base = Stream->time_base;

    result = avcodec_parameters_from_context(Stream->codecpar, Codec.get());

    if (result < 0) {
        throw NotSupportedError();
    }

    Frame = Wrapper<AVFrame>(av_frame_alloc(),
                             [](auto *p) { av_frame_free(&p); });
    Frame->width = Codec->width;
    Frame->height = Codec->height;
    Frame->format = Codec->pix_fmt;

    TranscodeFrame = Wrapper<AVFrame>(av_frame_alloc(),
                                      [](auto *p) { av_frame_free(&p); });
    TranscodeFrame->width = Codec->width;
    TranscodeFrame->height = Codec->height;
    TranscodeFrame->format = AV_PIX_FMT_RGBA;

    result = av_frame_get_buffer(Frame.get(), 0);

    if (result < 0) {
        throw NotSupportedError();
    }

    Packet = Wrapper<AVPacket>(av_packet_alloc(),
                               [](auto *p) { av_packet_free(&p); });

    Transcoder = Wrapper<struct SwsContext>(
            sws_getContext(width,
                           height,
                           AV_PIX_FMT_RGBA,
                           width,
                           height,
                           (enum AVPixelFormat)Frame->format,
                           SWS_BICUBIC,
                           NULL,
                           NULL,
                           NULL),
            sws_freeContext);

    if (!((Format->oformat)->flags & AVFMT_NOFILE)) {
        result = avio_open(&Format->pb, path.c_str(), AVIO_FLAG_WRITE);

        if (result < 0) {
            throw IOError();
        }
    }

    result = avformat_write_header(Format.get(), &options.Dictionary);

    if (result < 0) {
        throw EncodeError();
    }

#    ifndef NDEBUG
    av_dump_format(Format.get(), 0, path.c_str(), 1);
#    endif
}

LibAV::~LibAV() {
}

void LibAV::WriteFrame(const Canvas &canvas) {
    int result = av_frame_make_writable(Frame.get());

    if (result < 0) {
        throw EncodeError();
    }

    result = av_image_fill_arrays(TranscodeFrame->data,
                                  TranscodeFrame->linesize,
                                  canvas.Buffer,
                                  AV_PIX_FMT_RGBA,
                                  canvas.Width,
                                  canvas.Height,
                                  LEVEL1_DCACHE_LINESIZE);

    if (result < 0) {
        throw EncodeError();
    }

    result = sws_scale(Transcoder.get(),
                       (const uint8_t *const *)TranscodeFrame->data,
                       TranscodeFrame->linesize,
                       0,
                       canvas.Height,
                       Frame->data,
                       Frame->linesize);

    if (result < 0) {
        throw EncodeError();
    }

    Frame->pts = PTS++;

    result = avcodec_send_frame(Codec.get(), Frame.get());

    while (result >= 0) {
        result = avcodec_receive_packet(Codec.get(), Packet.get());

        if (result == AVERROR(EAGAIN) || result == AVERROR_EOF)
            break;
        else if (result < 0) {
            throw EncodeError();
        }

        av_packet_rescale_ts(Packet.get(), Codec->time_base, Stream->time_base);

        Packet->stream_index = Stream->index;

        result = av_interleaved_write_frame(Format.get(), Packet.get());
        if (result < 0) {
            throw EncodeError();
        }
    }
}

void LibAV::Flush() {
    av_write_trailer(Format.get());
}

} // namespace Encoding
} // namespace trc

#endif /* DISABLE_LIBAV */
