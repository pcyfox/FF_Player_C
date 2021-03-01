//
// Created by LN on 2021/2/24.
//

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
    pthread_t recorder_thread = 0;
    AsyncQueue<AVPacket> packetQueue;
    char *storeFile;
    AVFormatContext *o_fmt_ctx = NULL;
    AVStream *o_video_stream = NULL;
    volatile enum RecordState recordState = UN_START_RECORD;

public:
    ~RecorderInfo();

    void SetRecordState(RecordState state);

    RecordState GetRecordState();
};


#endif //TCTS_EDU_APP_RECODER_RECORDERINFO_H
