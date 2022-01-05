//
// Created by LN on 2021/8/20.
//

#include <jni.h>
#include <VMLoader.h>

#ifdef __cplusplus
extern "C" {
#include <libavcodec/jni.h>
#include "AndroidLog.h"
#endif
#ifdef __cplusplus
}
#endif

jint JNI_OnLoad(JavaVM *jvm, void *reserved) {
    JNIEnv *env = NULL;
    if (jvm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        LOGE("--------JavaVm load fail!--------");
        av_jni_set_java_vm(jvm, NULL);
        return JNI_ERR;
    }
    vm = jvm;
    LOGI("--------JavaVm load success!--------");
    return JNI_VERSION_1_6;
}
