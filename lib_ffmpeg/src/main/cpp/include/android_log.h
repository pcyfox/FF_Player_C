#pragma once

#ifndef H_LOG
#define H_LOG

#include <android/log.h>
#include <stdbool.h>

#define TAG "ff-player"

static bool IS_DEBUG = false;

void LOGD(const char *fmt, ...);

void LOGI(const char *fmt, ...);

void LOGW(const char *fmt, ...);

void LOGE(const char *fmt, ...);

void printCharsHex(const char *data, int length, int printLen, const char *tag);

void isDebug(bool isDebug);

#endif
