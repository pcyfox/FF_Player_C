//
// Created by LN on 2021/1/13.
//

#pragma once
#ifndef TCTS_EDU_APP_RECODER_MUXER_H
#define TCTS_EDU_APP_RECODER_MUXER_H

#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <stdbool.h>
#include <AndroidLog.h>
#include "PlayerResult.h"

//#define LOG_TAG "Muxer------------->"

int MuxAVFile(char *audio_srcPath, char *video_srcPath, char *destPath,
              void (*call_back)(float progress));


#endif //TCTS_EDU_APP_RECODER_MUXER_H
