#ifdef __cplusplus
extern "C" {
#endif
#include"common.h"
#include"opengles_display.h"
#include <libavcodec/avcodec.h>

#include <stdio.h>
#include<string.h>
#include<stdlib.h>
#include "rtmp_sys.h"
#include "log.h"
#include "test.h"


#ifdef ANDROID
#include <jni.h>
#include <android/log.h>
#define LOGE(format, ...)  __android_log_print(ANDROID_LOG_ERROR, "(>_<)", format, ##__VA_ARGS__)
#define LOGI(format, ...)  __android_log_print(ANDROID_LOG_INFO,  "(^_^)", format, ##__VA_ARGS__)
#else
#define LOGE(format, ...)  printf("(>_<) " format "\n", ##__VA_ARGS__)
#define LOGI(format, ...)  printf("(^_^) " format "\n", ##__VA_ARGS__)
#endif
#define __STDC_CONSTANT_MACROS


int InitSockets()
{
#ifdef WIN32
	WORD version;
	WSADATA wsaData;
	version = MAKEWORD(1, 1);
	return (WSAStartup(version, &wsaData) == 0);
#endif
}

void CleanupSockets()
{
#ifdef WIN32
	WSACleanup();
#endif
}

#define TAG_TYPE_SCRIPT 18
#define TAG_TYPE_AUDIO  8
#define TAG_TYPE_VIDEO  9

typedef unsigned char byte;
typedef unsigned int uint;


FILE *fp_in;
FILE *fp_out=fopen("/sdcard/rtmptest.yuv","w+");
FILE *fp_h264=fopen("/sdcard/rtmph264.h264","w+");
int saveh264=0;
int savenum=0;

AVPacket avpkt;
AVFrame *orig=NULL;
static pthread_mutex_t nalu_mutex = PTHREAD_MUTEX_INITIALIZER;
//YuvBuf *yuvbuff=NULL;

static void ffmpeg_log_handler(
void *ptr, int  lev,const char *fmt,va_list args)
{
	int prio;

	switch(lev)
	{
		case AV_LOG_DEBUG:	prio = ANDROID_LOG_DEBUG;	break;
		case AV_LOG_INFO:	prio = ANDROID_LOG_INFO;	break;
		case AV_LOG_WARNING:	prio = ANDROID_LOG_WARN;	break;
		case AV_LOG_ERROR:	prio = ANDROID_LOG_ERROR;	break;
		case AV_LOG_FATAL:	prio = ANDROID_LOG_FATAL;	break;
		default:	prio = ANDROID_LOG_DEFAULT;	break;
	}

	__android_log_vprint(prio,LOG_TAG,fmt,args);
}
static const char *resultDescription(int result) {
	switch(result) {
		case 0:
			return "No errors.";

		case -1:
			return "Error -1";
	}
	return "Could not define result\n";
}
typedef struct
{
    char *data;
    int len;
    int num;
}Nalu;

Nalu Naludata;
#define FRAME_MAX_CNT 30
#define FRAME_MAX_LEN 300*1024

typedef struct __frame_data {
	char* abFrameData;
	int nFrameDataMaxSize;
}frame_data_t;

typedef struct __decoder_handle {
	frame_data_t frame[FRAME_MAX_CNT];
	int r_frame_idx;
	int w_frame_idx;
	int frame_cnt_unuse;
	pthread_mutex_t mutex;
}decoder_handle_t;

decoder_handle_t Nalulist;
static void initMutex()
{
	pthread_mutex_init(&Nalulist.mutex, NULL);
}


typedef struct _DecData{
	struct SwsContext *sws_ctx;
	AVCodecContext av_context;
	unsigned int packet_num;
	uint8_t *bitstream;
	int bitstream_size;
	uint64_t last_error_reported_time;
	bool_t first_image_decoded;
	bool_t isStoped;
}DecData;
static void ffmpeg_init(){
	static bool_t done=FALSE;
	av_log_set_callback(ffmpeg_log_handler);
	if (!done){
#ifdef FF_API_AVCODEC_INIT
		avcodec_init();
#endif
		avcodec_register_all();
		done=TRUE;
	}
}

typedef struct {
    byte Signature[3];
    byte Version;
    byte Flags;
    uint DataOffset;
} FLV_HEADER;

typedef struct {
    byte TagType;
    byte DataSize[3];
    byte Timestamp[3];
    uint Reserved;
} TAG_HEADER;


//reverse_bytes - turn a BigEndian byte array into a LittleEndian integer
uint reverse_bytes(byte *p, char c) {
    int r = 0;
    int i;
    for (i=0; i<c; i++)
        r |= ( *(p+i) << (((c-1)*8)-8*i));
    return r;
}

typedef struct AndroidDisplay{
	jobject android_video_window;
	jmethodID set_opengles_display_id;
	jmethodID request_render_id;
	struct opengles_display* ogl;
}AndroidDisplay;
AndroidDisplay *ad = NULL;

JavaVM* g_jvmInstance=0;
pthread_key_t jnienv_key;

FILE *fpvideo=fopen("/sdcard/rtmpvideo.h264","wb");

int recivertmp(int handler,char *url)
{

//init Naludata;
	memset(&Nalulist, 0, sizeof(decoder_handle_t));
	initMutex();
	for (int i = 0; i < FRAME_MAX_CNT; i++)
	{
		Nalulist.frame[i].abFrameData = (char*)malloc(FRAME_MAX_LEN);
		Nalulist.frame[i].nFrameDataMaxSize = -1;
	}
	Nalulist.r_frame_idx = 0;
	Nalulist.w_frame_idx = 0;
	Nalulist.frame_cnt_unuse = FRAME_MAX_CNT;

	InitSockets();

	double duration=-1;
	int nRead;
	//is live stream ?
	bool bLiveStream=true;


	int bufsize=1024*1024*10;
	char *buf=(char*)malloc(bufsize);
	memset(buf,0,bufsize);
	long countbufsize=0;
	Naludata.num=0;
	Naludata.data=NULL;

	//FILE *fp=fopen("/sdcard/rtmpreceive.flv","wb");
	//if (!fp){
	//	RTMP_LogPrintf("Open File Error.\n");
	//	__android_log_print(ANDROID_LOG_ERROR, "First", "Open File Error.");
	//	CleanupSockets();
	//	return -1;
	//}

	/* set log level */
	//RTMP_LogLevel loglvl=RTMP_LOGDEBUG;
	//RTMP_LogSetLevel(loglvl);


	RTMP *rtmp=RTMP_Alloc();
	RTMP_Init(rtmp);
	//set connection timeout,default 30s
	rtmp->Link.timeout=10;
	// HKS's live URL
	if(!RTMP_SetupURL(rtmp,url)) //rtmp://live.hkstv.hk.lxdns.com/live/hks
	{
		RTMP_Log(RTMP_LOGERROR,"SetupURL Err\n");
		__android_log_print(ANDROID_LOG_ERROR, "First", "SetupURL Err.");
		RTMP_Free(rtmp);
		CleanupSockets();
		return -1;
	}
	if (bLiveStream){
		rtmp->Link.lFlags|=RTMP_LF_LIVE;
	}

	//1hour
	RTMP_SetBufferMS(rtmp, 3600*1000);

	if(!RTMP_Connect(rtmp,NULL)){
		RTMP_Log(RTMP_LOGERROR,"Connect Err\n");
		__android_log_print(ANDROID_LOG_ERROR, "First", "Connect Err.");
		RTMP_Free(rtmp);
		CleanupSockets();
		return -1;
	}

	if(!RTMP_ConnectStream(rtmp,0)){
		RTMP_Log(RTMP_LOGERROR,"ConnectStream Err\n");
		__android_log_print(ANDROID_LOG_ERROR, "First", "ConnectStream Err.");
		RTMP_Close(rtmp);
		RTMP_Free(rtmp);
		CleanupSockets();
		return -1;
	}

	__android_log_print(ANDROID_LOG_ERROR, "First", "Begin");
	while(nRead=RTMP_Read(rtmp,buf,bufsize)){
		//if (saveh264==1&&savenum>0)
		//{
			//Y, U, V
		//	savenum--;
		//	fwrite(buf,1,nRead,fp_flv);
		//}


		flv_video(buf,nRead,handler);

		countbufsize+=nRead;
		//RTMP_LogPrintf("Receive: %5dByte, Total: %5.2fkB\n",nRead,countbufsize*1.0/1024);
	}

	pthread_mutex_lock(&(Nalulist.mutex));
	for (int i = 0; i < FRAME_MAX_CNT; i++)
	{
		free(Nalulist.frame[i].abFrameData);
		Nalulist.frame[i].abFrameData = NULL;
	}
	pthread_mutex_unlock(&(Nalulist.mutex));
	//if(fp)
		//fclose(fp);

	if(buf){
		free(buf);
	}


	if(rtmp){
		RTMP_Close(rtmp);
		RTMP_Free(rtmp);
		CleanupSockets();
		rtmp=NULL;
	}
	return 0;
}

int flv_video(char *buf,int nRead,int handler){
	char *flv="FLV";
	char *body;
	 TAG_HEADER *tagheader;
	 int size=0;
	 int naltail_pos=0;
	 int i = 0;
	 int nalu_type;

	 if(flv[0]==buf[0]&&flv[1]==buf[1]&&flv[2]==buf[2])
	 {
	 	 naltail_pos+=13;
	 }

	 do{
		 //remember free
		 tagheader=(TAG_HEADER*)malloc(sizeof(TAG_HEADER));
		 memset(tagheader,0,sizeof(TAG_HEADER));
		 memcpy(tagheader,buf,sizeof(TAG_HEADER));

		 naltail_pos+=11;
		int tagheader_datasize=tagheader->DataSize[0]*65536+tagheader->DataSize[1]*256+tagheader->DataSize[2];
		int tagheader_timestamp=tagheader->Timestamp[0]*65536+tagheader->Timestamp[1]*256+tagheader->Timestamp[2];

		char tagtype_str[10];

		//process tag by type
		switch (tagheader->TagType)
		{

			case TAG_TYPE_AUDIO:
			{
				naltail_pos+=tagheader_datasize;
				break;
			}
			case TAG_TYPE_VIDEO:
			{
				char videotag_str[100]={0};
				strcat(videotag_str,"| ");
				char tagdata_first_byte;
				tagdata_first_byte=buf[naltail_pos];
				int x=tagdata_first_byte&0xF0;
				x=x>>4;
				switch (x)
				{
				case 1:
				{
					strcat(videotag_str,"key frame  ");
					nalu_type=buf[naltail_pos+1]&0xff;
					if(nalu_type==1)
					{
						naltail_pos+=5;
						while(naltail_pos<nRead-5)
						{

							naltail_pos++;
							size=(buf[naltail_pos++]&0xff)*65536;
							size+=(buf[naltail_pos++]&0xff)*256;
							size+=buf[naltail_pos++]&0xff;
							nalu_type=buf[naltail_pos]&0xff;
							if(nalu_type==9||nalu_type==6)
							{
								naltail_pos+=size;
								continue;
							}
							body = (char*)malloc(size+4);
							memset(body,0,size+4);
							i=0;
							body[i++] = 0x00;// 1:Iframe  7:AVC
							body[i++] = 0x00;// AVC NALU
							body[i++] = 0x00;
							body[i++] = 0x01;
							memcpy(&body[i],buf+naltail_pos,size);

							pthread_mutex_lock(&(Nalulist.mutex));
							if (Nalulist.w_frame_idx >= FRAME_MAX_CNT)
								Nalulist.w_frame_idx = 0;
							int w_frame_idx = Nalulist.w_frame_idx;
							if (Nalulist.frame[w_frame_idx].abFrameData != NULL)
							{
								memcpy(Nalulist.frame[w_frame_idx].abFrameData, body, size+4);
								Nalulist.frame[w_frame_idx].nFrameDataMaxSize = size+4;

							}
							Nalulist.w_frame_idx++;
							Nalulist.frame_cnt_unuse--;
							pthread_mutex_unlock(&(Nalulist.mutex));

							__android_log_print(ANDROID_LOG_ERROR, "First", " Get Nalu.");
							//int error;
							//error=decode(handler,body,size+4);

							//fwrite(body,1,size+4,fpvideo);
							free(body);
							naltail_pos+=size;
							//if(buf[naltail_pos]==0x00&&buf[naltail_pos+1]==0x00&&buf[naltail_pos+2]==0x00&&buf[naltail_pos+3]==0x00)
							break;
						}

					}
					else
					{
						//AVCDecoderConfiguration
					}
					break;
				}


				case 2:
				{
					strcat(videotag_str,"inter frame");
					nalu_type=buf[naltail_pos+1]&0xff;
					if(nalu_type==1)
					{
						naltail_pos+=5;
						while(naltail_pos<nRead-5)
						{
							naltail_pos++;
							size=(buf[naltail_pos++]&0xff)*65536;
							size+=(buf[naltail_pos++]&0xff)*256;
							size+=buf[naltail_pos++]&0xff;
							nalu_type=buf[naltail_pos]&0xff;
							if(nalu_type==9||nalu_type==6)
							{
								naltail_pos+=size;
								continue;
							}
							body = (char*)malloc(size+4);
							memset(body,0,size+4);
							i=0;
							body[i++] = 0x00;// 1:Iframe  7:AVC
							body[i++] = 0x00;// AVC NALU
							body[i++] = 0x00;
							body[i++] = 0x01;
							memcpy(&body[i],buf+naltail_pos,size);

							pthread_mutex_lock(&(Nalulist.mutex));
							if (Nalulist.w_frame_idx >= FRAME_MAX_CNT)
								Nalulist.w_frame_idx = 0;
							int w_frame_idx = Nalulist.w_frame_idx;
							if (Nalulist.frame[w_frame_idx].abFrameData != NULL)
							{
								memcpy(Nalulist.frame[w_frame_idx].abFrameData, body, size+4);
								Nalulist.frame[w_frame_idx].nFrameDataMaxSize = size+4;

							}
							Nalulist.w_frame_idx++;
							Nalulist.frame_cnt_unuse--;
							pthread_mutex_unlock(&(Nalulist.mutex));

							__android_log_print(ANDROID_LOG_ERROR, "First", " Get Nalu.");
							//int error;
							//error=decode(handler,body,size+4);
							//fwrite(body,1,size+4,fpvideo);
							free(body);
							naltail_pos+=size;
							//if(buf[naltail_pos]==0x00&&buf[naltail_pos+1]==0x00&&buf[naltail_pos+2]==0x00&&buf[naltail_pos+3]==0x00)
							break;
						}
					}
					break;
				}

				case 3:break;
				case 4:break;
				case 5:break;
				default:break;
				}
				printf("%s\n",videotag_str);
				break;
				}
			default:
				naltail_pos+=tagheader_datasize;
				break;
		}
			naltail_pos+=4;
	}while(naltail_pos<nRead);

	free(tagheader);

    return 0;
}

void _android_key_cleanup(void *data)
{
	LOGV("_android_key_cleanup");
	JNIEnv* env=(JNIEnv*)pthread_getspecific(jnienv_key);
	if (env != NULL)
	{
		LOGV("Thread end, detaching jvm from current thread g_jvmInstance = %p",g_jvmInstance);
		g_jvmInstance->DetachCurrentThread();
		LOGV("Thread end, begin pthread_setspecific &jnienv_key = %p",&jnienv_key);
		pthread_setspecific(jnienv_key,NULL);
		LOGV("Thread end, end pthread_setspecific &jnienv_key = %p",&jnienv_key);
	}
}
JNIEnv *get_jni_env(void)
{
	JNIEnv *env=NULL;
	if (g_jvmInstance==NULL)
	{
		LOGE("NO jvm");
	}
	else
	{
		env=(JNIEnv*)pthread_getspecific(jnienv_key);
		if (env==NULL)
		{
			LOGE("AttachCurrentThread()");
			if (g_jvmInstance->AttachCurrentThread(&env,NULL)!=0)
			{
				LOGE("AttachCurrentThread() failed !");
				return NULL;
			}
			LOGE("AttachCurrentThread() success !");
			pthread_setspecific(jnienv_key,env);
		}
	}
	return env;
}


jint JNI_OnLoad(JavaVM* jvm, void* reserved)
{
    LOGV("JNI_OnLoad");
    g_jvmInstance = jvm;
    LOGV("g_jvmInstance = %p",g_jvmInstance);
    pthread_key_create(&jnienv_key,_android_key_cleanup);
    LOGV("&jnienv_key = %p",&jnienv_key);
    return JNI_VERSION_1_4;
}

 void JNI_OnUnload(JavaVM* jvm, void* reserved)
{
	JNIEnv *env;
	LOGV("JNI_OnUnload");
	g_jvmInstance=0;

	if(jvm->GetEnv((void**)&env, JNI_VERSION_1_4) != JNI_OK)
	{
		LOGV("Get the current java vm enviroment fail!!\n");
		return;
	}
    else
	{
    	LOGV("Get the current java vm enviroment done!!\n");
	}
}
 void unInitAndroidDisplay(){
 	 	 JNIEnv *jenv=get_jni_env();
 	 	if (ad->ogl) {
 	 		/* clear native ptr, to prevent rendering to occur now that ptr is invalid */
 	 		if (ad->android_video_window)
 	 			jenv->CallVoidMethod(ad->android_video_window,ad->set_opengles_display_id, 0);
 	 		ogl_display_uninit(ad->ogl,TRUE);
 	 		ogl_display_free(ad->ogl);
 	 		ad->ogl = NULL;
 	 	}
 	 	if (ad->android_video_window) jenv->DeleteGlobalRef(ad->android_video_window);

 	 	av_free(ad);
 	 	ad = NULL;
 }




 static void dec_open(DecData *d){
 	AVCodec *codec;
 	int error;
 	codec=avcodec_find_decoder(CODEC_ID_H264);
 	if (codec==NULL) LOGE("Could not find H264 decoder in ffmpeg.");
 	else{
 		LOGV("Found H264 CODEC");
 	}
 	avcodec_get_context_defaults3(&d->av_context, NULL);
 	//d->av_context.flags2 |= CODEC_FLAG2_CHUNKS;
 	error=avcodec_open2(&d->av_context,codec, NULL);
 	if (error!=0){
 		LOGE("avcodec_open() failed.");
 	}else{
 		LOGV("OPENED H264 CODEC");
 	}
 }

 DecData *d = NULL;
 static void dec_init(){
 	d=(DecData*)av_mallocz(sizeof(DecData));
 	ffmpeg_init();
 	d->sws_ctx=NULL;
 	d->packet_num=0;
 	dec_open(d);
 	d->bitstream_size=65535;
 	d->bitstream=(uint8_t*)av_mallocz(d->bitstream_size);
 	d->last_error_reported_time=0;
 	d->isStoped = false;
 }
 static void dec_uninit(){
 	if (d->sws_ctx!=NULL){
 		sws_freeContext(d->sws_ctx);
 		d->sws_ctx=NULL;
 	}
 	avcodec_close(&d->av_context);
 	av_free(d->bitstream);
 	av_free(d);
 	d = NULL;
 }

 static void yuv_buf_init(YuvBuf *buf, int w, int h, uint8_t *ptr){
 	int ysize,usize;
 	ysize=w*h;
 	usize=ysize/4;
 	buf->w=w;
 	buf->h=h;
 	buf->planes[0]=ptr;
 	buf->planes[1]=buf->planes[0]+ysize;
 	buf->planes[2]=buf->planes[1]+usize;
 	buf->planes[3]=0;
 	buf->strides[0]=w;
 	buf->strides[1]=w/2;
 	buf->strides[2]=buf->strides[1];
 	buf->strides[3]=0;
 }


 int decodeNalu(char *addr,int size) {

	AVFrame *orig = av_frame_alloc();
	int len;
	int got_picture=0;
	AVPacket pkt;
	av_init_packet(&pkt);

	pkt.data = (uint8_t *)addr;
	pkt.size = size;

	__android_log_print(ANDROID_LOG_ERROR, "Two", "Decode:%d",Naludata.num);

	len=avcodec_decode_video2(&d->av_context,orig,&got_picture,&pkt);

	LOGV("decoded data len = %d    got_picture = %d",len,got_picture);
	LOGV("av_context.width = %d    av_context.height = %d  av_context.pix_fmt = %d",d->av_context.width,d->av_context.height,d->av_context.pix_fmt);
	if (len<=0) {
		LOGE("ms_AVdecoder_process: error %i.",len);
				return 1;
	}
	if (saveh264==1&&savenum>0)
	{
		//Y, U, V
		savenum--;
		int i;
		for(i=0;i<orig->height;i++){
			fwrite(orig->data[0]+orig->linesize[0]*i,1,orig->width,fp_out);
		}
		for(i=0;i<orig->height/2;i++){
			fwrite(orig->data[1]+orig->linesize[1]*i,1,orig->width/2,fp_out);
		}
		for(i=0;i<orig->height/2;i++){
			fwrite(orig->data[2]+orig->linesize[2]*i,1,orig->width/2,fp_out);
		}
	}
	if (got_picture) {
		LOGV("one Frame Decoded");
		LOGV("Frame width = %d,height = %d,keyFrame = %d format = %d", orig->width,orig->height,orig->key_frame,orig->format);
		//ms_queue_put(f->outputs[0],get_as_yuvmsg(f,d,&orig));
		if(d->sws_ctx == NULL){
			d->sws_ctx=sws_getContext(d->av_context.width,d->av_context.height,d->av_context.pix_fmt,
					d->av_context.width,d->av_context.height,PIX_FMT_YUV420P,SWS_FAST_BILINEAR,
								NULL, NULL, NULL);
		}
		YuvBuf *yuvbuff = (YuvBuf*)av_mallocz(sizeof(YuvBuf));
		yuvbuff->w = orig->width;
		yuvbuff->h = orig->height;
		uint8_t *ptr = (uint8_t *)av_mallocz((yuvbuff->w*yuvbuff->h*3)/2);
		yuv_buf_init(yuvbuff,yuvbuff->w,yuvbuff->h,ptr);
		if (sws_scale(d->sws_ctx,(const uint8_t * const *)orig->data,orig->linesize, 0,
				d->av_context.height, yuvbuff->planes, yuvbuff->strides)<0){
			LOGE("error in sws_scale().");
		}

		/*yuvbuff->w = orig->width;
		yuvbuff->h = orig->height;
		for(int i = 0 ;i < 4;i++){
			yuvbuff->planes[i] = orig->data[i];
			yuvbuff->strides[i] = orig->linesize[i];
		}*/
		ogl_display_set_yuv_to_display(ad->ogl,yuvbuff);
		LOGV("ad->android_video_window = %p",ad->android_video_window);
		JNIEnv *env=get_jni_env();
		env->CallVoidMethod(ad->android_video_window,ad->request_render_id);
		//av_free(yuvbuff);
		//usleep(1000000/30);
	}

	av_free_packet(&pkt);
	av_frame_free(&orig);
 	return 0;
 }

int putdata()
{
	while(1)
	{
		pthread_mutex_lock(&(Nalulist.mutex));
		if (Nalulist.frame_cnt_unuse >= FRAME_MAX_CNT)
		{
			pthread_mutex_unlock(&(Nalulist.mutex));
			usleep(1000);
			continue;
		}
		int r_frame_idx = Nalulist.r_frame_idx;

		fwrite(Nalulist.frame[r_frame_idx].abFrameData,1,Nalulist.frame[r_frame_idx].nFrameDataMaxSize,fp_h264);
		decodeNalu(Nalulist.frame[r_frame_idx].abFrameData,Nalulist.frame[r_frame_idx].nFrameDataMaxSize);


		Nalulist.r_frame_idx++;
		if (Nalulist.r_frame_idx >= FRAME_MAX_CNT)
			Nalulist.r_frame_idx = 0;
		Nalulist.frame_cnt_unuse++;
		pthread_mutex_unlock(&(Nalulist.mutex));
	}
}

pthread_t	_threadDec;
void *DecThread(void *obj) {
	const char *name = (const char *)obj;
	LOGV("in DecThread name = %s",name);
	dec_init();
	putdata();
	dec_uninit();
	unInitAndroidDisplay();
	_android_key_cleanup(NULL);
	return NULL;
}


pthread_t	_threadGet;
void *GetThread(void *obj) {
	char *name = (char *)obj;
	LOGV("in DecThread name = %s",name);
	recivertmp(1,name);
	return NULL;
}


void androidplay_init(JNIEnv *env,jobject id)
{
	if(ad == NULL)
	{
		ad = (AndroidDisplay*)av_mallocz(sizeof(AndroidDisplay));
		jclass wc = env->FindClass("com/example/h264dec/display/AndroidVideoWindowImpl");
		if (wc==0)
		{
				LOGE("Could not find com/example/h264dec/display/AndroidVideoWindowImpl class !");
		}
		ad->set_opengles_display_id=env->GetMethodID(wc,"setOpenGLESDisplay","(I)V");
		ad->request_render_id= env->GetMethodID(wc,"requestRender","()V");
		if (ad->set_opengles_display_id == 0)
			LOGE("Could not find 'setOpenGLESDisplay' method\n");
		if (ad->request_render_id == 0)
			LOGE("Could not find 'requestRender' method\n");
		ad->ogl = ogl_display_new();
		if (id)
		{
			unsigned int ptr = (unsigned int)ad->ogl;
			ad->android_video_window=(env)->NewGlobalRef(id);
			LOGV("Sending opengles_display pointer as long: %p -> %u", ad->ogl, ptr);
			(env)->CallVoidMethod(id,ad->set_opengles_display_id, ptr);
		}
		else ad->android_video_window=NULL;
		env->DeleteLocalRef(wc);
	}
}

void Java_com_example_h264dec_ActivityYuvOrRgbViewer_playVideo(JNIEnv *env, jclass cls, jobject id,jstring infile) {
	LOGV("Java_com_example_h264dec_ActivityYuvOrRgbViewer_setVideoWindowId id = %p",id);
	const char *inputFileName = env->GetStringUTFChars(infile,0);

	androidplay_init(env,id);
	if(0 != pthread_create(&_threadGet, NULL, GetThread, (void *)inputFileName)) {
			LOGE("Could not create decode thread !!!");
		}
	if(0 != pthread_create(&_threadDec, NULL, DecThread, (void *)inputFileName)) {
		LOGE("Could not create decode thread !!!");
	}

}
void Java_com_example_h264dec_ActivityYuvOrRgbViewer_saveVideo(JNIEnv *env, jclass cls) {
	LOGV("Java_com_example_h264dec_ActivityYuvOrRgbViewer_stopVideo");

	saveh264=1;
	savenum=100;
	//if(d != NULL){
	//	d->isStoped = true;
	//}
}
void Java_com_example_h264dec_ActivityYuvOrRgbViewer_stopVideo(JNIEnv *env, jclass cls) {
	LOGV("Java_com_example_h264dec_ActivityYuvOrRgbViewer_stopVideo");

	//if(d != NULL){
	//	d->isStoped = true;
	//}
}
jstring Java_com_example_h264dec_ActivityH264Decoder_decodeFile
  	(JNIEnv *env, jclass cls, jstring infile, jstring outfile, jint width, jint height) {
	const char *inputFileName = env->GetStringUTFChars(infile,0);
	const char *outputFileName = env->GetStringUTFChars(outfile,0);
	//dec_init();
	//__android_log_print(ANDROID_LOG_ERROR,"LOG TAG", "DECODING FROM %s TO %s\n", inputFileName, outputFileName);
	//LOGV("before decode_file");

	int ret = 0;//decode_file(inputFileName);
	LOGV("decode_file ret = %d",ret);
	const char *result = resultDescription(ret);
	//dec_uninit();
	__android_log_print(ANDROID_LOG_ERROR,"LOG TAG", "RESULT: %s\n", result);
	return env->NewStringUTF(result);
}

#ifdef __cplusplus
}
#endif
