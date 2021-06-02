//
// Created by LN on 2021/2/24.
//

#include <android_log.h>
#include "include/RecorderInfo.h"

RecorderInfo::~RecorderInfo() {
    if (o_fmt_ctx != NULL) {
        avformat_free_context(o_fmt_ctx);
        o_fmt_ctx = NULL;
    }
    LOGD("delete RecorderInfo over!");
}

void RecorderInfo::SetRecordState(RecordState state) {
    recordState = state;
}

RecordState RecorderInfo::GetRecordState() {
    return recordState;
}
