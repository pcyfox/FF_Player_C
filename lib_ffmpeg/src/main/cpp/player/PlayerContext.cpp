//
// Created by LN on 2021/1/8.
//

#include "PlayerContext.h"

PlayerContext::PlayerContext() {
    pthread_mutex_init(&mutex, NULL);
    pthread_cond_init(&cond, NULL);
    playState = UNINITIALIZED;
    if (IS_DEBUG) {
        av_log_set_level(AV_LOG_DEBUG);
    }
    av_register_all();
    inputContext = avformat_alloc_context();

    if (!inputContext) {
        LOGE("avformat alloc context fail!");
    }

    videoPacketQueue.tag = "player";
    LOGD("-------PlayerInfo created---------");
}

PlayerContext::~PlayerContext() {
    LOGW("-------PlayerInfo Delete Start---------");
    playState = UNINITIALIZED;
    if (bsf_ctx) {
        av_bsf_free(&bsf_ctx);
    }

    pthread_mutex_destroy(&mutex);
    pthread_cond_destroy(&cond);

    if (inputContext != NULL) {
        avformat_free_context(inputContext);
    }

    inputContext = NULL;
    decode_thread = 0;
    deMux_thread = 0;
    open_resource_thread = 0;
    stateListener = NULL;

    LOGW("-------PlayerInfo Delete Over---------");
}


void PlayerContext::SetPlayState(PlayState s, bool isNotify) volatile {
    playState = s;
    if (stateListener && isNotify) {
        stateListener(playState, id);
    }
    LOGI("----------------->PlayerInfo SetPlayState() called with:state=%s,isNotify=%d",
         StateListener::PlayerStateToString(s).c_str(), isNotify);
}


PlayState PlayerContext::GetPlayState() {
    return playState;
}


void PlayerContext::SetStateListener(void (*listener)(PlayState, int)) {
    this->stateListener = listener;
}


