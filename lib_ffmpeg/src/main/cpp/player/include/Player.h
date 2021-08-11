//
// Created by LN on 2021/1/4.
//
#pragma once
#ifndef PLAYER_PLAYER_H
#define PLAYER_PLAYER_H

#include <android/native_window_jni.h>

#include <PlayerInfo.h>
#include <RecorderInfo.h>


class Player {

public:
    Player(int id);

    ~Player();

public:
    jobject jPlayerObject{};
    int playerId;
    PlayerInfo *playerInfo = NULL;
    RecorderInfo *recorderInfo = NULL;

    void (*playStateListener)(PlayState, int) = NULL;

    void (*recorderStateListener)(RecordState, int) = NULL;

public:

    static void SetDebug(bool isDebug);

    void StartRecorderThread() const;

    void StartDeMuxThread();

    void StartOpenResourceThread(char *res) const;

    int InitPlayerInfo();

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

    int Release(void) const;

    void SetPlayStateChangeListener(void (*listener)(PlayState playState, int id));

    void SetRecordStateChangeListener(void (*listener)(RecordState playState, int id));


};


#endif



