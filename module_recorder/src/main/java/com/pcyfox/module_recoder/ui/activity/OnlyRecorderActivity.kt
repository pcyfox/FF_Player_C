package com.pcyfox.module_recoder.ui.activity

import android.annotation.SuppressLint
import android.graphics.Color
import android.os.Bundle
import android.os.Environment
import android.os.SystemClock
import android.util.Log
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import com.blankj.utilcode.constant.PermissionConstants
import com.blankj.utilcode.util.FileUtils
import com.blankj.utilcode.util.PermissionUtils
import com.blankj.utilcode.util.TimeUtils
import com.blankj.utilcode.util.ToastUtils
import com.pcyfox.lib_ffmpeg.AVMuxer
import com.pcyfox.lib_ffmpeg.FFPlayer
import com.pcyfox.lib_ffmpeg.PlayState
import com.pcyfox.lib_ffmpeg.RecordState
import com.pcyfox.module_recoder.R
import com.pcyfox.module_recoder.audio.MediaConstants
import com.pcyfox.module_recoder.audio.AudioRecorder
import kotlinx.android.synthetic.main.recorde_activity_media_recoder.*
import java.io.File

class OnlyRecorderActivity : AppCompatActivity() {
    private val TAG = "MediaRecorderActivity"
    private var recordCount = 0
    private var startTime = ""
    private var isStartedRecord = false
    private var ffPlayer: FFPlayer? = null
    private var isMuxAVWorking = false

    //private val url = "http://vfx.mtime.cn/Video/2019/03/13/mp4/190313094901111138.mp4"
    //private val url = "/storage/emulated/0/test/20210406_16_26_12/1/out.mp4"
    private var url =
        "rtsp://admin:taike@2020@192.168.10.54:554/Streaming/Channels/101?transportmode=unicast&profile=Profile_1"

    private val storeDir = Environment.getExternalStorageDirectory().absolutePath + "/test/"
    private val audioRecorder = AudioRecorder.getInstance()
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.recorde_activity_media_recoder)
        PermissionUtils.permission(PermissionConstants.MICROPHONE, PermissionConstants.STORAGE)
            .request()
        initView()
        if (!File(storeDir).exists()) {
            File(storeDir).mkdir()
        }
        startTime = TimeUtils.getNowMills().toString()
    }


    private fun initView() {
        recorder_url.setText(url)
        recorder_btn_control.postDelayed({
            //   recorder_btn_control.performClick()
        }, 3 * 1000)
    }

    override fun onPostResume() {
        super.onPostResume()
    }

    override fun onPostCreate(savedInstanceState: Bundle?) {
        super.onPostCreate(savedInstanceState)
        ffPlayer = FFPlayer(hashCode())
        ffPlayer?.setOnPlayStateChangeListener {
            Log.d(TAG, "onStateChanged() called playState=$it")
            when (it) {
                PlayState.PREPARED -> ffPlayer?.run {
                    ffPlayer?.start()
                }
                PlayState.STARTED -> {
                    if (isStartedRecord) {
                        ffPlayer?.startRecord()
                    }
                }

                else -> {
                }
            }
        }
        ffPlayer?.setOnRecordStateChangeListener {
            Log.d(TAG, "onRecordStateChanged() called:$it")
            when (it) {
                RecordState.RECORD_PREPARED -> {
                    prepareAudio()
                    audioRecorder.startRecording()
                }
                RecordState.RECORDING -> {}
                RecordState.RECORD_ERROR -> {}
                RecordState.RECORDER_RELEASE -> {}
                RecordState.RECORD_STOP -> {}
                RecordState.RECORD_START -> {
                }
                RecordState.RECORD_RESUME -> {}
                RecordState.RECORD_PAUSE -> {}
                RecordState.UN_START_RECORD -> {}
            }

        }
    }

    var videoPath = ""
    var audioPath = ""

    fun mux(v: View) {
        if (!File(audioPath).exists() || !File(videoPath).exists()) {
            return
        }
        if (isMuxAVWorking) {
            ToastUtils.showLong("有合成任务正在进行中！")
            return
        }
        if (isStartedRecord) {
            ToastUtils.showLong("正在录制中！！")
            return
        }
        recorder_btn_mux.text = "合成中"
        Thread({
            isMuxAVWorking = true
            val destFile = File(storeDir, "test.mp4")
            if (destFile.exists()) {
                Log.d(TAG, "mux: delete file=${destFile.absolutePath}")
                destFile.delete()
            }
            Thread.sleep(100)
            val startMuxTime = SystemClock.uptimeMillis()
            AVMuxer.mux(audioPath, videoPath, destFile.absolutePath, null)
            val time = (SystemClock.uptimeMillis() - startMuxTime) / 1000.0
            ToastUtils.showLong("合成结束! coast time=" + time + "s")
            Log.d(
                TAG,
                "mux over:coastTime=$time,videoPath=${videoPath},out path=" + destFile.absolutePath
            )
            isMuxAVWorking = false
            runOnUiThread {
                recorder_btn_mux.text = "合成"
            }
        }, "mux thread").start()
    }

    @SuppressLint("SdCardPath")
    fun onStartOrStop(v: View) {
        if (!isStartedRecord) {
            isStartedRecord = true
            recorder_btn_control.setBackgroundColor(Color.RED)
            recorder_btn_control.text = "停止"
            prepareVideo()
            recordCount++
        } else {
            ffPlayer?.stop()
            audioRecorder.stopRecording()
            isStartedRecord = false
            recorder_btn_control.setBackgroundColor(Color.WHITE)
            recorder_btn_control.text = "开始"
        }

    }

    private fun prepareVideo() {
        Log.d(TAG, "prepareVideo() called")
        val videoSaveDir = File(storeDir + "h264/")
        if (!videoSaveDir.exists()) {
            videoSaveDir.mkdir()
        } else {
            FileUtils.deleteAllInDir(videoSaveDir)
        }

        videoPath =
            videoSaveDir.absolutePath + "/$startTime _$recordCount.h264".replace(" ", "-")
        File(videoPath).createNewFile()
        ffPlayer?.run {
            if (setResource(recorder_url.text.toString()) > 0) {
                config(
                    recorder_rtp_view,
                    recorder_rtp_view.width,
                    recorder_rtp_view.height,
                    true
                )
                prepareRecorder(videoPath)
            }
        }
    }


    private fun prepareAudio() {
        Log.d(TAG, "prepareAudio() called")
        val audioSaveDir = File(storeDir + "aac/")
        if (!audioSaveDir.exists()) {
            audioSaveDir.mkdir()
        } else {
            FileUtils.deleteAllInDir(audioSaveDir)
        }

        audioPath =
            audioSaveDir.absolutePath + "/$startTime _$recordCount.aac".replace(" ", "-")
        File(audioPath).createNewFile()
        audioRecorder.prepare(
            audioPath,
            MediaConstants.DEFAULT_CHANNEL_COUNT,
            MediaConstants.DEFAULT_RECORD_SAMPLE_RATE,
            MediaConstants.DEFAULT_RECORD_ENCODING_BITRATE
        )
    }

}