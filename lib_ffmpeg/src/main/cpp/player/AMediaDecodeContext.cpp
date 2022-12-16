//
// Created by LN on 2021/8/23.
//

#include <cstdint>
#include <cstring>
#include <Utils.h>
#include "AMediaDecodeContext.h"

void dumpOutFormat(AMediaCodec *videoMediaCodec) {
//size_t outsize = 0;
//u_int8_t *outData = AMediaCodec_getOutputBuffer(codec, status, &outsize);
    auto formatType = AMediaCodec_getOutputFormat(videoMediaCodec);
    LOGD("Decode:format formatType to: %s\n",
         AMediaFormat_toString(formatType)
    );
    int colorFormat = 0;
    int width = 0;
    int height = 0;
    AMediaFormat_getInt32(formatType,
                          "color-format", &colorFormat);
    AMediaFormat_getInt32(formatType,
                          "width", &width);
    AMediaFormat_getInt32(formatType,
                          "height", &height);
    LOGD("Decode:format color-format : %d ,width ：%d, height :%d", colorFormat,
         width,
         height);
}


int decode(AMediaCodec *codec, uint8_t *data, int length, int64_t pts) {
    if (!codec || !data) {
        return PLAYER_RESULT_ERROR;
    }

    // 获取buffer的索引
    ssize_t index = AMediaCodec_dequeueInputBuffer(codec, TIMEOUT_US);
    if (index >= 0) {
        size_t out_size;
        uint8_t *inputBuf = AMediaCodec_getInputBuffer(codec, index, &out_size);
        if (inputBuf != nullptr && length <= out_size) {
            //clear buf
            memset(inputBuf, 0, out_size);
            // 将待解码的数据copy到硬件中
            memcpy(inputBuf, data, length);
            if (pts <= 0) {
                pts = getCurrentTime();
            }

            media_status_t status = AMediaCodec_queueInputBuffer(codec, index, 0, length,
                                                                 pts, 0);

            if (status != AMEDIA_OK) {
                LOGE("queue input buffer error status=%d", status);
            }
        }
    }

    AMediaCodecBufferInfo bufferInfo;
    ssize_t status = AMediaCodec_dequeueOutputBuffer(codec, &bufferInfo, TIMEOUT_US);

    if (status > 0) {
        AMediaCodec_releaseOutputBuffer(codec, status, bufferInfo.size != 0);
        if (bufferInfo.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
            LOGE("video producer output EOS");
        }
        if (IS_DEBUG) {
            //dumpOutFormat(codec)
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
            case AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM:
                LOGW("end of stream");
                break;
            default:
                LOGE("unexpected info code: %zd", status);
        }
    }
    return (int) status;
}


int AMediaDecodeContext::decodeVideo(uint8_t *data, int length, int64_t pts) const {
    return decode(videoCodec, data, length, pts);
}

int AMediaDecodeContext::decodeAudio(uint8_t *data, int length, int64_t pts) const {
    return decode(audioCodec, data, length, pts);
}


int AMediaDecodeContext::release() {
    if (nativeWindow) {
        ANativeWindow_release(nativeWindow);
        nativeWindow = nullptr;
    }
    if (videoCodec) {
        AMediaCodec_stop(videoCodec);
        AMediaCodec_delete(videoCodec);
        videoCodec = nullptr;
    }
    if (audioCodec) {
        AMediaCodec_stop(audioCodec);
        AMediaCodec_delete(audioCodec);
        audioCodec = nullptr;
    }
    return 0;
}


int AMediaDecodeContext::stop() const {
    if (videoCodec) {
        return AMediaCodec_stop(videoCodec) == AMEDIA_OK ? PLAYER_RESULT_OK
                                                         : PLAYER_RESULT_ERROR;
    }

    return PLAYER_RESULT_ERROR;
}

int AMediaDecodeContext::start() const {
    if (videoCodec) {
        return AMediaCodec_start(videoCodec) == AMEDIA_OK ? PLAYER_RESULT_OK
                                                          : PLAYER_RESULT_ERROR;
    }

    return PLAYER_RESULT_ERROR;
}

