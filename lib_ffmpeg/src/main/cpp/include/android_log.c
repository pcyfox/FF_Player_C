
#include "android_log.h"

void isDebug(bool isDebug) {
    IS_DEBUG = isDebug;
}

void printCharsHex(const char *data, int length, int printLen, const char *tag) {
    if (!IS_DEBUG) {
        return;
    }
    LOGD("-----------------------------%s-printLen=%d--------------------------------------->", tag,
         printLen);
    if (printLen > length) {
        return;
    }
    for (int i = 0; i < printLen; ++i) {
        LOGD("------------%s:i=%d,char=%02x", tag, i, *(data + i));
    }
}

void LOG(int level, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    __android_log_vprint(level, TAG, fmt, args);
    va_end(args);
}


void LOGV(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    __android_log_vprint(ANDROID_LOG_VERBOSE, TAG, fmt, args);
    va_end(args);
}


void LOGD(const char *fmt, ...) {
    if (!IS_DEBUG) {
        return;
    }
    va_list args;
    va_start(args, fmt);
    __android_log_vprint(ANDROID_LOG_DEBUG, TAG, fmt, args);
    va_end(args);
}


void LOGI(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    __android_log_vprint(ANDROID_LOG_INFO, TAG, fmt, args);
    va_end(args);
}


void LOGW(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    __android_log_vprint(ANDROID_LOG_WARN, TAG, fmt, args);
    va_end(args);
}

void LOGE(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    __android_log_vprint(ANDROID_LOG_ERROR, TAG, fmt, args);
    va_end(args);
}




