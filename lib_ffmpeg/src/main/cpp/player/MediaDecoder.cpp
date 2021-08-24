//
// Created by LN on 2021/8/23.
//

#include <cstdint>
#include <cstring>
#include <Utils.h>
#include "MediaDecoder.h"


int MediaDecoder::decodeVideo(uint8_t *data, int length, int64_t pts) const {
    // 获取buffer的索引
    ssize_t index = AMediaCodec_dequeueInputBuffer(videoMediaCodec, 10000);
    if (index >= 0) {
        size_t out_size;
        uint8_t *inputBuf = AMediaCodec_getInputBuffer(videoMediaCodec, index, &out_size);
        if (inputBuf != NULL && length <= out_size) {
            //clear buf
            memset(inputBuf, 0, out_size);
            // 将待解码的数据copy到硬件中
            memcpy(inputBuf, data, length);
            if (pts <= 0 || !pts) {
                pts = getCurrentTime();
            }
            media_status_t status = AMediaCodec_queueInputBuffer(videoMediaCodec, index, 0, length,
                                                                 pts, 0);
            if (status != AMEDIA_OK) {
                LOGE("queue input buffer error status=%d", status);
            }
        }
    }

    AMediaCodecBufferInfo bufferInfo;
    ssize_t status = AMediaCodec_dequeueOutputBuffer(videoMediaCodec, &bufferInfo, 10000);
    if (status > 0) {
        AMediaCodec_releaseOutputBuffer(videoMediaCodec, status, bufferInfo.size != 0);
        if (bufferInfo.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
            LOGE("video producer output EOS");
        }
        if (IS_DEBUG) {
            //size_t outsize = 0;
            //u_int8_t *outData = AMediaCodec_getOutputBuffer(codec, status, &outsize);
            auto formatType = AMediaCodec_getOutputFormat(videoMediaCodec);
            LOGD("Decode:format formatType to: %s\n", AMediaFormat_toString(formatType));
            int colorFormat = 0;
            int width = 0;
            int height = 0;
            AMediaFormat_getInt32(formatType, "color-format", &colorFormat);
            AMediaFormat_getInt32(formatType, "width", &width);
            AMediaFormat_getInt32(formatType, "height", &height);
            LOGD("Decode:format color-format : %d ,width ：%d, height :%d", colorFormat,
                 width,
                 height);
        }
    } else {
        switch (status) {
            case AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED:
                LOGW("output buffers changed");
                break;
            case AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED:
                LOGW("output buffers format changed");
                break;
            case AMEDIACODEC_INFO_TRY_AGAIN_LATER:
                LOGW("timeout,video no output buffer right now");
                break;
            default:
                LOGE("unexpected info code: %zd", status);
        }
    }
    return status;
}


int MediaDecoder::release() {
    if (nativeWindow) {
        ANativeWindow_release(nativeWindow);
        nativeWindow = nullptr;
    }
    if (videoMediaCodec) {
        AMediaCodec_stop(videoMediaCodec);
        AMediaCodec_delete(videoMediaCodec);
        videoMediaCodec = nullptr;
    }
    return 0;
}


int MediaDecoder::stop() const {
    if (videoMediaCodec) {
        return AMediaCodec_stop(videoMediaCodec) == AMEDIA_OK ? PLAYER_RESULT_OK
                                                              : PLAYER_RESULT_ERROR;
    }

    return PLAYER_RESULT_ERROR;
}

int MediaDecoder::start() const {
    if (videoMediaCodec) {
        return AMediaCodec_start(videoMediaCodec) == AMEDIA_OK ? PLAYER_RESULT_OK
                                                               : PLAYER_RESULT_ERROR;
    }

    return PLAYER_RESULT_ERROR;
}

int MediaDecoder::flush() const {
    if (videoMediaCodec) {
        return AMediaCodec_flush(videoMediaCodec) == AMEDIA_OK ? PLAYER_RESULT_OK
                                                               : PLAYER_RESULT_ERROR;
    }
    return PLAYER_RESULT_ERROR;
}


int MediaDecoder::init(const char *mine, ANativeWindow *window, int width, int height, uint8_t *sps,
                       int spsSize,
                       uint8_t *pps, int ppsSize) {

    if (width * height <= 0) {
        LOGE("MediaDecoder init() not support video size");
        return PLAYER_RESULT_ERROR;
    }
    if (videoMediaCodec) {
        AMediaCodec_stop(videoMediaCodec);
        AMediaCodec_delete(videoMediaCodec);
        videoMediaCodec = nullptr;
        LOGW("MediaDecoder createAMediaCodec() delete old codec!");
    }
    videoMediaCodec = AMediaCodec_createDecoderByType(mine);
    if (!videoMediaCodec) {
        LOGE("MediaDecoder createAMediaCodec()  create decoder fail!");
        return PLAYER_RESULT_ERROR;
    } else {
        LOGI("MediaDecoder createAMediaCodec() create decoder success!");
    }
    AMediaFormat *videoFormat = AMediaFormat_new();
    AMediaFormat_setString(videoFormat, "mime", mine);
    this->mine = mine;
    if (window) {
        nativeWindow = window;
    }

    AMediaFormat_setInt32(videoFormat, AMEDIAFORMAT_KEY_WIDTH, width); // 宽度
    AMediaFormat_setInt32(videoFormat, AMEDIAFORMAT_KEY_HEIGHT, height); // 高度

    if (spsSize && sps) {
        AMediaFormat_setBuffer(videoFormat, "csd-0", sps, spsSize); // sps
    }
    if (ppsSize && pps) {
        AMediaFormat_setBuffer(videoFormat, "csd-1", pps, ppsSize); // pps
    }

    media_status_t status = AMediaCodec_configure(videoMediaCodec, videoFormat, nativeWindow,
                                                  nullptr,
                                                  0);

    if (status == AMEDIA_OK) {
        LOGD("MediaDecoder configure AMediaCodec success!");
    } else {
        LOGE("MediaDecoder configure AMediaCodec fail!,ret=%d", status);
        AMediaCodec_delete(videoMediaCodec);
        videoMediaCodec = nullptr;
        return PLAYER_RESULT_ERROR;
    }
    return PLAYER_RESULT_OK;
}


int MediaDecoder::init(uint8_t *sps, int spsSize, uint8_t *pps, int ppsSize) {
    if (nativeWindow) {
        return init(mine, nativeWindow, width, height, sps, spsSize, pps, ppsSize);
    }
    return PLAYER_RESULT_ERROR;
}

void MediaDecoder::config(char *mine, ANativeWindow *nativeWindow, int width, int height) {
    this->mine = mine;
    this->nativeWindow = nativeWindow;
    this->width = width;
    this->height = height;
}

void MediaDecoder::configAudio(char *mine) {


}

MediaDecoder::~MediaDecoder() {
    release();
}

