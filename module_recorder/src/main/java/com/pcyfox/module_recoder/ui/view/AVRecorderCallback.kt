package com.pcyfox.module_recoder.ui.view

import java.io.File

interface AVRecorderCallback {
    fun onPrepareRecord()
    fun onStartRecord(audio: File?, video: File?)
    fun onPauseRecord()
    fun onRecordProgress(mills: Long, amp: Int)
    fun onStopRecord(audio: File?, video: File?)
    fun onError(throwable: Exception?)
}