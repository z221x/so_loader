#include "SoLoader.h"
#include "ElfReader.h"
JNIEnv* env_;
jobject application;
JavaVM*  javaVm_;
SoLoader::SoLoader() {
   reader=new ElfReader();
}
SoLoader::~SoLoader() {
    reader=new ElfReader();
}
void SoLoader::loadlibiary(const char *path) {
    nativeload(path,LOADLIBIARY);
}

void SoLoader::load(const char *path) {
    nativeload(path,LOAD);
}

void SoLoader::nativeload(const char *path, int flag) {
    if(!reader->Open(path,flag))
    {
        LOGE("SoLoader","Open failed");
        return;
    }
    if(!reader->Read())
    {
        LOGE("SoLoader","Read failed");
        return;
    }
    soinfo * si_ = reader->Load();
    if(!si_)
    {
        LOGE("SoLoader","Load failed");
        return;
    }
    if(!si_->prelink_image())
    {
        LOGE("SoLoader","prelink_image failed");
        return;
    }
    if(!si_->link_image())
    {
        LOGE("SoLoader","link_image failed");
        return;
    }
    si_->call_constructors();
    si_->call_JNI_Onload();

}
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    if (vm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    javaVm_=vm;
    return JNI_VERSION_1_6;
}
extern "C"
JNIEXPORT void JNICALL
Java_com_nobody_demo_MainActivity_soloader(JNIEnv *env, jobject thiz, jobject context) {
    env_ = env;
    application = context;
    SoLoader loader=SoLoader();
    loader.loadlibiary("libtest.so");

}
