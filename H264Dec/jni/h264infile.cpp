#ifdef __cplusplus
extern "C" {
#endif
#include"common.h"
#include"opengles_display.h"
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
typedef struct AndroidDisplay{
	jobject android_video_window;
	jmethodID set_opengles_display_id;
	jmethodID request_render_id;
	struct opengles_display* ogl;
}AndroidDisplay;
AndroidDisplay *ad = NULL;
JavaVM* g_jvmInstance=0;
pthread_key_t jnienv_key;
JNIEnv *get_jni_env(void){
	JNIEnv *env=NULL;
	if (g_jvmInstance==NULL){
		LOGE("NO jvm");
	}else{
		env=(JNIEnv*)pthread_getspecific(jnienv_key);
		if (env==NULL){
			LOGE("AttachCurrentThread()");
			if (g_jvmInstance->AttachCurrentThread(&env,NULL)!=0){
				LOGE("AttachCurrentThread() failed !");
				return NULL;
			}
			LOGE("AttachCurrentThread() success !");
			pthread_setspecific(jnienv_key,env);
		}
	}
	return env;
}
void _android_key_cleanup(void *data){
	LOGV("_android_key_cleanup");
	JNIEnv* env=(JNIEnv*)pthread_getspecific(jnienv_key);
	if (env != NULL) {
		LOGV("Thread end, detaching jvm from current thread g_jvmInstance = %p",g_jvmInstance);
		g_jvmInstance->DetachCurrentThread();
		LOGV("Thread end, begin pthread_setspecific &jnienv_key = %p",&jnienv_key);
		pthread_setspecific(jnienv_key,NULL);
		LOGV("Thread end, end pthread_setspecific &jnienv_key = %p",&jnienv_key);
	}
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
 int verifyFile(const char *fileFrom) {
 	LOGV("verifyFile in");
 	FILE *inputBufferFile = fopen(fileFrom, "rb");
 	if(inputBufferFile == NULL) {
 		LOGV("Failed to open the file %s\n", fileFrom);
 		return -1;
 	}
 	fclose(inputBufferFile);

 	LOGV("verifyFile out");
 	return 0;
 }
 int readBufferFromH264InputFile(FILE *file, uint8_t *buff, int maxBuffSize, long *timeStampLfile)  {
     unsigned int readOffset = 0;
     int bytes_read = 0;
     unsigned int code = 0;

 	do {
 		//Start codes are always byte aligned.
 		bytes_read = fread(buff + readOffset, 1, 1, file);
 		if(bytes_read == 0) {
 			LOGE("QCOMOMXINTERFACE - Failed to read frame or it might be EOF\n \n");
 			return 0;
 		}
 		code <<= 8;
 		code |= (0x000000FF & buff[readOffset]);

 		//VOP start code comparision //util read the second frame start code
 		if ((readOffset>3) && ( H264_START_CODE == code)) {
 			//Seek backwards by 4
 			fseek(file, -4, SEEK_CUR);
 			readOffset-=3;
 			break;
 		}
 		readOffset++;

 		if ((int)readOffset >= maxBuffSize - 1) {
 			if ((int)readOffset > maxBuffSize - 1) {
 				int diff = readOffset - maxBuffSize;
 				readOffset -= diff;
 				fseek(file, -diff, SEEK_CUR);
 			}
 			break;
 		}

 	} while (1);

 	*timeStampLfile += 1000000/10;
 	return readOffset;
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
 int decode_file(const char *fileFrom) {
 	int errorCode = verifyFile(fileFrom);
 	LOGV("decode_file errorCode = %d",errorCode);
 	if (errorCode != 0) {
 		return errorCode;
 	}
 	FILE *fFrom = fopen(fileFrom, "rb");

 	//char buffer[3350000];
 	//	int maxBuffSize = 3350000;
 		long timeStampLfile = 0;
 		int totalSize = 0;

 		int size = 0;
 		AVFrame *orig = av_frame_alloc();
 		uint8_t *p,*end;
 		LOGV("decode_file in while ");
 		do {
 			size = readBufferFromH264InputFile(fFrom, d->bitstream, d->bitstream_size, &timeStampLfile);
 			LOGV("readBufferFromH264InputFile size = %d",size);
 			int result = 0;
 			p=d->bitstream;
 			end=d->bitstream+size;
 			while (end-p>0 && !d->isStoped) {
 					int len;
 					int got_picture=0;
 					AVPacket pkt;
 					//avcodec_get_frame_defaults(orig);
 					av_init_packet(&pkt);
 					pkt.data = p;
 					pkt.size = end-p;
 					len=avcodec_decode_video2(&d->av_context,orig,&got_picture,&pkt);
 					LOGV("decoded data len = %d    got_picture = %d",len,got_picture);
 					LOGV("av_context.width = %d    av_context.height = %d  av_context.pix_fmt = %d",d->av_context.width,d->av_context.height,d->av_context.pix_fmt);
 					if (len<=0) {
 						LOGE("ms_AVdecoder_process: error %i.",len);
 								break;
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
 								#if LIBSWSCALE_VERSION_INT >= AV_VERSION_INT(0,9,0)
 									if (sws_scale(d->sws_ctx,(const uint8_t * const *)orig->data,orig->linesize, 0,
 											d->av_context.height, yuvbuff->planes, yuvbuff->strides)<0){
 								#else
 									if (sws_scale(d->sws_ctx,(uint8_t **)orig->data,orig->linesize, 0,
 											d->av_context.height, yuvbuff->planes, yuvbuff->strides)<0){
 								#endif
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
 							p+=len;
 						}

 		} while (size > 0 && !d->isStoped);
 		av_frame_free(&orig);
 		totalSize += size;
 		fclose(fFrom);
 	//sem_init(&semaphoreBufferIsAvailable, 0, 0);
 //        LOGE("Decoding file using codec string : %s", codecString);
 	//FILE *file = fopen(fileTo, "wb");



 	return errorCode;
 }
pthread_t	_threadDec;
void *DecThread(void *obj) {
	const char *name = (const char *)obj;
	LOGV("in DecThread name = %s",name);
	dec_init();
	decode_file(name);
	dec_uninit();
	unInitAndroidDisplay();
	_android_key_cleanup(NULL);
	return NULL;
}
void Java_com_example_h264dec_ActivityYuvOrRgbViewer_setVideoWindowId(JNIEnv *env, jclass cls, jobject id,jstring infile) {
	LOGV("Java_com_example_h264dec_ActivityYuvOrRgbViewer_setVideoWindowId id = %p",id);
	const char *inputFileName = env->GetStringUTFChars(infile,0);
	if(ad == NULL){
		ad = (AndroidDisplay*)av_mallocz(sizeof(AndroidDisplay));
	jclass wc = env->FindClass("com/example/h264dec/display/AndroidVideoWindowImpl");
	if (wc==0){
			LOGE("Could not find com/example/h264dec/display/AndroidVideoWindowImpl class !");
	}
	ad->set_opengles_display_id=env->GetMethodID(wc,"setOpenGLESDisplay","(I)V");
		ad->request_render_id= env->GetMethodID(wc,"requestRender","()V");
		if (ad->set_opengles_display_id == 0)
			LOGE("Could not find 'setOpenGLESDisplay' method\n");
		if (ad->request_render_id == 0)
			LOGE("Could not find 'requestRender' method\n");
		ad->ogl = ogl_display_new();
		if (id) {
						unsigned int ptr = (unsigned int)ad->ogl;
						ad->android_video_window=(env)->NewGlobalRef(id);
						LOGV("Sending opengles_display pointer as long: %p -> %u", ad->ogl, ptr);
						(env)->CallVoidMethod(id,ad->set_opengles_display_id, ptr);
					}else ad->android_video_window=NULL;
		env->DeleteLocalRef(wc);
		if(0 != pthread_create(&_threadDec, NULL, DecThread, (void *)inputFileName)) {
			LOGE("Could not create decode thread !!!");
		}
	}
}
void Java_com_example_h264dec_ActivityYuvOrRgbViewer_stopVideo(JNIEnv *env, jclass cls) {
	LOGV("Java_com_example_h264dec_ActivityYuvOrRgbViewer_stopVideo");
	if(d != NULL){
		d->isStoped = true;
	}
}
jstring Java_com_example_h264dec_ActivityH264Decoder_decodeFile
  	(JNIEnv *env, jclass cls, jstring infile, jstring outfile, jint width, jint height) {
	const char *inputFileName = env->GetStringUTFChars(infile,0);
	const char *outputFileName = env->GetStringUTFChars(outfile,0);
	dec_init();
	__android_log_print(ANDROID_LOG_ERROR,"LOG TAG", "DECODING FROM %s TO %s\n", inputFileName, outputFileName);
	LOGV("before decode_file");
	int ret = decode_file(inputFileName);
	LOGV("decode_file ret = %d",ret);
	const char *result = resultDescription(ret);
	dec_uninit();
	__android_log_print(ANDROID_LOG_ERROR,"LOG TAG", "RESULT: %s\n", result);
	return env->NewStringUTF(result);
}

#ifdef __cplusplus
}
#endif
