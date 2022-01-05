//
// Created by LN on 2021/1/4.
//
#pragma once
#ifndef PLAYER_PLAYER_H
#define PLAYER_PLAYER_H

#include <android/native_window_jni.h>

#include <PlayerContext.h>
#include "RecorderContext.h"
#include "JPlayerObject.h"
#include <android/looper.h>

class Player {

public:
    Player(int id);

    ~Player();

public:
    int playerId;
    PlayerContext *playerContext = NULL;
    RecorderContext *recorderContext = NULL;
    void (*playStateListener)(PlayState, int) = NULL;
    void (*recorderStateListener)(RecordState, int) = NULL;
    JPlayerObject jPlayer;

public:

    static void SetDebug(bool isDebug);

    void StartRecorderThread() const;

    void StartDeMuxThread();

    void StartDecodeThread() const;

    void StartOpenResourceThread() const;

    int InitPlayerContext();

    int SetResource(char *resource);

    int PrepareRecorder(char *outPath);

    int Configure(ANativeWindow *window, int w, int h, bool isOnly) const;

    int OnWindowChange(ANativeWindow *window, int w, int h) const;

    int OnWindowDestroy(ANativeWindow *window);

    int Start(void);

    int Play(void);

    int Pause(int delay) const;

    int Resume(void) const;

    int Stop(void) const;

    int PauseRecord(void) const;

    int StopRecord(void) const;

    int StartRecord(void) const;

    int ResumeRecord(void) const;

    int Release(void) ;

    void SetPlayStateChangeListener(void (*listener)(PlayState playState, int id));

    void SetRecordStateChangeListener(void (*listener)(RecordState playState, int id));


};


#endif



