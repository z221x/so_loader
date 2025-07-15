#include <jni.h>
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, "Test", __VA_ARGS__)
extern "C"
JNIEXPORT void JNICALL
Java_com_nobody_demo_MainActivity_test(JNIEnv *env, jobject thiz) {
   LOGI("Native Call Success");
}
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    if (vm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    LOGI("JNI_OnLoad Success");
    return JNI_VERSION_1_6;
}
