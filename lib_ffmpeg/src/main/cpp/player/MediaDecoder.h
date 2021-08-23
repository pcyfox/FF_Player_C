//
// Created by LN on 2021/8/23.
//

#ifndef FF_PLAYER_C_MEDIADECODER_H
#define FF_PLAYER_C_MEDIADECODER_H

#include <android/native_window_jni.h>
#include <android/native_window.h>
#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <PlayerResult.h>


#ifdef __cplusplus
extern "C" {
#include "android_log.h"
#endif

#ifdef __cplusplus
}
#endif


class MediaDecoder {

public:
    AMediaCodec *mediaCodec = nullptr;
    ANativeWindow *nativeWindow = nullptr;
    char *mine;
    int width;
    int height;

public:


    void config(char *mine, ANativeWindow *nativeWindow, int width, int height);

    int init(char *mine, ANativeWindow *nativeWindow, int width, int height, uint8_t *sps,
             int spsSize, uint8_t *pps, int ppsSize);


    int init(uint8_t *sps, int spsSize, uint8_t *pps, int ppsSize);


    int start() const;

    int flush() const;

    int stop() const;


    int release();

    int decode(uint8_t *data, int length, int64_t pts) const;

};


#endif //FF_PLAYER_C_MEDIADECODER_H
