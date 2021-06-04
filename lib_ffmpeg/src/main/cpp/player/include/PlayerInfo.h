//
// Created by Dwayne on 20/11/24.
//


#ifndef FF_PLAYER_PLAYER_INFO
#define FF_PLAYER_PLAYER_INFO

#include <media/NdkMediaCodec.h>
#include "pthread.h"
#include "android_log.h"
#include "StateListener.h"
#include "AsyncQueue.hpp"
#include <android/native_window.h>

#ifdef __cplusplus
extern "C" {

#include"libavformat/avformat.h"
#include "libavcodec/avcodec.h"
#include "libavutil/avutil.h"
#endif

#ifdef __cplusplus
}
#endif


class PlayerInfo {

public:
    int id;
    AMediaCodec *videoCodec = NULL;
    AMediaCodec *audioCodec = NULL;
    AVFormatContext *inputContext = NULL;
    AVFormatContext *outContext = NULL;
    AVStream *inputVideoStream = NULL;
    bool isOnlyRecorderNode = false;
    char *resource = NULL;
    int width, height;

    AsyncQueue<AVPacket> packetQueue;

    pthread_t decode_thread = 0;
    pthread_t deMux_thread = 0;
    pthread_t open_resource_thread = 0;

    ANativeWindow *window = NULL;
    int windowWith;
    int windowHeight;

    pthread_mutex_t mutex;
    pthread_cond_t cond;

    int8_t lastNALUType = 0;

    const char *mine = "video/avc";
public:

    volatile enum PlayState playState = UN_USELESS;

    void (*stateListener)(PlayState, int) = NULL;


public:

    PlayerInfo();

    ~PlayerInfo();

    void SetPlayState(PlayState s) volatile;

    PlayState GetPlayState();

    void SetStateListener(void (*stateListener)(PlayState,int));

};

#endif //AUDIO_PRACTICE_QUEUE_H
