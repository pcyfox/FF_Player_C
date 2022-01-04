//
// Created by LN on 2021/2/24.
//


#ifdef __cplusplus
extern "C" {
#include "android_log.h"
#endif
#ifdef __cplusplus
}
#endif

#include "RecorderContext.h"

RecorderContext::~RecorderContext() {
    LOGD("start to delete RecorderInfo ");
    if (o_fmt_ctx != NULL) {
        packetQueue.clearAVPacket();
        avformat_free_context(o_fmt_ctx);
        o_fmt_ctx = NULL;
    }
    free(storeFile);
    storeFile=NULL;
    LOGD("delete RecorderInfo over!");
}

void RecorderContext::SetRecordState(RecordState state) {
    LOGI("SetRecordState state=%d", state);
    recordState = state;
    if (listener != NULL) {
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
    //packetQueue.tag = "recorder";
}
