//
// Created by LN on 2021/8/23.
//

#ifndef FF_PLAYER_C_AMEDIADECODECONTEXT_H
#define FF_PLAYER_C_AMEDIADECODECONTEXT_H

#include <android/native_window_jni.h>
#include <android/native_window.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>

#include <PlayerResult.h>
#include "libavcodec/avcodec.h"


#ifdef __cplusplus
extern "C" {
#include "AndroidLog.h"
#endif

#ifdef __cplusplus
}
#endif


#define TIMEOUT_US  10000

#define  MEDIA_MIMETYPE_AUDIO_AAC  "audio/mp4a-latm"
#define  MEDIA_MIMETYPE_VIDEO_AVC  "video/avc"

//wrap Android MediaCodec
class AMediaDecodeContext {

public:
    AMediaCodec *videoCodec = nullptr;
    AMediaCodec *audioCodec = nullptr;

    ANativeWindow *nativeWindow = nullptr;
    const char *videoMineType;
    int width;
    int height;


public:
    ~AMediaDecodeContext();

    void configAudio(char *mine);

    void configVideo(char *mine, ANativeWindow *nativeWindow, int width, int height);


    int createVideoCodec(const char *mineType, ANativeWindow *window, int width, int height,
                         uint8_t *sps,
                         int spsSize, uint8_t *pps, int ppsSize);


    int initVideoCodec(uint8_t *sps, int spsSize, uint8_t *pps, int ppsSize);


    int start() const;

    int flush() const;

    int stop() const;

    int release();

    int decodeVideo(uint8_t *data, int length, int64_t pts) const;

    int decodeAudio(uint8_t *data, int length, int64_t pts) const;

};


#endif //FF_PLAYER_C_AMEDIADECODECONTEXT_H
