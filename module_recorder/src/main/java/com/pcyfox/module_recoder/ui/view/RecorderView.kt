package com.pcyfox.module_recoder.ui.view

import android.content.Context
import android.text.TextUtils
import android.util.AttributeSet
import android.util.Log
import android.view.SurfaceView
import android.view.ViewGroup
import android.widget.RelativeLayout
import androidx.annotation.WorkerThread
import com.pcyfox.lib_ffmpeg.FFPlayer
import com.pcyfox.lib_ffmpeg.OnPlayStateChangeListener
import com.pcyfox.lib_ffmpeg.PlayState
import com.pcyfox.module_recoder.audio.AudioRecorder
import com.pcyfox.module_recoder.audio.MediaConstants
import com.pcyfox.module_recoder.audio.RecorderContract
import java.io.File
import java.lang.Exception

class RecorderView : RelativeLayout {
    constructor(context: Context?, attrs: AttributeSet?) : super(context, attrs);
    constructor(context: Context?, attrs: AttributeSet?, defStyleAttr: Int) : super(
        context,
        attrs
    )

    constructor(
        context: Context?,
        attrs: AttributeSet?,
        defStyleAttr: Int,
        defStyleRes: Int
    ) : super(context, attrs, defStyleAttr, defStyleRes)

    private val sv = SurfaceView(context)
    private var videoPath = ""
    private var audioPath = ""
    private val TAG = "RecorderView"
    private val ffPlayer: FFPlayer = FFPlayer(hashCode())
    private val audioRecorder = AudioRecorder.getInstance()
    private var avRecorderCallback: AVRecorderCallback? = null
    override fun onFinishInflate() {
        super.onFinishInflate()
        addView(sv)
        sv.layoutParams.width = ViewGroup.LayoutParams.MATCH_PARENT
        sv.layoutParams.height = ViewGroup.LayoutParams.MATCH_PARENT

        audioRecorder.setRecorderCallback(object : RecorderContract.RecorderCallback {
            override fun onPrepareRecord() {
                avRecorderCallback?.onPauseRecord()
            }

            override fun onStartRecord(output: File?) {
                avRecorderCallback?.onStartRecord(output, File(videoPath))
            }

            override fun onPauseRecord() {
                avRecorderCallback?.onPauseRecord()
            }

            override fun onRecordProgress(mills: Long, amp: Int) {
                avRecorderCallback?.onRecordProgress(mills, amp)
            }

            override fun onStopRecord(output: File?) {
                avRecorderCallback?.onStopRecord(output, File(videoPath))
            }

            override fun onError(throwable: Exception?) {
                avRecorderCallback?.onError(throwable)
            }
        })
    }


    fun setOnStateChangeListener(listener: OnPlayStateChangeListener) {
        ffPlayer.setOnPlayStateChangeListener(listener)
    }

    fun prepareRecorder(videoPath: String?, audioPath: String? = "") {
        Log.d(TAG, "prepareRecorder() called with: videoPath = $videoPath, audioPath = $audioPath")
        if (!videoPath.isNullOrEmpty()) {
            this.videoPath = videoPath
            ffPlayer.prepareRecorder(videoPath);
        }
        if (!audioPath.isNullOrEmpty()) {
            this.audioPath = audioPath
            audioRecorder.prepare(
                audioPath,
                MediaConstants.DEFAULT_CHANNEL_COUNT,
                MediaConstants.DEFAULT_RECORD_SAMPLE_RATE,
                MediaConstants.DEFAULT_RECORD_ENCODING_BITRATE
            )
        }
    }

    fun startRecord(): Boolean {
        Log.d(TAG, "startRecord() called with: videoPath = $videoPath, audioPath = $audioPath")
        if (TextUtils.isEmpty(videoPath) || TextUtils.isEmpty(audioPath)) {
            return false
        }
        if (ffPlayer.playState != PlayState.STARTED) {
            Log.e(TAG, "startRecord() called fail,state=${ffPlayer.playState}")
            return false
        }
        audioRecorder.startRecording()
        return ffPlayer.startRecord() > 0
    }


    fun startRecordVideo(): Boolean {
        Log.d(TAG, "startRecordVideo() called")
        if (TextUtils.isEmpty(videoPath)) {
            return false
        }
        if (ffPlayer.playState != PlayState.STARTED) {
            Log.d(TAG, "startRecordVideo() called fail,state=${ffPlayer.playState}")
            return false
        }
        return ffPlayer.startRecord() > 0
    }


    fun pauseRecord(): Int {
        audioRecorder.pauseRecording()
        return ffPlayer.pauseRecord()
    }


    fun resumeRecord(): Int {
        audioRecorder.startRecording()
        return ffPlayer.resumeRecord()
    }

    fun stopRecord(): Int {
        audioRecorder.stopRecording()
        return ffPlayer.stopRecord()
    }


    fun setResource(url: String, isOnlyRecordeVideo: Boolean = false) {
        Log.d(
            TAG,
            "setResource() called with: url = $url, isOnlyRecordeVideo = $isOnlyRecordeVideo"
        )
        ffPlayer.run {
            if (setResource(url) > 0) {
                config(sv, sv.width, sv.height, isOnlyRecordeVideo)
            } else {
                Log.e(TAG, "start() called,open url=$url fail!")
            }
        }
    }

    fun start(): Boolean {
        return ffPlayer.start() > 0
    }

    fun play(): Boolean {
        return ffPlayer.play() > 0
    }

    fun stop(): Boolean {
        stopRecord()
        return ffPlayer.stop() > 0
    }

    fun release(){
       ffPlayer.release()
    }
    fun pause(): Boolean {
        pauseRecord()
        return ffPlayer.pause() > 0
    }

    fun resume(): Boolean {
        return ffPlayer.resume() > 0
    }


    @WorkerThread
    fun mux(videoPath: String, audioPath: String, outPath: String): Boolean {
        Log.d(
            TAG,
            "mux() called with: videoPath = $videoPath, audioPath = $audioPath, outPath = $outPath"
        )
        return FFPlayer.mux(audioPath, videoPath, outPath) >= 0
    }


    @WorkerThread
    fun mux(outPath: String): Boolean {
        val vf = File(videoPath)
        val af = File(audioPath)
        val of = File(outPath)
        return if (vf.isFile && vf.canRead() && af.isFile && af.canRead() && of.isFile) {
            mux(videoPath, audioPath, outPath)
        } else {
            Log.e(TAG, "mux fail,access file error!")
            false
        }
    }


}