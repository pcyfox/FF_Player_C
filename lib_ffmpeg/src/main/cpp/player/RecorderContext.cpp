//
// Created by LN on 2021/2/24.
//


#ifdef __cplusplus
extern "C" {
#include "AndroidLog.h"
#endif
#ifdef __cplusplus
}
#endif

#include "RecorderContext.h"
#include "include/RecorderContext.h"


RecorderContext::~RecorderContext() {
    LOGD("start to delete RecorderInfo ");
    if (o_fmt_ctx != nullptr) {
        videoPacketQueue.clearAVPacket();
        avformat_free_context(o_fmt_ctx);
        o_fmt_ctx = nullptr;
    }
    free(storeFile);
    storeFile = nullptr;
    LOGD("delete RecorderInfo over!");
}

void RecorderContext::SetRecordState(RecordState state) {
    LOGI("SetRecordState state=%d", state);
    recordState = state;
    if (listener != nullptr) {
        listener(recordState, id);
    }
}

RecordState RecorderContext::GetRecordState() {
    return recordState;
}

void RecorderContext::SetStateListener(void (*l)(RecordState, int)) {
    listener = l;
}

RecorderContext::RecorderContext() {
    videoPacketQueue.tag = "recorder";
}


