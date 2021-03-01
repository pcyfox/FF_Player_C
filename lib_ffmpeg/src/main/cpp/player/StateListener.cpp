//
// Created by LN on 2021/1/8.
//

#include "include/StateListener.h"
#include "android_log.h"


void StateListener::onStateChange(PlayState state) {
    LOGI("onStateChange=%d", state);
}
