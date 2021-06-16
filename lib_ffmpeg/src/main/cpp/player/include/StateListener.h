//
// Created by LN on 2021/1/8.
//
#pragma once
#ifndef TCTS_EDU_APP_RECODER_STATELISTENER_H
#define TCTS_EDU_APP_RECODER_STATELISTENER_H
#include <string>

enum PlayState {
    UNINITIALIZED,
    INITIALIZED,
    PREPARED,
    EXECUTING,
    STARTED,
    PAUSE,
    STOPPED,
    ERROR,
    RELEASE,
};


enum RecordState {
    UN_START_RECORD,
    RECORD_ERROR,
    RECORD_PREPARED,
    RECORD_START,
    RECORDING,
    RECORD_PAUSE,
    RECORD_STOP
};




class StateListener {

public:
    static void onStateChange(PlayState state);

    static std::string PlayerStateToString(int state) ;


};


#endif //TCTS_EDU_APP_RECODER_STATELISTENER_H