int AMediaDecodeContext::flush() const {
    if (videoCodec) {
        return AMediaCodec_flush(videoCodec) == AMEDIA_OK ? PLAYER_RESULT_OK
                                                          : PLAYER_RESULT_ERROR;
    }
    return PLAYER_RESULT_ERROR;
}


int AMediaDecodeContext::createVideoCodec(const char *mineType, ANativeWindow *window, int w,
                                          int h,
                                          uint8_t *sps,
                                          int spsSize,
                                          uint8_t *pps, int ppsSize) {

    if (w * h <= 0) {
        LOGE("MediaDecoder createVideoCodec() not support video size");
        return PLAYER_RESULT_ERROR;
    }
    if (videoCodec) {
        AMediaCodec_stop(videoCodec);
        AMediaCodec_delete(videoCodec);
        videoCodec = nullptr;
        LOGW("MediaDecoder createAMediaCodec() delete old codec!");
    }
    videoCodec = AMediaCodec_createDecoderByType(mineType);
    if (!videoCodec) {
        LOGE("MediaDecoder createAMediaCodec()  create decoder fail!");
        return PLAYER_RESULT_ERROR;
    } else {
        LOGI("MediaDecoder createAMediaCodec() create decoder success!");
    }
    AMediaFormat *videoFormat = AMediaFormat_new();
    AMediaFormat_setString(videoFormat, "mime", mineType);
    this->videoMineType = mineType;
    if (window) {
        nativeWindow = window;
    }

    AMediaFormat_setInt32(videoFormat, AMEDIAFORMAT_KEY_WIDTH, w); // 宽度
    AMediaFormat_setInt32(videoFormat, AMEDIAFORMAT_KEY_HEIGHT, h); // 高度

    if (spsSize && sps) {
        AMediaFormat_setBuffer(videoFormat, "csd-0", sps, spsSize); // sps
    }
    if (ppsSize && pps) {
        AMediaFormat_setBuffer(videoFormat, "csd-1", pps, ppsSize); // pps
    }

    media_status_t status = AMediaCodec_configure(videoCodec, videoFormat, nativeWindow,
                                                  nullptr,
                                                  0);

    if (status == AMEDIA_OK) {
        LOGD("MediaDecoder configure AMediaCodec success!");
    } else {
        LOGE("MediaDecoder configure AMediaCodec fail!,ret=%d", status);
        AMediaCodec_delete(videoCodec);
        videoCodec = nullptr;
        return PLAYER_RESULT_ERROR;
    }
    return PLAYER_RESULT_OK;
}


int AMediaDecodeContext::initVideoCodec(uint8_t *sps, int spsSize, uint8_t *pps, int ppsSize) {
    if (nativeWindow) {
        return createVideoCodec(videoMineType, nativeWindow, width, height, sps, spsSize, pps, ppsSize);
    }
    return PLAYER_RESULT_ERROR;
}

void
AMediaDecodeContext::configVideo(char *mineType, ANativeWindow *window, int w, int h) {
    this->videoMineType = mineType;
    this->nativeWindow = window;
    this->width = w;
    this->height = h;
}


void AMediaDecodeContext::configAudio(char *mineType) {

}


bool
MakeAACCodecSpecificData(AMediaFormat *meta,
                         unsigned int profile,
                         unsigned int sampling_freq_index,
                         int32_t channel_configuration,
                         char *mineType) {

    if (sampling_freq_index > 11u) {
        return false;
    }

    uint8_t csd[2];
    csd[0] = ((profile + 1) << 3) | (sampling_freq_index >> 1);
    csd[1] = ((sampling_freq_index << 7) & 0x80) | (channel_configuration << 3);

    static const int32_t kSamplingFreq[] = {
            96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050,
            16000, 12000, 11025, 8000
    };
    int32_t sampleRate = kSamplingFreq[sampling_freq_index];

    AMediaFormat_setBuffer(meta, "csd-0", csd, sizeof(csd));
    AMediaFormat_setString(meta, AMEDIAFORMAT_KEY_MIME, mineType);
    AMediaFormat_setInt32(meta, AMEDIAFORMAT_KEY_SAMPLE_RATE, sampleRate);
    AMediaFormat_setInt32(meta, AMEDIAFORMAT_KEY_CHANNEL_COUNT, channel_configuration);
    return true;
}


AMediaDecodeContext::~AMediaDecodeContext() {
    release();
}


