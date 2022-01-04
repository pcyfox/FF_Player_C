package com.pcyfox.module_recoder.ui.activity

import android.content.Intent
import androidx.appcompat.app.AppCompatActivity
import android.os.Bundle
import android.view.View
import com.pcyfox.module_recoder.R

class RMainActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.recorde_activity_main)
    }

    fun onClick(v: View) {
        when (v.id) {
            R.id.btn_test_play -> {
                startActivity(Intent(this, TestRecorderActivity::class.java))
            }
            R.id.btn_test_multi_video_record -> {
                startActivity(Intent(this, RecordeMultiVideoTestActivity::class.java))
            }
            R.id.btn_local_video_play_test -> startActivity(
                Intent(
                    this, LocalVideoPlayTestActivity::class.java
                )
            )
        }
    }
}