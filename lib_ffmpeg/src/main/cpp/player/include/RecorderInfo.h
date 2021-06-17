//
// Created by LN on 2021/2/24.
//

#pragma once
#ifndef TCTS_EDU_APP_RECODER_RECORDERINFO_H
#define TCTS_EDU_APP_RECODER_RECORDERINFO_H

#include "StateListener.h"
#include "AsyncQueue.hpp"

#ifdef __cplusplus
extern "C" {

#include"libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#endif

#ifdef __cplusplus
}
#endif


class RecorderInfo {
public:
    int id;
    pthread_t recorder_thread = 0;
    AsyncQueue<AVPacket> packetQueue;
    char *storeFile;
    AVFormatContext *o_fmt_ctx = NULL;
    AVStream *inputVideoStream = NULL;
    AVStream *o_video_stream = NULL;
    volatile enum RecordState recordState = UN_START_RECORD;
private:
    void (*listener)(RecordState, int);

public:
    ~RecorderInfo();

    void SetRecordState(RecordState state, int id);

    RecordState GetRecordState();

    void SetStateListener(void (*listener)(RecordState, int));
};


#endif //TCTS_EDU_APP_RECODER_RECORDERINFO_H
