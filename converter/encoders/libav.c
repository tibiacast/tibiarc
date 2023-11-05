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

#    include "encoding.h"

#    ifdef __cplusplus
extern "C"
{
#    endif

#    include <libavcodec/avcodec.h>
#    include <libavformat/avformat.h>
#    include <libavutil/imgutils.h>
#    include <libavutil/pixdesc.h>
#    include <libswscale/swscale.h>

#    ifdef __cplusplus
}
#    endif

#    include "utils.h"

struct trc_encoder_libav {
    struct trc_encoder Base;

    AVFormatContext *Format;
    AVCodecContext *Codec;
    AVPacket *Packet;
    AVStream *Stream;
    AVFrame *Frame;

    struct SwsContext *Transcoder;
    AVFrame *TranscodeFrame;

    int64_t PTS;

    int Opened;
};

/* Try to find the best possible image format/range we can get when converting
 * from our native BGR24. */
static bool libav_SetBestPixelFormat(const AVCodec *codec,
                                     AVCodecContext *context) {
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
    return best_format != AV_PIX_FMT_NONE;
}

static bool libav_Error(int error) {
    char message[512];
    av_strerror(error, message, sizeof(message));
    return trc_ReportError("%s", message);
}

static bool libav_Open(struct trc_encoder_libav *encoder,
                       const char *format,
                       const char *encoding,
                       const char *flags,
                       int xResolution,
                       int yResolution,
                       int frameRate,
                       const char *path) {
    AVDictionary *options = NULL;
    int result;

    if (flags != NULL) {
        result = av_dict_parse_string(&options, flags, "=", ":", 0);

        if (result < 0) {
            /* av_dict_parse_string may build a partial result on failure. */
            av_dict_free(&options);
            return libav_Error(result);
        }
    }

    result = avformat_alloc_output_context2(&encoder->Format,
                                            NULL,
                                            format,
                                            path);
    if (result < 0) {
        av_dict_free(&options);

        /* We can return without cleaning up the format context, as libav_Free
         * will take care of that for us. It also ignores all fields that are
         * NULL so we won't have to bother keeping track of which fields have
         * been initialized and which have not. */
        return libav_Error(result);
    }

    const AVOutputFormat *outputFormat = (encoder->Format)->oformat;
    const AVCodec *codec;

    if (encoding == NULL) {
        codec = avcodec_find_encoder(outputFormat->video_codec);
    } else {
        codec = avcodec_find_encoder_by_name(encoding);
    }

    if (codec == NULL) {
        av_dict_free(&options);

        if (encoding != NULL) {
            return trc_ReportError("failed to find a codec by the name '%s'",
                                   encoding);
        }

        return trc_ReportError("failed open default codec for '%s'",
                               outputFormat->long_name);
    }

    encoder->Codec = avcodec_alloc_context3(codec);

    (encoder->Codec)->width = xResolution;
    (encoder->Codec)->height = yResolution;

    if (outputFormat->flags & AVFMT_GLOBALHEADER) {
        (encoder->Codec)->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (!libav_SetBestPixelFormat(codec, encoder->Codec)) {
        av_dict_free(&options);
        return trc_ReportError("failed to select a suitable pixel format");
    }

    (encoder->Codec)->time_base = (AVRational){1, frameRate};

    result = avcodec_open2(encoder->Codec, codec, &options);

    if (result < 0) {
        av_dict_free(&options);
        return libav_Error(result);
    }

    encoder->Stream = avformat_new_stream(encoder->Format, NULL);
    (encoder->Stream)->id = 0;
    (encoder->Stream)->time_base = (encoder->Stream)->time_base;

    result = avcodec_parameters_from_context((encoder->Stream)->codecpar,
                                             encoder->Codec);

    if (result < 0) {
        av_dict_free(&options);
        return libav_Error(result);
    }

    encoder->Frame = av_frame_alloc();
    (encoder->Frame)->width = (encoder->Codec)->width;
    (encoder->Frame)->height = (encoder->Codec)->height;
    (encoder->Frame)->format = (encoder->Codec)->pix_fmt;

    encoder->TranscodeFrame = av_frame_alloc();
    (encoder->TranscodeFrame)->width = (encoder->Codec)->width;
    (encoder->TranscodeFrame)->height = (encoder->Codec)->height;
    (encoder->TranscodeFrame)->format = AV_PIX_FMT_RGBA;

    result = av_frame_get_buffer(encoder->Frame, 0);

    if (result < 0) {
        av_dict_free(&options);
        return libav_Error(result);
    }

    encoder->Packet = av_packet_alloc();

    encoder->Transcoder =
            sws_getContext(xResolution,
                           yResolution,
                           AV_PIX_FMT_RGBA,
                           xResolution,
                           yResolution,
                           (enum AVPixelFormat)(encoder->Frame)->format,
                           SWS_BICUBIC,
                           NULL,
                           NULL,
                           NULL);

    if (!(((encoder->Format)->oformat)->flags & AVFMT_NOFILE)) {
        result = avio_open(&(encoder->Format)->pb, path, AVIO_FLAG_WRITE);

        if (result < 0) {
            av_dict_free(&options);
            return libav_Error(result);
        }
    }

    result = avformat_write_header(encoder->Format, &options);

    /* We won't consult options from here on. */
    av_dict_free(&options);

    if (result < 0) {
        return libav_Error(result);
    }

#    ifdef DEBUG
    av_dump_format(encoder->Format, 0, path, 1);
#    endif

    encoder->Opened = 1;
    return true;
}

static bool libav_WriteFrame(struct trc_encoder_libav *encoder,
                             struct trc_canvas *frame) {
    int result = av_frame_make_writable(encoder->Frame);

    if (result < 0) {
        return libav_Error(result);
    }

    result = av_image_fill_arrays((encoder->TranscodeFrame)->data,
                                  (encoder->TranscodeFrame)->linesize,
                                  frame->Buffer,
                                  AV_PIX_FMT_RGBA,
                                  frame->Width,
                                  frame->Height,
                                  LEVEL1_DCACHE_LINESIZE);

    if (result < 0) {
        return libav_Error(result);
    }

    result = sws_scale(encoder->Transcoder,
                       (const uint8_t *const *)(encoder->TranscodeFrame)->data,
                       (encoder->TranscodeFrame)->linesize,
                       0,
                       frame->Height,
                       (encoder->Frame)->data,
                       (encoder->Frame)->linesize);

    if (result < 0) {
        return libav_Error(result);
    }

    (encoder->Frame)->pts = encoder->PTS;
    encoder->PTS += 1;

    result = avcodec_send_frame(encoder->Codec, encoder->Frame);

    while (result >= 0) {
        result = avcodec_receive_packet(encoder->Codec, encoder->Packet);

        if (result == AVERROR(EAGAIN) || result == AVERROR_EOF)
            break;
        else if (result < 0) {
            return libav_Error(result);
        }

        av_packet_rescale_ts(encoder->Packet,
                             (encoder->Codec)->time_base,
                             (encoder->Stream)->time_base);

        (encoder->Packet)->stream_index = (encoder->Stream)->index;

        result = av_interleaved_write_frame(encoder->Format, encoder->Packet);
        if (result < 0) {
            return libav_Error(result);
        }
    }

    return true;
}

static bool libav_Free(struct trc_encoder_libav *encoder) {
    if (encoder->Opened) {
        av_write_trailer(encoder->Format);
    }

    if (encoder->Format) {
        if (!(((encoder->Format)->oformat)->flags & AVFMT_NOFILE)) {
            avio_closep(&(encoder->Format)->pb);
        }
    }

    avcodec_free_context(&encoder->Codec);
    av_frame_free(&encoder->Frame);
    av_packet_free(&encoder->Packet);

    avformat_free_context(encoder->Format);

    sws_freeContext(encoder->Transcoder);

    checked_deallocate(encoder);

    return true;
}

struct trc_encoder *libav_Create() {
    struct trc_encoder_libav *encoder = (struct trc_encoder_libav *)
            checked_allocate(1, sizeof(struct trc_encoder_libav));

    encoder->Base.Open = (bool (*)(struct trc_encoder *,
                                   const char *,
                                   const char *,
                                   const char *,
                                   int,
                                   int,
                                   int,
                                   const char *))libav_Open;
    encoder->Base.WriteFrame = (bool (*)(struct trc_encoder *,
                                         struct trc_canvas *))libav_WriteFrame;
    encoder->Base.Free = (void (*)(struct trc_encoder *))libav_Free;

    return (struct trc_encoder *)encoder;
}

#endif /* DISABLE_LIBAV */
