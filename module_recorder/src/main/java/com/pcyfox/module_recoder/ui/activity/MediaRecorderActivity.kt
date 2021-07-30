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
import com.pcyfox.lib_ffmpeg.FFPlayer
import com.pcyfox.lib_ffmpeg.PlayState
import com.pcyfox.module_recoder.R
import com.pcyfox.module_recoder.audio.MediaConstants
import com.pcyfox.module_recoder.audio.AudioRecorder
import kotlinx.android.synthetic.main.recorde_activity_media_recoder.*
import java.io.File

class MediaRecorderActivity : AppCompatActivity() {
    private val TAG = "MediaRecorderActivity"
    private var recordCount = 0
    private var startTime = ""
    private var isStartedRecord = false
    private var ffPlayer: FFPlayer? = null
    private var isMuxAVWorking = false

    //private val url = "rtsp://admin:taike@2020@192.168.28.12:554/h264/ch01/main/av_stream"
    private val url = "/storage/emulated/0/test/20210406_16_26_12/1/out.mp4"

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
        recorder_tip.setText(url)
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
                    play()
                }

                PlayState.STARTED -> {
                    if (isStartedRecord) {
                        ffPlayer?.startRecord()
                        audioRecorder.startRecording()
                    }
                }

                else -> {
                }
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
            FFPlayer.mux(audioPath, videoPath, destFile.absolutePath,null)
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
        val videoSaveDir = File(storeDir + "h264/")
        val audioSaveDir = File(storeDir + "aac/")

        if (!isStartedRecord) {
            if (!videoSaveDir.exists()) {
                videoSaveDir.mkdir()
            } else {
                FileUtils.deleteAllInDir(videoSaveDir)
            }
            if (!audioSaveDir.exists()) {
                audioSaveDir.mkdir()
            } else {
                FileUtils.deleteAllInDir(audioSaveDir)
            }
            isStartedRecord = true
            recorder_btn_control.setBackgroundColor(Color.RED)
            recorder_btn_control.text = "停止"
            videoPath =
                videoSaveDir.absolutePath + "/$startTime _$recordCount.h264".replace(" ", "-")
            audioPath =
                audioSaveDir.absolutePath + "/$startTime _$recordCount.aac".replace(" ", "-")
            File(audioPath).createNewFile()
            File(videoPath).createNewFile()
            ffPlayer?.run {
                if (setResource(recorder_tip.text.toString()) > 0) {
                    prepareRecorder(videoPath)
                    config(
                        recorder_rtp_view,
                        recorder_rtp_view.width,
                        recorder_rtp_view.height,
                        true
                    )
                }
            }

            audioRecorder.prepare(
                audioPath,
                MediaConstants.DEFAULT_CHANNEL_COUNT,
                MediaConstants.DEFAULT_RECORD_SAMPLE_RATE,
                MediaConstants.DEFAULT_RECORD_ENCODING_BITRATE
            )
            recordCount++
        } else {
            ffPlayer?.stop()
            audioRecorder.stopRecording()
            isStartedRecord = false
            recorder_btn_control.setBackgroundColor(Color.WHITE)
            recorder_btn_control.text = "开始"
        }

    }

}