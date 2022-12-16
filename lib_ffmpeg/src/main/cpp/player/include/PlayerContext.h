//
// Created by Dwayne on 20/11/24.
//


#pragma once
#ifndef FF_PLAYER_PLAYER_INFO
#define FF_PLAYER_PLAYER_INFO

#include <media/NdkMediaCodec.h>
#include "pthread.h"
#include "StateListener.h"
#include "AsyncQueue.hpp"
#include "AMediaDecodeContext.h"
#include "Resource.h"
#include <android/native_window.h>

#ifdef __cplusplus
extern "C" {
#include "AndroidLog.h"
#include"libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#endif

#ifdef __cplusplus
}
#endif


class PlayerContext {

public:
    int id{};
    AVBSFContext *bsf_ctx = NULL;
    AVFormatContext *inputContext = NULL;

    AVStream *inputVideoStream = NULL;
    AVStream *inputAudioStream = NULL;
    bool isOnlyRecordMedia = false;
    Resource resource{};

    int width{}, height{};

    AsyncQueue<AVPacket> videoPacketQueue;
    AsyncQueue<AVPacket> audioPacketQueue;

    pthread_t decode_thread = 0;
    pthread_t deMux_thread = 0;
    pthread_t open_resource_thread = 0;

    pthread_mutex_t mutex{};
    pthread_cond_t cond{};

    bool isOpenAudio = false;
    char *mine = "video/avc";

    volatile enum PlayState playState = UNINITIALIZED;

    AMediaDecodeContext mediaDecodeContext;

private:
    void (*stateListener)(PlayState, int) = NULL;


public:

    PlayerContext();

    ~PlayerContext();

    void SetPlayState(PlayState s, bool isNotify) volatile;

    PlayState GetPlayState();

    void SetStateListener(void (*stateListener)(PlayState, int));

};

#endif //AUDIO_PRACTICE_QUEUE_H
