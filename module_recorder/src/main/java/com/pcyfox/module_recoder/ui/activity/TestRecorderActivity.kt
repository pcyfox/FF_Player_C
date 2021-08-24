package com.pcyfox.module_recoder.ui.activity

import android.media.MediaFormat
import android.os.Bundle
import android.os.Environment
import android.util.Log
import android.view.View
import android.widget.EditText
import androidx.appcompat.app.AppCompatActivity
import com.blankj.utilcode.constant.PermissionConstants
import com.blankj.utilcode.util.PermissionUtils
import com.blankj.utilcode.util.TimeUtils
import com.blankj.utilcode.util.ToastUtils
import com.pcyfox.lib_ffmpeg.PlayState
import com.pcyfox.module_recoder.R
import com.pcyfox.module_recoder.ui.view.AVRecorderCallback
import kotlinx.android.synthetic.main.recorder_activity_test.*
import java.io.File

class TestRecorderActivity : AppCompatActivity() {
    companion object {
        private val TAG = "TestActivity"
        private const val KEY_URL = "KEY_URL"
        private val storeDir = Environment.getExternalStorageDirectory().absolutePath + "/test/"
    }

    /**
     *
     */
    // private var url = "/storage/emulated/0/test/20210602_15_23_31/1/video.h264"
    //private var url = "rtsp://admin:taike@2020@192.168.28.12:554/h264/ch01/main/av_stream"
    private var url =
        "rtsp://admin:taike@2020@192.168.16.217:554/Streaming/Channels/101?transportmode=unicast&profile=Profile_1"
    // private var url = "rtmp://58.200.131.2:1935/livetv/hunantv"

    //private var url = "/storage/emulated/0/test.mp4"
    //private var url = "/storage/emulated/0/video.h264"
    //private val url = "/storage/emulated/0/test/20210602_15_23_31/1/out.mp4"
    private var recordCount = 0
    private var startTime = "2021"
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.recorder_activity_test)
        PermissionUtils.permission(PermissionConstants.MICROPHONE, PermissionConstants.STORAGE)
            .request()
        initView()
        val store = File(storeDir)
        if (!store.exists()) {
            store.mkdirs()
        }
        val inputUrl = intent?.getStringExtra(KEY_URL)
        if (!inputUrl.isNullOrEmpty()) {
            url = inputUrl
        }
        startTime = TimeUtils.getNowString(TimeUtils.getSafeDateFormat("yyyyMMdd"))

    }

    private fun initView() {
        et_url.setText(url)
        btn_play.postDelayed({
            btn_play.performClick()
        }, 200)
        rv_record.run {
            avRecorderCallback = object : AVRecorderCallback {
                override fun onPrepareRecord() {
                }

                override fun onStartRecord(audio: File?, video: File?) {
                }

                override fun onPauseRecord() {
                }

                override fun onRecordProgress(mills: Long, amp: Int) {
                    //Log.d(TAG, "onRecordProgress() called with: mills = $mills, amp = $amp")
                }

                override fun onStopRecord(audio: File?, video: File?) {
                }

                override fun onError(throwable: Exception?) {
                }
            }
            setOnStateChangeListener { state ->
                Log.d(TAG, "initView() called with: state = $state")
                when (state) {
                    PlayState.PREPARED -> {
                        play()
                    }
                    PlayState.STOPPED -> {
                        if (isFinishing || isDestroyed) {
                            rv_record.release()
                        }
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

    private fun getFile(fileName: String, isDeleteOld: Boolean = true): File {
        val f = File(getRootPath().absolutePath + "/$fileName")
        if (f.exists() && isDeleteOld) {
            f.delete()
        }
        f.createNewFile()
        return f
    }

    private fun getVideoFile(isDeleteOld: Boolean = true): File {
        return getFile("video.h264", isDeleteOld)
    }

    private fun getAudioFile(isDeleteOld: Boolean = true): File {
        return getFile("audio.aac", isDeleteOld)
    }

    private fun getOutFile(): File {
        return getFile("out.mp4")
    }


    fun onClick(v: View) {
        rv_record.run {
            url = et_url.text.toString()
            when (v.id) {
                R.id.btn_play -> {
                    recordCount++
                    setResource(url)
                }
                R.id.btn_start -> {
                    recordCount++
                    setResource(url)
                }

                R.id.btn_stop -> {
                    pb_recorde.visibility = View.GONE
                    stop()
                }
                R.id.btn_pause -> {
                    pause()
                    visibility = View.INVISIBLE
                }
                R.id.btn_resume -> {
                    visibility = View.VISIBLE
                    resume()
                }

                R.id.btn_start_record -> {
                    prepareRecorder(getVideoFile().absolutePath, getAudioFile().absolutePath)
                    postDelayed({
                        startRecord()
                        pb_recorde.visibility = View.VISIBLE
                    }, 100)
                }
                R.id.btn_pause_record -> {
                    pauseRecord()
                }
                R.id.btn_resume_record -> {
                    resumeRecord()
                    pb_recorde.visibility = View.VISIBLE
                }
                R.id.btn_stop_record -> {
                    stopRecord()
                    pb_recorde.visibility = View.GONE
                }
                R.id.btn_mux -> {
                    mux()
                }

                R.id.btn_release -> {
                    release()
                }
                else -> {
                }
            }
        }


    }


    private fun mux() {
        MediaFormat.MIMETYPE_AUDIO_AAC
        Thread {
            Log.d(TAG, "mux() called start")
            val ret = rv_record.mux(
                getVideoFile(false).absolutePath,
                getAudioFile(false).absolutePath,
                getOutFile().absolutePath
            ) {
                Log.d(TAG, "mux() ------------------------------progress=${it * 100}")
            }
            if (ret) {
                Log.d(TAG, "mux() called over!")
                ToastUtils.showShort("MUX Success!!")
            } else {
                ToastUtils.showShort("MUX Fail!")
                Log.e(TAG, "mux() called fail!")
            }
        }.start()
    }

    override fun onStop() {
        super.onStop()
    }

    override fun onDestroy() {
        super.onDestroy()
        rv_record.stop()
        rv_record.release()
        rv_record.release()
    }

}