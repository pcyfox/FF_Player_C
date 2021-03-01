//
// Created by LN on 2021/2/24.
//

#include "include/RecorderInfo.h"

RecorderInfo::~RecorderInfo() {
    if (o_fmt_ctx) {
        avformat_free_context(o_fmt_ctx);
        o_fmt_ctx = NULL;
    }
    LOGW("--------RecordInfo Deleted--------");
}

void RecorderInfo::SetRecordState(RecordState state) {
    recordState = state;
}

RecordState RecorderInfo::GetRecordState() {
    return recordState;
}
