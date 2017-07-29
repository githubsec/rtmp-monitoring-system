/*
 * rtmprecive.cpp
 *
 *  Created on: 2017骞�2鏈�22鏃�
 *      Author: c
 */

#include <stdio.h>
#include<string.h>
#include<stdlib.h>
#include "rtmp_sys.h"
#include "log.h"
#include "test.h"



#ifdef __cplusplus
extern "C"
{
#endif
#include"common.h"
#include"opengles_display.h"
#include <libavcodec/avcodec.h>
#ifdef __cplusplus
};
#endif

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

AVPacket avpkt;
AVFrame *orig=NULL;
YuvBuf *yuvbuff=NULL;

typedef struct DecState{
	AVCodec *av_codec;
	AVCodecContext *av_context;
	AVCodecParserContext *pCodecParserCtx;

	enum AVPixelFormat output_pix_fmt;
	enum AVCodecID codec;
	struct SwsContext *sws_ctx;
}DecState;

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

FILE *fpvideo=fopen("/sdcard/rtmpvideo.h264","wb");

int recivertmp()
{
	InitSockets();

	double duration=-1;
	int nRead;
	//is live stream ?
	bool bLiveStream=true;


	int bufsize=1024*1024*10;
	char *buf=(char*)malloc(bufsize);
	memset(buf,0,bufsize);
	long countbufsize=0;

	FILE *fp=fopen("/sdcard/rtmpreceive.flv","wb");
	if (!fp){
		RTMP_LogPrintf("Open File Error.\n");
		__android_log_print(ANDROID_LOG_ERROR, "First", "Open File Error.");
		CleanupSockets();
		return -1;
	}

	/* set log level */
	//RTMP_LogLevel loglvl=RTMP_LOGDEBUG;
	//RTMP_LogSetLevel(loglvl);

	int handler;
	handler=decode_init();

	RTMP *rtmp=RTMP_Alloc();
	RTMP_Init(rtmp);
	//set connection timeout,default 30s
	rtmp->Link.timeout=10;
	// HKS's live URL
	if(!RTMP_SetupURL(rtmp,"rtmp://live.hkstv.hk.lxdns.com/live/hks"))
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
		RTMP_Free(rtmp);
		CleanupSockets();
		return -1;
	}

	if(!RTMP_ConnectStream(rtmp,0)){
		RTMP_Log(RTMP_LOGERROR,"ConnectStream Err\n");
		RTMP_Close(rtmp);
		RTMP_Free(rtmp);
		CleanupSockets();
		return -1;
	}

	while(nRead=RTMP_Read(rtmp,buf,bufsize)){
		fwrite(buf,1,nRead,fp);

		flv_video(buf,nRead,handler);

		countbufsize+=nRead;
		//RTMP_LogPrintf("Receive: %5dByte, Total: %5.2fkB\n",nRead,countbufsize*1.0/1024);
	}

	if(fp)
		fclose(fp);

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
	 char *body;
	 TAG_HEADER *tagheader;
	 int size=0;
	 int naltail_pos=0;
	 int i = 0;
	 int nalu_type;

	 //remember free
	 tagheader=(TAG_HEADER*)malloc(sizeof(TAG_HEADER));
	 memset(tagheader,0,sizeof(TAG_HEADER));
	 memcpy(tagheader,buf,sizeof(TAG_HEADER));

	int tagheader_datasize=tagheader->DataSize[0]*65536+tagheader->DataSize[1]*256+tagheader->DataSize[2];
	int tagheader_timestamp=tagheader->Timestamp[0]*65536+tagheader->Timestamp[1]*256+tagheader->Timestamp[2];

	char tagtype_str[10];
	/*switch(tagheader->TagType)
	{
		case TAG_TYPE_AUDIO:sprintf(tagtype_str,"AUDIO");break;
		case TAG_TYPE_VIDEO:sprintf(tagtype_str,"VIDEO");break;
		case TAG_TYPE_SCRIPT:sprintf(tagtype_str,"SCRIPT");break;
		default:sprintf(tagtype_str,"UNKNOWN");break;
	}
	//printf("[%6s] %6d %6d |\n",tagtype_str,tagheader_datasize,tagheader_timestamp);*/



	//process tag by type
	switch (tagheader->TagType)
	{

		case TAG_TYPE_AUDIO:
		{
			break;
		}
		case TAG_TYPE_VIDEO:
		{
			char videotag_str[100]={0};
			strcat(videotag_str,"| ");
			char tagdata_first_byte;
			tagdata_first_byte=buf[11];
			int x=tagdata_first_byte&0xF0;
			x=x>>4;
			switch (x)
			{
			case 1:
			{
				naltail_pos+=16;
				strcat(videotag_str,"key frame  ");
				nalu_type=buf[12]&0xff;
				if(nalu_type==1)
				{
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
						int error;
						error=decode(handler,body,size+4);
						//fwrite(body,1,size+4,fpvideo);
						free(body);
						naltail_pos+=size;
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
				naltail_pos+=16;
				nalu_type=buf[12]&0xff;
				if(nalu_type==1)
				{
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
						int error;
						error=decode(handler,body,size+4);
						//fwrite(body,1,size+4,fpvideo);
						free(body);
						naltail_pos+=size;
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
			}
		default:
			break;
	}


	free(tagheader);

    return 0;
}

static void yuv_buf_init(YuvBuf *buf, uint8_t *ptr){
	int w=buf->w;
	int h=buf->h;
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

int decode_init()
{
	DecState *pdec=(DecState*)malloc(sizeof(DecState));
	avcodec_register_all();

	pdec->av_context= NULL;
	pdec->pCodecParserCtx=NULL;
	pdec->codec=AV_CODEC_ID_H264;
	pdec->output_pix_fmt=AV_PIX_FMT_YUV420P;
	pdec->av_codec=avcodec_find_decoder(pdec->codec);
	pdec->sws_ctx=NULL;
	if (pdec->av_codec==NULL) {
		printf("Codec not found\n");
		return -1;
	}

	pdec->av_context = avcodec_alloc_context3(pdec->av_codec);
	if (!pdec->av_context)
	{
		printf("Could not allocate video codec context\n");
		return -1;
	}

	pdec->pCodecParserCtx=av_parser_init(pdec->codec);
	if (pdec->pCodecParserCtx==NULL)
	{
		printf("Could not allocate video parser context\n");
		return -1;
	}

	int error;

	error=avcodec_open2(pdec->av_context, pdec->av_codec, NULL);
	if (error!=0)
	{
		printf("Could not open codec\n");
		return -1;
	}
	av_init_packet(&avpkt);
	orig=av_frame_alloc();
	yuvbuff = (YuvBuf*)av_mallocz(sizeof(YuvBuf));

   return (int)pdec;
}

int decode(int phandle, char* addr,int len)
{
	DecState *pdec=(DecState *)phandle;
	int got_picture=0;
	//__android_log_print(ANDROID_LOG_ERROR, "decodeFrame",  "avcodec_decode_video ........................\n");
	//if(avcodec_decode_video(&pdec->av_context,&orig,&got_picture,addr, len) < 0)

	avpkt.data =(uint8_t*) addr;
	avpkt.size = len;

	int framenum=0;
	unsigned int maxfps=0;

	if(avcodec_decode_video2(pdec->av_context,orig,&got_picture,&avpkt) != len)
	{
		if (len <= 4 ||
			(((addr[4]&0x1f) != 7) && ((addr[4]&0x1f) != 8)
			&&((addr[3]&0x1f) != 7) && ((addr[3]&0x1f) != 8)))
				//printf("decodeFrame", "decode error ,addr[3,4](%x,%x)",  addr[3], addr[4]);
		return 0;
	}

	if (got_picture)
	{
		//Y, U, V
		/*int i;
		for(i=0;i<orig->height;i++)
		{
		fwrite(orig->data[0]+orig->linesize[0]*i,1,orig->width,fp_out);
		}
    	for(i=0;i<orig->height/2;i++)
    	{
			fwrite(orig->data[1]+orig->linesize[1]*i,1,orig->width/2,fp_out);
		}
		for(i=0;i<orig->height/2;i++)
		{
			fwrite(orig->data[2]+orig->linesize[2]*i,1,orig->width/2,fp_out);
		}
		*/
		if(pdec->sws_ctx == NULL)
		{
			pdec->sws_ctx=sws_getContext(pdec->av_context->width,pdec->av_context->height,pdec->av_context->pix_fmt,
					pdec->av_context->width,pdec->av_context->height,PIX_FMT_YUV420P,SWS_FAST_BILINEAR,
								NULL, NULL, NULL);
		}
		//YuvBuf *yuvbuff = (YuvBuf*)av_mallocz(sizeof(YuvBuf));
		yuvbuff->w = orig->width;
		yuvbuff->h = orig->height;
		uint8_t *ptr = (uint8_t *)av_mallocz((yuvbuff->w*yuvbuff->h*3)/2);
		yuv_buf_init(yuvbuff,ptr);
		#if LIBSWSCALE_VERSION_INT >= AV_VERSION_INT(0,9,0)
			if (sws_scale(pdec->sws_ctx,(const uint8_t * const *)orig->data,orig->linesize, 0,
					pdec->av_context->height, yuvbuff->planes, yuvbuff->strides)<0){
		#else
			if (sws_scale(pdec->sws_ctx,(uint8_t **)orig->data,orig->linesize, 0,
					pdec->av_context->height, yuvbuff->planes, yuvbuff->strides)<0){
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

	return 1;
}

void decode_uninit(int phandle)
{
	DecState *pdec=(DecState *)phandle;

	if (pdec->sws_ctx!=NULL){
	 		sws_freeContext(pdec->sws_ctx);
	 		pdec->sws_ctx=NULL;
	 	}
	if (pdec->av_context!=NULL)
	{
		avcodec_close(pdec->av_context);
		av_free(pdec->av_context);
		pdec->av_context=NULL;
	}


	if (orig != NULL)
	{
		av_frame_free(&orig);
		orig = NULL;
	}

	av_free(yuvbuff);
	free(pdec);
	return;
}
