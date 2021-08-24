package com.pcyfox.module_recoder.ui.activity

import android.os.Bundle
import android.os.Environment
import android.util.Log
import android.view.View
import androidx.appcompat.app.AppCompatActivity
import com.blankj.utilcode.constant.PermissionConstants
import com.blankj.utilcode.util.PermissionUtils
import com.blankj.utilcode.util.TimeUtils
import com.pcyfox.lib_ffmpeg.PlayState
import com.pcyfox.module_recoder.R
import kotlinx.android.synthetic.main.recorder_activity_test_record_multi_video.*
import java.io.File

class RecordeMulitVideoTestActivity : AppCompatActivity() {
    private val TAG = "TestActivity"
    private val url_1 = "rtsp://admin:taike@2020@192.168.16.219:554/Streaming/Channels/102?transportmode=unicast&profile=Profile_2"
    private val url_2 = "rtsp://admin:taike@2020@192.168.16.219:554/Streaming/Channels/102?transportmode=unicast&profile=Profile_2"

    //  private val url = "/storage/emulated/0/test/20210406_16_26_12/1/out.mp4"
    private val storeDir = Environment.getExternalStorageDirectory().absolutePath + "/test/"

    private var recordCount = 0
    private var startTime = ""
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.recorder_activity_test_record_multi_video)
        PermissionUtils.permission(PermissionConstants.MICROPHONE, PermissionConstants.STORAGE)
            .request()
        initView()
        val store = File(storeDir)
        if (!store.exists()) {
            store.mkdirs()
        }
        startTime = TimeUtils.getNowString(TimeUtils.getSafeDateFormat("yyyyMMdd_HH_mm_ss"))
    }

    private fun initView() {
        rv_record_2.run {
            setOnStateChangeListener { state ->
                Log.d(TAG, "initView 2() called with: state = $state")
                when (state) {
                    PlayState.PREPARED -> {
                        start()
                        //play()
                    }
                    else -> {
                    }
                }
            }
        }
        rv_record_1.run {
            setOnStateChangeListener { state ->
                Log.d(TAG, "initView 1() called with: state = $state")
                when (state) {
                    PlayState.PREPARED -> {
                        start()
                        //play()
                    }
                    else -> {
                    }
                }
            }
        }

    }

    private fun getRootPath(): File {
        val rp = "$storeDir$startTime/$recordCount/"
        val rf = File(rp)
        if (!rf.exists()) {
            rf.mkdirs()
        }
        return rf
    }

    private fun getFile(fileName: String): File {
        val f = File(getRootPath().absolutePath + "/$fileName")
        if (f.exists()) {
            f.delete()
        } else {
            f.createNewFile()
        }
        return f
    }

    private fun getVideoFile(): File {
        return getFile("video.h264")
    }

    private fun getAudioFile(): File {
        return getFile("audio.aac")
    }

    private fun getOutFile(): File {
        return getFile("out.mp4")
    }


    fun onClick(v: View) {
        when (v.id) {
            R.id.btn_start -> {
                recordCount++
                rv_record_1.prepareRecorder(getVideoFile().absolutePath)
                rv_record_1.setResource(url_1, true)

                recordCount++
                rv_record_2.prepareRecorder(getVideoFile().absolutePath)
                rv_record_2.setResource(url_2, true)
            }

            R.id.btn_stop -> {
                rv_record_1.stop()
                rv_record_2.stop()
            }
            R.id.btn_pause -> {
                rv_record_1.pause()
                rv_record_2.pause()
            }
            R.id.btn_resume -> {
                rv_record_1.resume()
                rv_record_2.resume()
            }

            R.id.btn_start_record -> {
                rv_record_1.startRecordVideo()
                rv_record_2.startRecordVideo()
            }
            R.id.btn_pause_record -> {
                rv_record_1.pauseRecord()
                rv_record_2.pauseRecord()
            }
            R.id.btn_resume_record -> {
                rv_record_1.resumeRecord()
                rv_record_2.resumeRecord()
            }
            R.id.btn_stop_record -> {
                rv_record_1.stopRecord()
                rv_record_2.stopRecord()
            }
            R.id.btn_mux -> {
                mux()
            }
            else -> {
            }
        }


    }

    private fun mux() {

    }

    override fun onDestroy() {
        rv_record_1.release()
        rv_record_2.release()
        super.onDestroy()

    }
}