//
// Created by LN on 2022/1/5.
//

#ifndef FF_PLAYER_C_UTIL_HPP
#define FF_PLAYER_C_UTIL_HPP


static int GetStartCodeLen(const unsigned char *pkt) {
    if (pkt[3] == 1) {
        return 4;
    } else if (pkt[2] == 1) {
        return 3;
    } else {
        return 0;
    }
}


static int GetNALUType(AVPacket *packet) {
    int type = -1;
    const uint8_t *buf = packet->data;
    int len = GetStartCodeLen(buf);
    if (len >= 3 && packet->size >= len) {
        type = buf[len] & 0xFF;
    }
    return type;
}


AVStream *findStream(AVFormatContext *fmt_ctx, AVMediaType type) {
    //输出多媒体文件信息,第二个参数是流的索引值（默认0），第三个参数，0:输入流，1:输出流
    int index = av_find_best_stream(fmt_ctx, type, -1, -1, NULL, 0);
    if (index == AVERROR_STREAM_NOT_FOUND) {
        return NULL;
    } else {
        return fmt_ctx->streams[index];
    }
}

//H.264:AVCC->AnnexB
static int H264_mp4toannexb_filter(AVBSFContext *bsf_ctx, AVPacket *pkt) {
// 1 获取相应的比特流过滤器
    // FLV/MP4/MKV等结构中，h264需要h264_mp4toannexb处理。添加SPS/PPS等信息。
    // FLV封装时，可以把多个NALU放在一个VIDEO TAG中,结构为4B NALU长度+NALU1+4B NALU长度+NALU2+...,
    // 需要做的处理把4B长度换成00000001或者000001
    // annexb模式: startcode 00000001 AVCC模式: 无startcode (mp4 flv mkv)
    // 4 发送和接受
    // bitstreamfilter内部去维护内存空间
    if (av_bsf_send_packet(bsf_ctx, pkt) != 0) {
        return PLAYER_RESULT_ERROR;
    }
    while (av_bsf_receive_packet(bsf_ctx, pkt) != 0);
    // LOGD("av_bsf_receive_packet,ret=%d", ret);
    return PLAYER_RESULT_OK;
}


static void dumpStreamInfo(AVCodecParameters *codecpar) {
    uint8_t exSize = codecpar->extradata_size;
    if (exSize > 0) {
        uint8_t *extraData = codecpar->extradata;
        printCharsHex((char *) extraData, exSize, exSize - 1, "SPS-PPS");
    }
    int w = codecpar->width;
    int h = codecpar->height;
    int level = codecpar->level;
    int profile = codecpar->profile;
    int sample_rate = codecpar->sample_rate;
    const int codecId = codecpar->codec_id;
    LOGD("input video stream info :w=%d,h=%d,codec=%d,level=%d,profile=%d,sample_rate=%d",
         w, h,
         codecId, level, profile, sample_rate);
    int format = codecpar->format;
    switch (format) {
        case AV_PIX_FMT_YUV420P:
            LOGD("input video color format is YUV420p");
            break;
        case AV_PIX_FMT_YUYV422:   ///< packed YUV 4:2:2, 16bpp, Y0 Cb Y1 Cr
            LOGD("input video color format is YUVV422");
            break;
        case AV_PIX_FMT_YUV422P:
            LOGD("input video color format is YUV422P");
            break;
        case AV_PIX_FMT_YUV444P:
            LOGD("input video color format is YUV444P");
            break;
        case AV_PIX_FMT_YUVJ420P://ffmpeg 默认格式
            LOGD("input video color format is YUVJ420P");
            break;
        default:
            LOGD("input video color format is other");
            break;
    }
}

#endif //FF_PLAYER_C_UTIL_HPP
