//
// Created by LN on 2021/1/4.
//
#pragma once
#ifndef PLAYER_PLAYER_H
#define PLAYER_PLAYER_H

#include <android/native_window_jni.h>

#include <PlayerInfo.h>
#include <RecorderInfo.h>


static bool LOG_DEBUG = false;

class Player {

public:
    Player(int id);

    ~Player();

public:
    jobject jPlayerObject;
    int playerId;
    PlayerInfo *playerInfo = NULL;
    RecorderInfo *recorderInfo = NULL;

    void (*playStateListener)(PlayState, RecordState recordState, int) = NULL;

public:

    void SetDebug(bool isDebug);

    void StartRecorderThread();

    void StartDeMuxThread();

    void StartOpenResourceThread(char *res);

    int InitPlayerInfo();

    int SetResource(char *resource);

    int PrepareRecorder(char *outPath);

    int Configure(ANativeWindow *window, int w, int h, bool isOnly);

    int OnWindowChange(ANativeWindow *window, int w, int h) const;

    int OnWindowDestroy(ANativeWindow *window);

    int Start(void);

    int Play(void);

    int Pause(int delay);

    int Resume(void);

    int Stop(void);

    void SetPlayStateChangeListener(void (*listener)(PlayState playState, int id));

    void SetRecordStateChangeListener(void (*listener)(RecordState playState, int id));


    int PauseRecord(void);

    int StopRecord(void);

    int StartRecord(void);

    int ResumeRecord(void);

    int Release(void);

};


#endif



