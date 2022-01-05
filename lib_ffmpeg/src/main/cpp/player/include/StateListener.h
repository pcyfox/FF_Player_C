//
// Created by LN on 2021/1/8.
//
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
    RECORD_UN_START,
    RECORD_ERROR,
    RECORD_PREPARED,
    RECORD_START,
    RECORDING,
    RECORD_PAUSE,
    RECORD_RESUME,
    RECORD_STOP,
    RECORDER_RELEASE,
};


class StateListener {

public:
    static void onStateChange(PlayState state);

    static std::string PlayerStateToString(int state);


};


#endif //TCTS_EDU_APP_RECODER_STATELISTENER_H
