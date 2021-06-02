//
// Created by LN on 2021/1/8.
//

#ifndef TCTS_EDU_APP_RECODER_STATELISTENER_H
#define TCTS_EDU_APP_RECODER_STATELISTENER_H

enum PlayState {
    UN_USELESS, INITIALIZED, PREPARED, STOPPED, ERROR, STARTED, PAUSE,RELEASE
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
    virtual void onStateChange(PlayState state);

};


#endif //TCTS_EDU_APP_RECODER_STATELISTENER_H
