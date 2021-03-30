//
// Created by LN on 2021/1/8.
//

#include "PlayerInfo.h"

PlayerInfo::PlayerInfo() {
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
    playState = UN_USELESS;
    av_register_all();
    LOGD("-------PlayerInfo created---------");
}

PlayerInfo::~PlayerInfo() {
    LOGW("-------PlayerInfo Delete Start---------");
    if (window) {
        ANativeWindow_release(window);
        window = NULL;
    }
    if (AMediaCodec) {
        AMediaCodec_delete(AMediaCodec);
        AMediaCodec = NULL;
    }

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);
    avformat_free_context(outContext);
    outContext = NULL;
    avformat_free_context(inputContext);
    inputContext = NULL;
    decode_thread = 0;
    deMux_thread = 0;
    open_resource_thread = 0;
    LOGW("-------PlayerInfo Delete Over---------");
}


void PlayerInfo::SetPlayState(PlayState s) volatile {
    playState = s;
    if (stateListener) {
        stateListener(playState);
    }
    LOGD("PlayerInfo SetPlayState() :%d", s);
}


PlayState PlayerInfo::GetPlayState() {
    return playState;
}


void PlayerInfo::SetStateListener(void (*listener)(PlayState)) {
    this->stateListener = listener;
}


