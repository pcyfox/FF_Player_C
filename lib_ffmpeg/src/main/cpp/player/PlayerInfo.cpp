//
// Created by LN on 2021/1/8.
//

#include "PlayerInfo.h"

PlayerInfo::PlayerInfo() {
    pthread_mutex_init(&mutex, nullptr);
    pthread_cond_init(&cond, nullptr);
    playState = UN_USELESS;
    av_register_all();
    inputContext = avformat_alloc_context();
    if (!inputContext) {
        LOGE("avformat_alloc_context fail");
    }
    LOGD("-------PlayerInfo created---------");
}

PlayerInfo::~PlayerInfo() {
    LOGW("-------PlayerInfo Delete Start---------");
    if (window) {
        ANativeWindow_release(window);
        window = nullptr;
    }
    if (videoCodec) {
        AMediaCodec_stop(videoCodec);
        AMediaCodec_delete(videoCodec);
        videoCodec = nullptr;
    }

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);
    if (outContext != nullptr) {
        avformat_free_context(outContext);
        outContext = nullptr;
    }
    if (inputContext != nullptr) {
        avformat_free_context(inputContext);
    }
    inputContext = nullptr;
    decode_thread = 0;
    deMux_thread = 0;
    open_resource_thread = 0;
    LOGW("-------PlayerInfo Delete Over---------");
}


void PlayerInfo::SetPlayState(PlayState s) volatile {
    playState = s;
    if (stateListener) {
        stateListener(playState, id);
    }
    LOGD("PlayerInfo SetPlayState() :%d", s);
}


PlayState PlayerInfo::GetPlayState() {
    return playState;
}


void PlayerInfo::SetStateListener(void (*listener)(PlayState, int)) {
    this->stateListener = listener;
}


