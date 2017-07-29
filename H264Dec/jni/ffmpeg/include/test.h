#ifndef TEST_h
#define TEST_h

#ifdef __cplusplus
extern "C"
{
#endif
JNIEnv *get_jni_env(void);
int recivertmp(int,char*);
int flv_video(char *,int ,int );
int decode_init();
int decode(int , char* ,int );
#ifdef __cplusplus
}
#endif

#endif

