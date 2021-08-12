//
// Created by LN on 2021/1/4.
//
#pragma once
#ifndef  JPLAYER_METHODS_H
#define JPLAYER_METHODS_H


class JPlayerObject {
public:
    jobject jPlayerObject{};
    jmethodID jMid_onPlayStateChangeId = NULL;
    jmethodID jMid_onRecordStateChangeId = NULL;
};


#endif



