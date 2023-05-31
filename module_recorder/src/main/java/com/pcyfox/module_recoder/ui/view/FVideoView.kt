package com.pcyfox.module_recoder.ui.view

import android.content.Context
import android.text.TextUtils
import android.util.AttributeSet
import android.util.Log
import android.view.SurfaceView
import android.view.ViewGroup
import android.widget.RelativeLayout
import androidx.annotation.WorkerThread
import com.pcyfox.lib_ffmpeg.*
import com.pcyfox.module_recoder.audio.AudioRecorder
import com.pcyfox.module_recoder.audio.MediaConstants
import com.pcyfox.module_recoder.audio.RecorderContract
import java.io.File
import java.lang.Exception

class FVideoView : RelativeLayout {
    constructor(context: Context?, attrs: AttributeSet?) : super(context, attrs);
    constructor(context: Context?, attrs: AttributeSet?, defStyleAttr: Int) : super(
        context,
        attrs
    )

    private var isVisibleChanged = false
    private val sv = SurfaceView(context)
    private var videoPath = ""
    private var audioPath = ""
    private val TAG = "RecorderView"
    private val ffPlayer: FFPlayer = FFPlayer(hashCode(), true)
    private val audioRecorder = AudioRecorder.getInstance()
    private var isNeedRelease = false
    var avRecorderCallback: AVRecorderCallback? = null
    var listener: OnPlayStateChangeListener? = null
    var isOnlyRecordeVideo = false
    override fun onFinishInflate() {
        super.onFinishInflate()
        addView(sv)
        sv.layoutParams.width = ViewGroup.LayoutParams.MATCH_PARENT
        sv.layoutParams.height = ViewGroup.LayoutParams.MATCH_PARENT
    }


    fun setOnStateChangeListener(listener: OnPlayStateChangeListener?) {
        this.listener = listener
        ffPlayer.setOnPlayStateChangeListener { state ->
            if (isNeedRelease && state == PlayState.STOPPED) {
                doRelease()
            }
            listener?.onStateChange(state)
        }
    }

    fun getPlayState() = ffPlayer.playState
    fun getRecorderState() = ffPlayer.recodeState

    fun prepareRecorder(videoPath: String?, audioPath: String? = "") {
        Log.d(TAG, "prepareRecorder() called with: videoPath = $videoPath, audioPath = $audioPath")

        ffPlayer.setOnRecordStateChangeListener {
            Log.d(TAG, "record StateChange state=$it")
            if (isOnlyRecordeVideo) {
                return@setOnRecordStateChangeListener
            }
            when (it) {
                RecordState.RECORD_PREPARED-> audioRecorder.startRecording()
                RecordState.RECORD_PAUSE -> audioRecorder.pauseRecording()
                RecordState.RECORD_STOP -> audioRecorder.stopRecording()
                RecordState.RECORD_RESUME -> audioRecorder.startRecording()
                else -> {
                }
            }
        }

        if (!videoPath.isNullOrEmpty()) {
            this.videoPath = videoPath
            ffPlayer.prepareRecorder(videoPath)
        }
        if (!audioPath.isNullOrEmpty()) {
            this.audioPath = audioPath
            File(audioPath).run {
                if (!exists()) {
                    createNewFile()
                }
            }
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
        if (ffPlayer.playState != PlayState.STARTED) {
            Log.e(TAG, "startRecord() called fail,state=${ffPlayer.playState}")
            return false
        }

        if (videoPath.isEmpty() && audioPath.isEmpty()) {
            return false
        }

        if (audioPath.isNotEmpty()) {
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


        if (videoPath.isNotEmpty()) {
            if (ffPlayer.recodeState == RecordState.RECORD_STOP) {
                ffPlayer.prepareRecorder(videoPath)
            }

            ffPlayer.startRecord() > 0
        }

        return true
    }


    fun startRecordVideo(): Boolean {
        return startRecord()
    }


    fun pauseRecord(): Int {
        return ffPlayer.pauseRecord()
    }

    fun resumeRecord(): Int {
        return ffPlayer.resumeRecord()
    }

    fun stopRecord(): Int {
        return ffPlayer.stopRecord()
    }


    fun setResource(url: String, isJustRecord: Boolean = false) {
        isNeedRelease = false
        Log.d(
            TAG,
            "setResource() called with: url = $url, isJustRecord= $isJustRecord"
        )
        ffPlayer.run {
            if (setResource(url) > 0) {
                config(sv, sv.width, sv.height, isJustRecord)
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

    fun release() {
        if (getPlayState() == PlayState.STOPPED) {
            doRelease()
        } else {
            stop()
            postDelayed({
                doRelease()
            }, 100)
        }
    }


    private fun doRelease() {
        Log.w(TAG, "doRelease() called")
        ffPlayer.release()
        setOnStateChangeListener(null)
        removeAllViews()
    }

    fun pause(): Boolean {
        return ffPlayer.pause() > 0
    }

    fun resume(): Boolean {
        if (isVisibleChanged) {
            isVisibleChanged = false
        }
        return ffPlayer.resume() > 0
    }

    fun resumeWindow() {
        ffPlayer.resumeWindow()
    }

    @WorkerThread
    fun mux(
        videoPath: String,
        audioPath: String,
        outPath: String,
        onMuxProgressListener: OnMuxProgressListener? = null
    ): Boolean {
        Log.d(
            TAG,
            "mux() called with: videoPath = $videoPath, audioPath = $audioPath, outPath = $outPath"
        )

        return AVMuxer.mux(audioPath, videoPath, outPath, onMuxProgressListener) >= 0
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

    override fun onVisibilityAggregated(isVisible: Boolean) {
        Log.d(TAG, "onVisibilityAggregated() called with: isVisible = $isVisible")
        isVisibleChanged = true
        super.onVisibilityAggregated(isVisible)
    }


}