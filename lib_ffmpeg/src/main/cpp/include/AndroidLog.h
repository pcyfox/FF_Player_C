#pragma once

#ifndef H_LOG
#define H_LOG

#include <android/log.h>
#include <stdbool.h>
#include "libavutil/log.h"

#define TAG "ff-player"

static bool IS_DEBUG = true;

void LOGD(const char *fmt, ...);

void LOGV(const char *fmt, ...);

void LOGI(const char *fmt, ...);

void LOGW(const char *fmt, ...);

void LOGE(const char *fmt, ...);

void printCharsHex(const char *data, int length, int printLen, const char *tag);

void isDebug(bool isDebug);


static void ffmpeg_android_log_callback(void *ptr, int level, const char *fmt, va_list vl) {

    if (IS_DEBUG) {
        switch (level) {
            case AV_LOG_DEBUG:
                LOGD(fmt, vl);
                break;
            case AV_LOG_VERBOSE:
                LOGV(fmt, vl);
                break;
            case AV_LOG_INFO:
                LOGI(fmt, vl);
                break;
            case AV_LOG_WARNING:
                LOGW(fmt, vl);
                break;
            case AV_LOG_ERROR:
                LOGE(fmt, vl);
                break;
        }
    }
}


#endif
