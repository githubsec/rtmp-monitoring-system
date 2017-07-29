/*
mediastreamer2 library - modular sound and video processing and streaming
Copyright (C) 2006  Simon MORLAT (simon.morlat@linphone.org)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#include "mediastreamer-config.h"
#endif

#include <jni.h>
#include "mediastreamer2/msfilter.h"
#include "mediastreamer2/msvideo.h"
#include <android/log.h>
#include <pthread.h>

#include <libavutil/opt.h>
#include <libavcodec/avcodec.h>
#include <libavutil/common.h>
#include <libavutil/mathematics.h>
#include <libavutil/avutil.h>
#include <libswscale/swscale.h>
#include <libavformat/avformat.h>

#include "hevcdecoder.h"
#include "hwh265.h"

#include "android-opengl-display.h"

enum SyncMode {
	SYNC_NONE,
	SYNC_PFRAME,
	SYNC_IFRAME
};


//added by JM.
typedef struct PacketQueue   
{   
    AVPacketList *first_pkt, *last_pkt;   
    int nb_packets;   
    int size;   
    pthread_mutex_t mutex;   
    pthread_cond_t cond;   
} PacketQueue; 
//end added by JM.



#define AVCODEC_MAX_AUDIO_FRAME_SIZE 192000

typedef struct DecState{
	AVCodecContext* av_context;
	AVCodec *av_codec;
	enum AVCodecID codec;
	mblk_t *input;
	YuvBuf outbuf;
	mblk_t *yuv_msg;
	struct SwsContext *sws_ctx;
	enum AVPixelFormat output_pix_fmt;
	uint8_t dci[512];
	int dci_size;
	uint64_t last_error_reported_time;
	bool_t snow_initialized;
	int sync_mode;
	bool_t sync_status;
	uint16_t sync_seq;
	int frameid;
	jobject winid;
	
//testing locally
	AVCodecParserContext *pCodecParserCtx;
	uint8_t *cur_ptr;
	int cur_size;
	int first_time;
	int flush;

//added by JM. 
	AVCodecContext* 	audioContext;
	AVCodec 		*audio_codec;
	PacketQueue     audioq;  
	uint8_t         audio_buf[AVCODEC_MAX_AUDIO_FRAME_SIZE];  
	unsigned int    audio_buf_size;  
	unsigned int    audio_buf_index;  
	AVPacket        audio_pkt;  
	uint8_t         *audio_pkt_data;  
	int             audio_pkt_size;  
	int             quit;
//end added by JM.
}DecState;

//added by XL
typedef struct HevcDecState{
	vid_dec_ctx_t s_app_ctx;
	YuvBuf outbuf;
	mblk_t *yuv_msg;
	struct SwsContext *sws_ctx;
	int frameid;
	jobject winid;

	FILE *ps_piclen_file;
	FILE *ps_ip_file;
	FILE *ps_op_file;
	FILE *ps_op_chksum_file;
	WORD32 ret;
	CHAR ac_error_str[STRLENGTH];
	UWORD8 *pu1_bs_buf;

	ivd_out_bufdesc_t *ps_out_buf;
	UWORD32 u4_num_bytes_dec;
	UWORD32 file_pos;

	UWORD32 u4_ip_frm_ts;
	UWORD32 u4_op_frm_ts ;

	WORD32 u4_bytes_remaining;
	UWORD32 i;
	UWORD32 u4_ip_buf_len;
	UWORD32 frm_cnt;
	WORD32 total_bytes_comsumed;

//#ifdef PROFILE_ENABLE
	UWORD32 u4_tot_cycles;
	UWORD32 u4_tot_fmt_cycles;
	UWORD32 peak_window[PEAK_WINDOW_SIZE];
	UWORD32 peak_window_idx;
	UWORD32 peak_avg_max;
//#endif


	WORD32 width;
	WORD32 height;
	iv_obj_t *codec_obj;

	//ffmpeg parser
	AVCodecContext* av_context;
	AVCodec *av_codec;
	enum AVCodecID codec;
	AVCodecParserContext *pCodecParserCtx;
	uint8_t *cur_ptr;
	int cur_size;

	//haisi parser
	UWORD8 *pStream;
	WORD32 iFileLen;
	WORD32 iNaluLen;
	UWORD8* pInputStream;

	//test time
	int firstframe;
	unsigned int threetime;
	int framenum;
	unsigned int displaytime;
	unsigned int parsertime;
}HevcDecState;

//Added by XL
typedef struct HwDecState
{
	FILE *fpInFile;
	FILE *fpOutFile;
	YuvBuf outbuf;
	mblk_t *yuv_msg;
	struct SwsContext *sws_ctx;
	int frameid;
	jobject winid;

	INT32 iRet;
	INT32 iInputParam;
	UINT8 *pInputStream , *pStream;
	UINT32 uiChannelId;
	UINT32 iFrameIdx;
	BOOL32 bStreamEnd;
	INT32 iFileLen;
	INT32 LoadCount;
	INT32 time;
	INT64 StartTime, EndTime;
	IH265DEC_HANDLE hDecoder;
	IHW265D_INIT_PARAM stInitParam;

	IHWVIDEO_ALG_VERSION_STRU stVersion;
	INT32 MultiThreadEnable;
	INT32 DispOutput;

	int framenum;
	unsigned int threetime;
	unsigned int displaytime;
	unsigned int parsertime;

}HwDecState;


//added by JM.
DecState *global_video_state;  

//end added by JM.

static mblk_t *get_as_yuvmsg(MSFilter *f, DecState *s, AVFrame *orig){
	AVCodecContext *ctx=s->av_context;

	if (ctx->width==0 || ctx->height==0){
		__android_log_print(ANDROID_LOG_ERROR, "videodec", "%s: wrong image size provided by decoder.",f->desc->name);
		return NULL;
	}
	if (orig->data[0]==NULL){
		__android_log_print(ANDROID_LOG_ERROR, "videodec", "no image data.");
		return NULL;
	}
	if (s->outbuf.w!=ctx->width || s->outbuf.h!=ctx->height){
		if (s->sws_ctx!=NULL){
			sws_freeContext(s->sws_ctx);
			s->sws_ctx=NULL;
		}
		s->yuv_msg=ms_yuv_buf_alloc(&s->outbuf,ctx->width,ctx->height);
		s->outbuf.w=ctx->width;
		s->outbuf.h=ctx->height;
		s->sws_ctx=sws_getContext(ctx->width,ctx->height,ctx->pix_fmt, ctx->width,ctx->height,s->output_pix_fmt,SWS_FAST_BILINEAR, NULL, NULL, NULL);
	}
	if (s->sws_ctx==NULL){
		__android_log_print(ANDROID_LOG_ERROR, "videodec", "missing rescaling context.");
		return NULL;
	}
	if (sws_scale(s->sws_ctx,(const uint8_t* const*)orig->data,orig->linesize, 0,
					ctx->height, s->outbuf.planes, s->outbuf.strides)<0){
		__android_log_print(ANDROID_LOG_ERROR, "videodec", "error in ms_sws_scale().");
	}
	return dupmsg(s->yuv_msg);
}

static mblk_t *hw_get_as_yuvmsg(MSFilter *f, HwDecState *s, IH265DEC_OUTARGS *orig){
	if (orig->uiDecWidth==0 || orig->uiDecHeight==0){
		__android_log_print(ANDROID_LOG_ERROR, "videodec", "%s: wrong image size provided by decoder.",f->desc->name);
		return NULL;
	}
	if (orig->pucOutYUV[0]==NULL){
		__android_log_print(ANDROID_LOG_ERROR, "videodec", "no image data.");
		return NULL;
	}
	if (s->outbuf.w!=orig->uiDecWidth || s->outbuf.h!=orig->uiDecHeight){
		if (s->sws_ctx!=NULL){
			sws_freeContext(s->sws_ctx);
			s->sws_ctx=NULL;
		}
		s->yuv_msg=ms_yuv_buf_alloc(&s->outbuf,orig->uiDecWidth,orig->uiDecHeight);
		s->outbuf.w=orig->uiDecWidth;
		s->outbuf.h=orig->uiDecHeight;
		s->sws_ctx=sws_getContext(orig->uiDecWidth,orig->uiDecHeight,AV_PIX_FMT_YUV420P, orig->uiDecWidth,orig->uiDecHeight,AV_PIX_FMT_YUV420P,SWS_FAST_BILINEAR, NULL, NULL, NULL);
	}
	if (s->sws_ctx==NULL){
		__android_log_print(ANDROID_LOG_ERROR, "videodec", "missing rescaling context.");
		return NULL;
	}
	int linesize[4];
	linesize[0]=orig->uiYStride;
	linesize[1]=orig->uiUVStride;
	linesize[2]=orig->uiUVStride;
	if (sws_scale(s->sws_ctx,(const uint8_t* const*)orig->pucOutYUV,linesize, 0,
			orig->uiDecHeight, s->outbuf.planes, s->outbuf.strides)<0){
		__android_log_print(ANDROID_LOG_ERROR, "videodec", "error in ms_sws_scale().");
	}
	return dupmsg(s->yuv_msg);
}

static mblk_t *gethevc_as_yuvmsg(MSFilter *f, ivd_video_decode_op_t *yuv,HevcDecState *s){
	iv_yuv_buf_t ctx=yuv->s_disp_frm_buf;

	if (ctx.u4_y_wd==0 || ctx.u4_y_ht==0){
		__android_log_print(ANDROID_LOG_ERROR, "videodec", "%s: wrong image size provided by decoder.",f->desc->name);
		return NULL;
	}
	if (ctx.pv_u_buf==NULL){
		__android_log_print(ANDROID_LOG_ERROR, "videodec", "no image data.");
		return NULL;
	}
	if (s->outbuf.w!=ctx.u4_y_wd || s->outbuf.h!=ctx.u4_y_ht){
		if (s->sws_ctx!=NULL){
			sws_freeContext(s->sws_ctx);
			s->sws_ctx=NULL;
		}
		s->yuv_msg=ms_yuv_buf_alloc(&s->outbuf,ctx.u4_y_wd,ctx.u4_y_ht);
		s->outbuf.w=ctx.u4_y_wd;
		s->outbuf.h=ctx.u4_y_ht;
		s->sws_ctx=sws_getContext(ctx.u4_y_wd,ctx.u4_y_ht,AV_PIX_FMT_YUV420P, ctx.u4_y_wd,ctx.u4_y_ht,AV_PIX_FMT_YUV420P,SWS_FAST_BILINEAR, NULL, NULL, NULL);
	}
	if (s->sws_ctx==NULL){
		__android_log_print(ANDROID_LOG_ERROR, "videodec", "missing rescaling context.");
		return NULL;
	}

    uint8_t *data[4];
    data[0]=(uint8_t *)ctx.pv_y_buf;
    data[1]=(uint8_t *)ctx.pv_u_buf;
    data[2]=(uint8_t *)ctx.pv_v_buf;

    int linesize[4];
    linesize[0]=ctx.u4_y_strd;
    linesize[1]=ctx.u4_u_strd;
    linesize[2]=ctx.u4_v_strd;

	if (sws_scale(s->sws_ctx,(const uint8_t* const*)data,linesize, 0,
					ctx.u4_y_ht, s->outbuf.planes, s->outbuf.strides)<0){
		__android_log_print(ANDROID_LOG_ERROR, "videodec", "error in ms_sws_scale().");
	}
	return dupmsg(s->yuv_msg);
}

/* Bitmasks to select bits of a byte from low side */
static AVPacket avpkt;
static AVFrame *orig = NULL;
static pthread_mutex_t avpkt_mutex = PTHREAD_MUTEX_INITIALIZER;

static AVPacket audio_avpkt;
static AVFrame *audio_orig = NULL;
static pthread_mutex_t audio_avpkt_mutex = PTHREAD_MUTEX_INITIALIZER;


int AudioDecodeInit()
{
	DecState *pdec=(DecState *)malloc(sizeof(DecState)*1);
	
	avcodec_register_all();
	//avcodec_get_context_defaults(&pdec->audioContext);
	pdec->av_codec=NULL;
	pdec->codec=AV_CODEC_ID_ADPCM_G726;
	pdec->input=NULL;
	
	pdec->av_codec=avcodec_find_decoder(pdec->codec);
	if(pdec->av_codec==NULL)
	{
		__android_log_print(ANDROID_LOG_ERROR, "decodeFrame", "Line: %d,Could not find decoder %i!",__LINE__,pdec->codec);
		return 0;
	}
	pdec->av_context = avcodec_alloc_context3(pdec->av_codec);
	pdec->av_context->bits_per_coded_sample = 2;
	if(pdec->av_context==NULL)
	{
		__android_log_print(ANDROID_LOG_ERROR, "decodeFrame", "Line: %d,Could not avcodec_alloc_context3!",__LINE__);
		return 0;
	}

	int error;
	/* we must know picture size before initializing snow decoder*/
	error=avcodec_open2(pdec->av_context, pdec->av_codec, NULL);
	if (error!=0)
		__android_log_print(ANDROID_LOG_ERROR, "decodeFrame", 
		"avcodec_open() failed: %i",error);

	av_init_packet(&audio_avpkt);
	audio_orig = av_frame_alloc();

	return (int)pdec;

}
int AudioDecodeFrame(int phandle, uint8_t* addr, int len, char *pcmdata, int pcmmaxlen)
{
	DecState *pdec=(DecState *)phandle;
	int got_picture=0;
	int data_size = 0;

	//__android_log_print(ANDROID_LOG_ERROR, "decodeFrame",  "avcodec_decode_video ........................\n");
	////if(avcodec_decode_video(&pdec->av_context,&orig,&got_picture,addr, len) < 0)

	pthread_mutex_lock(&audio_avpkt_mutex);
	if (audio_orig == NULL)
	{
		pthread_mutex_unlock(&audio_avpkt_mutex);
		return 0;
	}
	audio_avpkt.data = addr;
	audio_avpkt.size = len;
	if(avcodec_decode_audio4(pdec->av_context,audio_orig,&got_picture,&audio_avpkt) < 0)
	{
		pthread_mutex_unlock(&audio_avpkt_mutex);
		__android_log_print(ANDROID_LOG_ERROR, "decodeFrame", "decode error line:%d", __LINE__);
		return 0;
	}
	if (got_picture)
	{
		data_size = av_samples_get_buffer_size(NULL, pdec->av_context->channels,
                 audio_orig->nb_samples,
                 pdec->av_context->sample_fmt, 1);//av_get_bytes_per_sample(pdec->av_context->sample_fmt);
		if (data_size < pcmmaxlen)
		{
			memcpy(pcmdata, audio_orig->data[0], data_size);
		}
		else
			data_size = 0;
	}
	pthread_mutex_unlock(&audio_avpkt_mutex);
	return data_size;

}
void AudioDecodeUnInit(int phandle)
{
	DecState *pdec=(DecState *)phandle;
	pthread_mutex_lock(&audio_avpkt_mutex);
	if (pdec->av_context!=NULL)
	{
		avcodec_close(pdec->av_context);
		av_free(pdec->av_context);
		pdec->av_context=NULL;
	}

	if (audio_orig != NULL)
	{
		av_frame_free(&audio_orig);
		audio_orig = NULL;
	}
	pthread_mutex_unlock(&audio_avpkt_mutex);
	__android_log_print(ANDROID_LOG_ERROR, "videodec", "line: %d", __LINE__);
	free(pdec);
}

int DecodeInit(JNIEnv* env, jobject winid)
{
	__android_log_print(ANDROID_LOG_ERROR, "videodec", "DecodeInit閿涗緤绱掗敍?");
	DecState *pdec=(DecState *)malloc(sizeof(DecState)*1);
	
	avcodec_register_all();
	//avcodec_get_context_defaults(&pdec->audioContext);
	pdec->av_codec=NULL;
	pdec->audio_codec = NULL;
	pdec->codec=AV_CODEC_ID_H264;
	pdec->input=NULL;
	pdec->yuv_msg=NULL;
	pdec->output_pix_fmt=AV_PIX_FMT_YUV420P;
	pdec->snow_initialized=FALSE;
	pdec->outbuf.w=0;
	pdec->outbuf.h=0;
	pdec->sws_ctx=NULL;
	
	pdec->av_codec=avcodec_find_decoder(pdec->codec);
	if(pdec->av_codec==NULL)
	{
		ms_error("Line: %d,Could not find decoder %i!",__LINE__,pdec->codec);
		return 0;
	}
	pdec->av_context = avcodec_alloc_context3(pdec->av_codec);


	int error;
	/* we must know picture size before initializing snow decoder*/
	error=avcodec_open2(pdec->av_context, pdec->av_codec, NULL);
	if (error!=0)
		ms_error("avcodec_open() failed: %i",error);
	if (pdec->codec==AV_CODEC_ID_H264 && pdec->dci_size>0)
	{
		pdec->av_context->extradata=pdec->dci;
		pdec->av_context->extradata_size=pdec->dci_size;
	}
	
	__android_log_print(ANDROID_LOG_ERROR, "videodec", "before pdec->frameid = android_ogl_display_init(env, winid);"); 
	

	pdec->frameid = android_ogl_display_init(env, winid);
	pdec->winid = winid;
	
	__android_log_print(ANDROID_LOG_ERROR, "videodec", "%d:pdec->frameid(%d),env(%p)", 
		__LINE__, pdec->frameid, env);
	av_init_packet(&avpkt);
	orig = av_frame_alloc();

	return (int)pdec;
}

//extern FILE *fp;
static FILE *fp = NULL;
int DecodeFrame(JNIEnv* env, int phandle, uint8_t* addr, int len)
{
	DecState *pdec=(DecState *)phandle;
	int got_picture=0;
	//__android_log_print(ANDROID_LOG_ERROR, "decodeFrame",  "avcodec_decode_video ........................\n");
	//if(avcodec_decode_video(&pdec->av_context,&orig,&got_picture,addr, len) < 0)

	pthread_mutex_lock(&avpkt_mutex);
	if (orig == NULL)
	{
		pthread_mutex_unlock(&avpkt_mutex);
		return 1;
	}
	if (fp == NULL) {
		//fp = fopen("/sdcard/debug.h264", "wb");
	}
	if (fp != NULL) {
		fwrite(addr, 1, len, fp);
	}
	avpkt.data = addr;
	avpkt.size = len;

	//test fps
	UWORD32 s_elapsed_time;
	TIMER s_start_timer;
	TIMER s_end_timer;
	TIMER f_start_timer;
	TIMER f_end_timer;
	int framenum=0;
	unsigned int maxfps=0;
	GETTIME(&f_start_timer);
	if(avcodec_decode_video2(pdec->av_context,orig,&got_picture,&avpkt) != len)
	{
		pthread_mutex_unlock(&avpkt_mutex);
		if (len <= 4 || 
			(((addr[4]&0x1f) != 7) && ((addr[4]&0x1f) != 8) 
			&&((addr[3]&0x1f) != 7) && ((addr[3]&0x1f) != 8)))
				__android_log_print(ANDROID_LOG_ERROR, "decodeFrame", "decode error line:%d, addr[3,4](%x,%x)", __LINE__, addr[3], addr[4]);
		return 0;
	}
	GETTIME(&f_end_timer);
	ELAPSEDTIME(f_start_timer, f_end_timer, s_elapsed_time, frequency);
	__android_log_print(ANDROID_LOG_INFO, "decodeFrame",  "Time:%u\n",s_elapsed_time);

	if (got_picture)
	{
		//Y, U, V
//		int i;
//		for(i=0;i<orig->height;i++){
//			fwrite(orig->data[0]+orig->linesize[0]*i,1,orig->width,fp_out);
//		}
//		for(i=0;i<orig->height/2;i++){
//			fwrite(orig->data[1]+orig->linesize[1]*i,1,orig->width/2,fp_out);
//		}
//		for(i=0;i<orig->height/2;i++){
//			fwrite(orig->data[2]+orig->linesize[2]*i,1,orig->width/2,fp_out);
//		}


		//__android_log_print(ANDROID_LOG_INFO, "decodeFrame",
		//	"decode success line:%d, resolution(%d * %d)", __LINE__, pdec->av_context->width, pdec->av_context->height);
		mblk_t *om = get_as_yuvmsg(NULL, pdec,orig);
		android_ogl_display_set_yuv(env, om, pdec->winid, pdec->frameid, 
			pdec->av_context->width, pdec->av_context->height);
		freemsg(om);
	}
	pthread_mutex_unlock(&avpkt_mutex);
//	len = avcodec_decode_audio4(c, decoded_frame, &got_frame, &avpkt);
//	if (len < 0) 
//	{			
//		fprintf(stderr, "Error while decoding\n"); 
//		exit(1);		
//	}		 
//	if (got_frame) 
//	{		
//		int data_size = av_get_bytes_per_sample(c->sample_fmt);
//  }


	return 1;
}

void DecodeUnInit(JNIEnv* env, int phandle)
{
	if (fp != NULL) {
		fclose(fp);
		fp = NULL;
	}

	DecState *pdec=(DecState *)phandle;
	android_ogl_display_deinit(env, pdec->winid, pdec->frameid);
	pthread_mutex_lock(&avpkt_mutex);
	if (pdec->av_context!=NULL)
	{
		avcodec_close(pdec->av_context);
		av_free(pdec->av_context);
		pdec->av_context=NULL;
	}
	if (pdec->yuv_msg!=NULL)
		freemsg(pdec->yuv_msg);
	if (pdec->sws_ctx!=NULL)
	{
		sws_freeContext(pdec->sws_ctx);
		pdec->sws_ctx=NULL;
	}
	
	if (orig != NULL)
	{
		av_frame_free(&orig);
		orig = NULL;
	}
	pthread_mutex_unlock(&avpkt_mutex);
	__android_log_print(ANDROID_LOG_ERROR, "videodec", "line: %d", __LINE__);
	free(pdec);
	return;
}

int H265DecodeInit(JNIEnv* env, jobject winid)
{
	__android_log_print(ANDROID_LOG_ERROR, "videodec", "DecodeInit閿涗緤绱掗敍?");
	DecState *pdec=(DecState *)malloc(sizeof(DecState)*1);

	avcodec_register_all();

	pdec->av_codec=NULL;
	pdec->audio_codec = NULL;
	pdec->codec=AV_CODEC_ID_H265;
	pdec->input=NULL;
	pdec->yuv_msg=NULL;
	pdec->output_pix_fmt=AV_PIX_FMT_YUV420P;
	pdec->snow_initialized=FALSE;
	pdec->outbuf.w=0;
	pdec->outbuf.h=0;
	pdec->sws_ctx=NULL;

	//local test
	pdec->pCodecParserCtx=NULL;
	pdec->first_time=1;
	pdec->flush=0;

	pdec->av_codec=avcodec_find_decoder(pdec->codec);
	if(pdec->av_codec==NULL)
	{
		ms_error("Line: %d,Could not find decoder %i!",__LINE__,pdec->codec);
		return 0;
	}

	pdec->av_context = avcodec_alloc_context3(pdec->av_codec);

	//local test
	pdec->pCodecParserCtx=av_parser_init(pdec->codec);

	int error;
	/* we must know picture size before initializing snow decoder*/

	error=avcodec_open2(pdec->av_context, pdec->av_codec, NULL);
	if (error!=0)
		ms_error("avcodec_open() failed: %i",error);
	if (pdec->codec==AV_CODEC_ID_H265 && pdec->dci_size>0)
	{
		pdec->av_context->extradata=pdec->dci;
		pdec->av_context->extradata_size=pdec->dci_size;
	}

	__android_log_print(ANDROID_LOG_INFO, "videodec", "before pdec->frameid = android_ogl_display_init(env, winid);");


	pdec->frameid = android_ogl_display_init(env, winid);
	pdec->winid = winid;

	__android_log_print(ANDROID_LOG_INFO, "videodec", "%d:pdec->frameid(%d),env(%p)",
		__LINE__, pdec->frameid, env);
	av_init_packet(&avpkt);
	orig = av_frame_alloc();

	return (int)pdec;
}

int H265DecodeFrame(JNIEnv* env, int phandle, uint8_t* addr, int len,FILE *fp_in,FILE *fp_out)
{
	/*DecState *pdec=(DecState *)phandle;
	int got_picture=0;
	//__android_log_print(ANDROID_LOG_ERROR, "decodeFrame",  "avcodec_decode_video ........................\n");
	//if(avcodec_decode_video(&pdec->av_context,&orig,&got_picture,addr, len) < 0)

	pthread_mutex_lock(&avpkt_mutex);
	if (orig == NULL)
	{
		pthread_mutex_unlock(&avpkt_mutex);
		return 1;
	}
	if (fp == NULL) {
		//fp = fopen("/sdcard/debug.h264", "wb");
	}
	if (fp != NULL) {
		fwrite(addr, 1, len, fp);
	}
	avpkt.data = addr;
	avpkt.size = len;
	if(avcodec_decode_video2(pdec->av_context,orig,&got_picture,&avpkt) != len)
	{
		pthread_mutex_unlock(&avpkt_mutex);
		if (len <= 4 ||
			((((addr[4]&0x7E) >> 1) != 32)&&(((addr[4]&0x7E) >> 1) != 33)&&(((addr[4]&0x7E) >> 1) != 34)
			&&(((addr[3]&0x7E) >> 1) != 32)&&(((addr[3]&0x7E) >> 1) != 33)&&(((addr[3]&0x7E) >> 1) != 34)))
				__android_log_print(ANDROID_LOG_ERROR, "decodeFrame", "decode error line:%d, addr[3,4](%x,%x)", __LINE__, addr[3], addr[4]);
		return 0;
	}
	if (got_picture)
	{
		__android_log_print(ANDROID_LOG_INFO, "decodeFrame",
			"decode success line:%d, resolution(%d * %d)", __LINE__, pdec->av_context->width, pdec->av_context->height);
		mblk_t *om = get_as_yuvmsg(NULL, pdec,orig);
		android_ogl_display_set_yuv(env, om, pdec->winid, pdec->frameid,
			pdec->av_context->width, pdec->av_context->height);
		freemsg(om);
	}
	pthread_mutex_unlock(&avpkt_mutex);
	return 1;
	*/

/*
 * local test;
 * fp_in is h265 file
 * fp_out is yuv file
 */

	DecState *pdec=(DecState *)phandle;
	int got_picture=0;
	int framenum=0;
	TIMER sumstart,sumend;
	UWORD32 sumtime;


	//test .h265 file from system.
	const int in_buffer_size=65536;
	uint8_t in_buffer[in_buffer_size + FF_INPUT_BUFFER_PADDING_SIZE];
	memset(in_buffer, 0, (in_buffer_size + FF_INPUT_BUFFER_PADDING_SIZE)*sizeof(uint8_t));
	int ret;

	//test fps


	while (1)
	{
		pdec->cur_size = fread(in_buffer, 1, in_buffer_size, fp_in);
		if (pdec->cur_size == 0)
		{
			break;
			//pdec->flush=1;
		}

		pdec->cur_ptr=in_buffer;

		GETTIME(&sumstart);
		while (pdec->cur_size>0)
		{
			int len = av_parser_parse2(
					pdec->pCodecParserCtx, pdec->av_context,
					&avpkt.data, &avpkt.size,
					pdec->cur_ptr , pdec->cur_size ,
					AV_NOPTS_VALUE, AV_NOPTS_VALUE, AV_NOPTS_VALUE);

			pdec->cur_ptr += len;
			pdec->cur_size -= len;

			if(avpkt.size==0)
			{
				//return 1;
				continue;
			}

			ret = avcodec_decode_video2(pdec->av_context, orig, &got_picture, &avpkt);
			if (ret < 0)
			{
				//printf("Decode Error.\n");
				return ret;
			}
			if (got_picture)
			{
				framenum++;
				if(pdec->first_time)
				{
					pdec->first_time=0;
				}


				mblk_t *om = get_as_yuvmsg(NULL, pdec,orig);
				android_ogl_display_set_yuv(env, om, pdec->winid, pdec->frameid,
							pdec->av_context->width, pdec->av_context->height);
				freemsg(om);

			}
			av_free_packet(&avpkt);
		}
		GETTIME(&sumend);
		ELAPSEDTIME(sumstart, sumend, sumtime, frequency);
		__android_log_print(ANDROID_LOG_ERROR, "decodeFrame",  "Frame number:%d;Every frame.Time:%5u;Fps:%5u\n",framenum,sumtime,1000000*framenum/sumtime);
		//	GETTIME(&sumstart);
		framenum=0;
	}
	return 1;
}


void H265DecodeUnInit(JNIEnv* env, int phandle)
{

	DecState *pdec=(DecState *)phandle;
	av_parser_close(pdec->pCodecParserCtx);
	DecodeUnInit(env,phandle);
	return;
}

int HevcDecodeInit(JNIEnv* env, jobject winid,FILE *fp_in, FILE *fp_out)
{
    int ret;

	HevcDecState *pdec=(HevcDecState *)malloc(sizeof(HevcDecState)*1);
	//test fps
	pdec->threetime=0;
	pdec->framenum=0;
	pdec->displaytime=0;
	pdec->parsertime=0;


	pdec->firstframe=0;
	pdec->frameid = android_ogl_display_init(env, winid);
	pdec->winid = winid;
	pdec->ps_piclen_file = NULL;
	pdec->ps_ip_file = NULL;
	pdec->ps_op_file = NULL;
	pdec->ps_op_chksum_file = NULL;
	pdec->pu1_bs_buf = NULL;
	pdec->u4_num_bytes_dec = 0;
	pdec->file_pos = 0;
	pdec->u4_ip_frm_ts = 0, pdec->u4_op_frm_ts = 0;
	pdec->u4_bytes_remaining = 0;
	pdec->frm_cnt = 0;
	pdec->width = 0, pdec->height = 0;

	/***********************************************************************/
	/*                  Initialize Application parameters                  */
	/***********************************************************************/
	strcpy(pdec->s_app_ctx.ac_ip_fname, "\0");
	pdec->s_app_ctx.dump_q_wr_idx = 0;
	pdec->s_app_ctx.dump_q_rd_idx = 0;
	pdec->s_app_ctx.display_thread_created = 0;
	pdec->s_app_ctx.disp_q_wr_idx = 0;
	pdec->s_app_ctx.disp_q_rd_idx = 0;
	pdec->s_app_ctx.disp_delay = 0;
	pdec->s_app_ctx.loopback = 0;
	pdec->s_app_ctx.display = 0;
	pdec->s_app_ctx.full_screen = 0;
	pdec->s_app_ctx.u4_piclen_flag = 0;
	pdec->s_app_ctx.fps = 30;
	pdec->file_pos = 0;
	pdec->total_bytes_comsumed = 0;
	pdec->u4_ip_frm_ts = 0;
	pdec->u4_op_frm_ts = 0;
	pdec->s_app_ctx.i4_degrade_type = 0;
	pdec->s_app_ctx.i4_degrade_pics = 0;
	pdec->s_app_ctx.e_arch = ARCH_ARM_A9Q;
	pdec->s_app_ctx.e_soc = SOC_GENERIC;
	pdec->s_app_ctx.u4_strd = STRIDE;
	pdec->s_app_ctx.display_thread_handle=malloc(ithread_get_handle_size());
	pdec->s_app_ctx.quit          = 0;
	pdec->s_app_ctx.paused        = 0;
	//s_app_ctx.u4_output_present = 0;
	pdec->s_app_ctx.get_stride = &default_get_stride;
	pdec->s_app_ctx.get_color_fmt = &default_get_color_fmt;
	/* Set function pointers for display */
	pdec->s_app_ctx.display_deinit_flag = 0;

	pdec->s_app_ctx.u4_file_save_flag=0;
	pdec->s_app_ctx.e_output_chroma_format = IV_YUV_420P;
	pdec->s_app_ctx.u4_max_frm_ts=-1;
	pdec->s_app_ctx.u4_num_cores=android_getCpuCount();
	pdec->s_app_ctx.share_disp_buf=0;

	pdec->ps_ip_file = fp_in;
	if(NULL == pdec->ps_ip_file)
	{
		sprintf(pdec->ac_error_str, "Could not open input file %s",
				pdec->s_app_ctx.ac_ip_fname);
		codec_exit(pdec->ac_error_str);
	}

	if(1 == pdec->s_app_ctx.u4_file_save_flag)
	{
		pdec->ps_op_file =fp_out;
		if(NULL == pdec->ps_op_file)
		{
			sprintf(pdec->ac_error_str, "Could not open output file %s",
					pdec->s_app_ctx.ac_op_fname);
			codec_exit(pdec->ac_error_str);
		}
	}

	//haisi parser
	fseek( pdec->ps_ip_file, 0, SEEK_END);
	pdec->iFileLen = ftell(pdec->ps_ip_file);
	fseek( pdec->ps_ip_file, 0, SEEK_SET);
	pdec->pInputStream = (unsigned char *) malloc(pdec->iFileLen);
	if (NULL == pdec->pInputStream)
	{
		fprintf(stderr, "Unable to malloc stream buffer (Size %d).\n", pdec->iFileLen);
		//free resource
		return -1;
	}
	fread(pdec->pInputStream, 1, pdec->iFileLen, pdec->ps_ip_file);
	pdec->pStream = pdec->pInputStream;

	{
		pdec->ps_out_buf = (ivd_out_bufdesc_t *)malloc(sizeof(ivd_out_bufdesc_t));
		/*****************************************************************************/
		/*   API Call: Initialize the Decoder                                        */
		/*****************************************************************************/
		{
			ihevcd_cxa_create_ip_t s_create_ip;
			ihevcd_cxa_create_op_t s_create_op;
			void *fxns = &ivd_cxa_api_function;

			s_create_ip.s_ivd_create_ip_t.e_cmd = IVD_CMD_CREATE;
			s_create_ip.s_ivd_create_ip_t.u4_share_disp_buf = pdec->s_app_ctx.share_disp_buf;
			s_create_ip.s_ivd_create_ip_t.e_output_format = (IV_COLOR_FORMAT_T)pdec->s_app_ctx.e_output_chroma_format;
			s_create_ip.s_ivd_create_ip_t.pf_aligned_alloc = ihevca_aligned_malloc;
			s_create_ip.s_ivd_create_ip_t.pf_aligned_free = ihevca_aligned_free;
			s_create_ip.s_ivd_create_ip_t.pv_mem_ctxt = NULL;
			s_create_ip.s_ivd_create_ip_t.u4_size = sizeof(ihevcd_cxa_create_ip_t);
			s_create_op.s_ivd_create_op_t.u4_size = sizeof(ihevcd_cxa_create_op_t);

			ret = ivd_cxa_api_function(NULL, (void *)&s_create_ip,
									   (void *)&s_create_op);
			if(ret != IV_SUCCESS)
			{
				sprintf(pdec->ac_error_str, "Error in Create %8x\n",
						s_create_op.s_ivd_create_op_t.u4_error_code);
				codec_exit(pdec->ac_error_str);
			}
			pdec->codec_obj = (iv_obj_t*)s_create_op.s_ivd_create_op_t.pv_handle;
			pdec->codec_obj->pv_fxns = fxns;
			pdec->codec_obj->u4_size = sizeof(iv_obj_t);
			pdec->s_app_ctx.cocodec_obj = pdec->codec_obj;
		}

		//test by xulei
		printf("step2,In Create decoder instance.\n");

	}


	/*************************************************************************/
	/* set num of cores                                                      */
	/*************************************************************************/
	{

		ihevcd_cxa_ctl_set_num_cores_ip_t s_ctl_set_cores_ip;
		ihevcd_cxa_ctl_set_num_cores_op_t s_ctl_set_cores_op;

		s_ctl_set_cores_ip.e_cmd = IVD_CMD_VIDEO_CTL;
		s_ctl_set_cores_ip.e_sub_cmd = (IVD_CONTROL_API_COMMAND_TYPE_T)IHEVCD_CXA_CMD_CTL_SET_NUM_CORES;
		s_ctl_set_cores_ip.u4_num_cores = pdec->s_app_ctx.u4_num_cores;
		s_ctl_set_cores_ip.u4_size = sizeof(ihevcd_cxa_ctl_set_num_cores_ip_t);
		s_ctl_set_cores_op.u4_size = sizeof(ihevcd_cxa_ctl_set_num_cores_op_t);

		ret = ivd_cxa_api_function((iv_obj_t *)pdec->codec_obj, (void *)&s_ctl_set_cores_ip,
								   (void *)&s_ctl_set_cores_op);
		if(ret != IV_SUCCESS)
		{
			sprintf(pdec->ac_error_str, "\nError in setting number of cores");
			codec_exit(pdec->ac_error_str);
		}
	}

	/*************************************************************************/
	/* set processsor                                                        */
	/*************************************************************************/
	{

		ihevcd_cxa_ctl_set_processor_ip_t s_ctl_set_num_processor_ip;
		ihevcd_cxa_ctl_set_processor_op_t s_ctl_set_num_processor_op;

		s_ctl_set_num_processor_ip.e_cmd = IVD_CMD_VIDEO_CTL;
		s_ctl_set_num_processor_ip.e_sub_cmd = (IVD_CONTROL_API_COMMAND_TYPE_T)IHEVCD_CXA_CMD_CTL_SET_PROCESSOR;
		s_ctl_set_num_processor_ip.u4_arch = pdec->s_app_ctx.e_arch;
		s_ctl_set_num_processor_ip.u4_soc = pdec->s_app_ctx.e_soc;
		s_ctl_set_num_processor_ip.u4_size = sizeof(ihevcd_cxa_ctl_set_processor_ip_t);
		s_ctl_set_num_processor_op.u4_size = sizeof(ihevcd_cxa_ctl_set_processor_op_t);

		ret = ivd_cxa_api_function((iv_obj_t *)pdec->codec_obj, (void *)&s_ctl_set_num_processor_ip,
								   (void *)&s_ctl_set_num_processor_op);
		if(ret != IV_SUCCESS)
		{
			sprintf(pdec->ac_error_str, "\nError in setting Processor type");
			codec_exit(pdec->ac_error_str);
		}
	}

	flush_output(pdec->codec_obj, &pdec->s_app_ctx, pdec->ps_out_buf,
				 pdec->pu1_bs_buf, &pdec->u4_op_frm_ts,
				 pdec->ps_op_file, pdec->ps_op_chksum_file,
				 pdec->u4_ip_frm_ts, pdec->u4_bytes_remaining);

	return (int)pdec;
}

int HevcDecodeFrame(JNIEnv* env, jobject winid,int phandle, uint8_t* addr, int len,FILE *fp_in, FILE *fp_out)
{
    int ret;
    int framenum=0;
    TIMER sumstart,sumend;
    UWORD32 sumtime;
    TIMER onestart,oneend;
    UWORD32 onetime;


    HevcDecState *pdec=(HevcDecState *)phandle;
    INT32 iNaluLen;
    IH265DEC_INARGS stInArgs={0};

    if(pdec->firstframe==0)
    {
    	pdec->firstframe=1;
    	/*****************************************************************************/
		/*   Decode header to get width and height and buffer sizes                  */
		/*****************************************************************************/
		{
			ivd_video_decode_ip_t s_video_decode_ip;
			ivd_video_decode_op_t s_video_decode_op;

			{
				ivd_ctl_set_config_ip_t s_ctl_ip;
				ivd_ctl_set_config_op_t s_ctl_op;


				s_ctl_ip.u4_disp_wd = STRIDE;
				if(1 == pdec->s_app_ctx.display)
					s_ctl_ip.u4_disp_wd = pdec->s_app_ctx.get_stride();

				s_ctl_ip.e_frm_skip_mode = IVD_SKIP_NONE;
				s_ctl_ip.e_frm_out_mode = IVD_DISPLAY_FRAME_OUT;
				s_ctl_ip.e_vid_dec_mode = IVD_DECODE_HEADER;
				s_ctl_ip.e_cmd = IVD_CMD_VIDEO_CTL;
				s_ctl_ip.e_sub_cmd = IVD_CMD_CTL_SETPARAMS;
				s_ctl_ip.u4_size = sizeof(ivd_ctl_set_config_ip_t);
				s_ctl_op.u4_size = sizeof(ivd_ctl_set_config_op_t);

				ret = ivd_cxa_api_function((iv_obj_t*)pdec->codec_obj, (void *)&s_ctl_ip,
										   (void *)&s_ctl_op);
				if(ret != IV_SUCCESS)
				{
					sprintf(pdec->ac_error_str,
							"\nError in setting the codec in header decode mode");
					codec_exit(pdec->ac_error_str);
				}
			}

			do
			{
				H265DecLoadAU(pdec->pStream, pdec->iFileLen, &iNaluLen);
				//stInArgs.eDecodeMode = (iNaluLen>0) ? IH265D_DECODE :IH265D_DECODE_END;
				stInArgs.pStream = pdec->pStream;
				stInArgs.uiStreamLen = iNaluLen;
				pdec->pStream += iNaluLen;
				pdec->iFileLen-= iNaluLen;

				s_video_decode_ip.e_cmd = IVD_CMD_VIDEO_DECODE;
				s_video_decode_ip.u4_ts = pdec->u4_ip_frm_ts;
				s_video_decode_ip.pv_stream_buffer = stInArgs.pStream;
				s_video_decode_ip.u4_num_Bytes = stInArgs.uiStreamLen;
				s_video_decode_ip.u4_size = sizeof(ivd_video_decode_ip_t);
				s_video_decode_op.u4_size = sizeof(ivd_video_decode_op_t);

				/*****************************************************************************/
				/*   API Call: Header Decode                                                  */
				/*****************************************************************************/
				ret = ivd_cxa_api_function((iv_obj_t *)pdec->codec_obj, (void *)&s_video_decode_ip,
										   (void *)&s_video_decode_op);

				if(ret != IV_SUCCESS)
				{
					sprintf(pdec->ac_error_str, "\nError in header decode %x",
							s_video_decode_op.u4_error_code);
					// codec_exit(pdec->ac_error_str);
				}

				pdec->u4_num_bytes_dec = s_video_decode_op.u4_num_bytes_consumed;
				pdec->file_pos += pdec->u4_num_bytes_dec;
				pdec->total_bytes_comsumed += pdec->u4_num_bytes_dec;
			}while(ret != IV_SUCCESS);

			__android_log_print(ANDROID_LOG_ERROR, "decodeFrame",  "Decode header,wd=%d ht=%d\n",s_video_decode_op.u4_pic_wd,s_video_decode_op.u4_pic_ht);

			/* copy pic_wd and pic_ht to initialize buffers */
			pdec->s_app_ctx.u4_pic_wd = s_video_decode_op.u4_pic_wd;
			pdec->s_app_ctx.u4_pic_ht = s_video_decode_op.u4_pic_ht;


			{

				ivd_ctl_getbufinfo_ip_t s_ctl_ip;
				ivd_ctl_getbufinfo_op_t s_ctl_op;
				WORD32 outlen = 0;

				s_ctl_ip.e_cmd = IVD_CMD_VIDEO_CTL;
				s_ctl_ip.e_sub_cmd = IVD_CMD_CTL_GETBUFINFO;
				s_ctl_ip.u4_size = sizeof(ivd_ctl_getbufinfo_ip_t);
				s_ctl_op.u4_size = sizeof(ivd_ctl_getbufinfo_op_t);
				ret = ivd_cxa_api_function((iv_obj_t*)pdec->codec_obj, (void *)&s_ctl_ip,
										   (void *)&s_ctl_op);
				if(ret != IV_SUCCESS)
				{
					sprintf(pdec->ac_error_str, "Error in Get Buf Info %x", s_ctl_op.u4_error_code);
					codec_exit(pdec->ac_error_str);
				}

				//test by xulei
				printf("s_ctl_op->u4_min_in_buf_size:%d  s_ctl_op->u4_min_out_buf_size:%d\n",s_ctl_op.u4_min_in_buf_size[0],s_ctl_op.u4_min_out_buf_size[0]);

				/* Allocate bitstream buffer */
				pdec->u4_ip_buf_len = s_ctl_op.u4_min_in_buf_size[0];

				pdec->pu1_bs_buf = (UWORD8 *)malloc(pdec->u4_ip_buf_len);

				if(pdec->pu1_bs_buf == NULL)
				{
					sprintf(pdec->ac_error_str,
							"\nAllocation failure for input buffer of i4_size %d",
							pdec->u4_ip_buf_len);
					codec_exit(pdec->ac_error_str);
				}

				/* Allocate output buffer only if display buffers are not shared */
				/* Or if shared and output is 420P */
				if((0 == pdec->s_app_ctx.share_disp_buf) || (IV_YUV_420P == pdec->s_app_ctx.e_output_chroma_format))
				{
					pdec->ps_out_buf->u4_min_out_buf_size[0] =
									s_ctl_op.u4_min_out_buf_size[0];
					pdec->ps_out_buf->u4_min_out_buf_size[1] =
									s_ctl_op.u4_min_out_buf_size[1];
					pdec->ps_out_buf->u4_min_out_buf_size[2] =
									s_ctl_op.u4_min_out_buf_size[2];

					outlen = s_ctl_op.u4_min_out_buf_size[0];
					if(s_ctl_op.u4_min_num_out_bufs > 1)
						outlen += s_ctl_op.u4_min_out_buf_size[1];

					if(s_ctl_op.u4_min_num_out_bufs > 2)
						outlen += s_ctl_op.u4_min_out_buf_size[2];

					pdec->ps_out_buf->pu1_bufs[0] = (UWORD8 *)malloc(outlen);
					if(pdec->ps_out_buf->pu1_bufs[0] == NULL)
					{
						sprintf(pdec->ac_error_str, "\nAllocation failure for output buffer of i4_size %d",
								outlen);
						codec_exit(pdec->ac_error_str);
					}

					if(s_ctl_op.u4_min_num_out_bufs > 1)
						pdec->ps_out_buf->pu1_bufs[1] = pdec->ps_out_buf->pu1_bufs[0]
										+ (s_ctl_op.u4_min_out_buf_size[0]);

					if(s_ctl_op.u4_min_num_out_bufs > 2)
						pdec->ps_out_buf->pu1_bufs[2] = pdec->ps_out_buf->pu1_bufs[1]
										+ (s_ctl_op.u4_min_out_buf_size[1]);

					pdec->ps_out_buf->u4_num_bufs = s_ctl_op.u4_min_num_out_bufs;
				}

			}
		}

		/*************************************************************************/
		/* Set the decoder in frame decode mode. It was set in header decode     */
		/* mode earlier                                                          */
		/*************************************************************************/
		{

			ivd_ctl_set_config_ip_t s_ctl_ip;
			ivd_ctl_set_config_op_t s_ctl_op;

			s_ctl_ip.u4_disp_wd = STRIDE;
			if(1 == pdec->s_app_ctx.display)
				s_ctl_ip.u4_disp_wd = pdec->s_app_ctx.get_stride();
			s_ctl_ip.e_frm_skip_mode = IVD_SKIP_NONE;

			s_ctl_ip.e_frm_out_mode = IVD_DISPLAY_FRAME_OUT;
			s_ctl_ip.e_vid_dec_mode = IVD_DECODE_FRAME;
			s_ctl_ip.e_cmd = IVD_CMD_VIDEO_CTL;
			s_ctl_ip.e_sub_cmd = IVD_CMD_CTL_SETPARAMS;
			s_ctl_ip.u4_size = sizeof(ivd_ctl_set_config_ip_t);

			s_ctl_op.u4_size = sizeof(ivd_ctl_set_config_op_t);

			ret = ivd_cxa_api_function((iv_obj_t *)pdec->codec_obj, (void *)&s_ctl_ip, (void *)&s_ctl_op);

			if(IV_SUCCESS != ret)
			{
				sprintf(pdec->ac_error_str, "Error in Set Parameters");
				//codec_exit(ac_error_str);
			}

		}
    }
    else
    {
    	/*************************************************************************/
		/* If required disable deblocking and sao at given level                 */
		/*************************************************************************/
		set_degrade(pdec->codec_obj, pdec->s_app_ctx.i4_degrade_type, pdec->s_app_ctx.i4_degrade_pics);

		while(pdec->framenum<3000)
		{
			if(pdec->framenum==3000)
			{
				pdec->framenum=0;
			}
			GETTIME(&sumstart);
			GETTIME(&onestart);
			//haisi parser
			H265DecLoadAU(pdec->pStream, pdec->iFileLen, &iNaluLen);
			//stInArgs.eDecodeMode = (iNaluLen>0) ? IH265D_DECODE :IH265D_DECODE_END;
			stInArgs.pStream = pdec->pStream;
			stInArgs.uiStreamLen = iNaluLen;
			pdec->pStream += iNaluLen;
			pdec->iFileLen-= iNaluLen;
			GETTIME(&oneend);
			ELAPSEDTIME(onestart, oneend, onetime, frequency);
			__android_log_print(ANDROID_LOG_ERROR, "decodeFrame",  "Time:%5u;",onetime);
			pdec->parsertime+=onetime;

			if(iNaluLen>0)
			{
				ivd_video_decode_ip_t s_video_decode_ip;
				ivd_video_decode_op_t s_video_decode_op;

				s_video_decode_ip.e_cmd = IVD_CMD_VIDEO_DECODE;
				s_video_decode_ip.u4_ts = pdec->u4_ip_frm_ts;
				s_video_decode_ip.pv_stream_buffer = stInArgs.pStream;
				s_video_decode_ip.u4_num_Bytes = stInArgs.uiStreamLen;
				s_video_decode_ip.u4_size = sizeof(ivd_video_decode_ip_t);
				s_video_decode_ip.s_out_buffer.u4_min_out_buf_size[0] =
								pdec->ps_out_buf->u4_min_out_buf_size[0];
				s_video_decode_ip.s_out_buffer.u4_min_out_buf_size[1] =
								pdec->ps_out_buf->u4_min_out_buf_size[1];
				s_video_decode_ip.s_out_buffer.u4_min_out_buf_size[2] =
								pdec->ps_out_buf->u4_min_out_buf_size[2];
				s_video_decode_ip.s_out_buffer.pu1_bufs[0] =
								pdec->ps_out_buf->pu1_bufs[0];
				s_video_decode_ip.s_out_buffer.pu1_bufs[1] =
								pdec->ps_out_buf->pu1_bufs[1];
				s_video_decode_ip.s_out_buffer.pu1_bufs[2] =
								pdec->ps_out_buf->pu1_bufs[2];
				s_video_decode_ip.s_out_buffer.u4_num_bufs =
								pdec->ps_out_buf->u4_num_bufs;
				s_video_decode_op.u4_size = sizeof(ivd_video_decode_op_t);

				ret = ivd_cxa_api_function((iv_obj_t *)pdec->codec_obj, (void *)&s_video_decode_ip,
										   (void *)&s_video_decode_op);


				if(IV_B_FRAME == s_video_decode_op.e_pic_type)
					pdec->s_app_ctx.b_pic_present |= 1;

				pdec->u4_num_bytes_dec = s_video_decode_op.u4_num_bytes_consumed;

				pdec->file_pos += pdec->u4_num_bytes_dec;
				pdec->total_bytes_comsumed += pdec->u4_num_bytes_dec;
				pdec->u4_ip_frm_ts++;

				if(1 == s_video_decode_op.u4_output_present)
				{
					pdec->width = s_video_decode_op.s_disp_frm_buf.u4_y_wd;
					pdec->height = s_video_decode_op.s_disp_frm_buf.u4_y_ht;

					pdec->u4_op_frm_ts++;
					framenum++;
					pdec->framenum++;

					GETTIME(&onestart);
					mblk_t *om = gethevc_as_yuvmsg(NULL, &s_video_decode_op, pdec);
					android_ogl_display_set_yuv(env, om, pdec->winid, pdec->frameid,
					pdec->width, pdec->height);
					freemsg(om);
					GETTIME(&oneend);
					ELAPSEDTIME(onestart, oneend, onetime, frequency);
					__android_log_print(ANDROID_LOG_ERROR, "decodeFrame",  "Display Time:%5u;",onetime);
					pdec->displaytime+=onetime;
				}
				else
				{
					if((s_video_decode_op.u4_error_code >> IVD_FATALERROR) & 1)
					{
						printf("Fatal error\n");
						return 0;
					}
				}
			}
			else
			{
				return 0;
			}
			GETTIME(&sumend);
			ELAPSEDTIME(sumstart, sumend, sumtime, frequency);
			__android_log_print(ANDROID_LOG_ERROR, "decodeFrame",  "Frame number:%d;Every frame.Time:%5u;Fps:%5u\n",framenum,sumtime,1000000*framenum/sumtime);
		//	GETTIME(&sumstart);
			framenum=0;
			pdec->threetime+=sumtime;
			if(pdec->framenum==3000)
			{
				__android_log_print(ANDROID_LOG_ERROR, "decodeFrame",  "Every 3000 frame.Time:%5u;Parser time:%u;Display time:%u;Fps:%5u\n",pdec->threetime,pdec->parsertime,pdec->displaytime,1000000*pdec->framenum/pdec->threetime);
				pdec->threetime=0;
			}
		}


    }
	return 1;
}
int HevcDecodeFramedelete(JNIEnv* env, jobject winid,int phandle, uint8_t* addr, int len,FILE *fp_in, FILE *fp_out)
{
    int ret;
    int framenum=0;
    TIMER sumstart,sumend;
    UWORD32 sumtime;


    HevcDecState *pdec=(HevcDecState *)phandle;

    GETTIME(&sumstart);
    const int in_buffer_size=65536;
    UWORD8 in_buffer[in_buffer_size+32];
	memset(in_buffer, 0, (in_buffer_size+32)*sizeof(UWORD8));
	pdec->cur_size = fread(in_buffer, sizeof(UWORD8),in_buffer_size, pdec->ps_ip_file);
	if(pdec->cur_size == 0)
	{
		return 0;
	}
	pdec->cur_ptr=in_buffer;
	pdec->pu1_bs_buf = (UWORD8 *)malloc(pdec->u4_ip_buf_len);

    /*************************************************************************/
    /* If required disable deblocking and sao at given level                 */
    /*************************************************************************/
    set_degrade(pdec->codec_obj, pdec->s_app_ctx.i4_degrade_type, pdec->s_app_ctx.i4_degrade_pics);




    while(pdec->u4_op_frm_ts < (pdec->s_app_ctx.u4_max_frm_ts + pdec->s_app_ctx.disp_delay))
    {
    	int len = av_parser_parse2(
    						pdec->pCodecParserCtx, pdec->av_context,
    						&pdec->pu1_bs_buf, &pdec->u4_bytes_remaining,
    						pdec->cur_ptr , pdec->cur_size ,
    						AV_NOPTS_VALUE, AV_NOPTS_VALUE, AV_NOPTS_VALUE);

		pdec->cur_ptr += len;
		pdec->cur_size -= len;
		if(pdec->u4_bytes_remaining==0)
			break;


    	if(pdec->u4_bytes_remaining==0)
    				break;

        {
            ivd_video_decode_ip_t s_video_decode_ip;
            ivd_video_decode_op_t s_video_decode_op;


            s_video_decode_ip.e_cmd = IVD_CMD_VIDEO_DECODE;
            s_video_decode_ip.u4_ts = pdec->u4_ip_frm_ts;
            s_video_decode_ip.pv_stream_buffer = pdec->pu1_bs_buf;
            s_video_decode_ip.u4_num_Bytes = pdec->u4_bytes_remaining;
            s_video_decode_ip.u4_size = sizeof(ivd_video_decode_ip_t);
            s_video_decode_ip.s_out_buffer.u4_min_out_buf_size[0] =
                            pdec->ps_out_buf->u4_min_out_buf_size[0];
            s_video_decode_ip.s_out_buffer.u4_min_out_buf_size[1] =
                            pdec->ps_out_buf->u4_min_out_buf_size[1];
            s_video_decode_ip.s_out_buffer.u4_min_out_buf_size[2] =
                            pdec->ps_out_buf->u4_min_out_buf_size[2];

            s_video_decode_ip.s_out_buffer.pu1_bufs[0] =
                            pdec->ps_out_buf->pu1_bufs[0];
            s_video_decode_ip.s_out_buffer.pu1_bufs[1] =
                            pdec->ps_out_buf->pu1_bufs[1];
            s_video_decode_ip.s_out_buffer.pu1_bufs[2] =
                            pdec->ps_out_buf->pu1_bufs[2];
            s_video_decode_ip.s_out_buffer.u4_num_bufs =
                            pdec->ps_out_buf->u4_num_bufs;
            s_video_decode_op.u4_size = sizeof(ivd_video_decode_op_t);

            /*****************************************************************************/
            /*   API Call: Video Decode                                                  */
            /*****************************************************************************/



            ret = ivd_cxa_api_function((iv_obj_t *)pdec->codec_obj, (void *)&s_video_decode_ip,
                                       (void *)&s_video_decode_op);


            if(IV_B_FRAME == s_video_decode_op.e_pic_type)
                pdec->s_app_ctx.b_pic_present |= 1;

            pdec->u4_num_bytes_dec = s_video_decode_op.u4_num_bytes_consumed;

            pdec->file_pos += pdec->u4_num_bytes_dec;
            pdec->total_bytes_comsumed += pdec->u4_num_bytes_dec;
            pdec->u4_ip_frm_ts++;


            if(1 == s_video_decode_op.u4_output_present)
            {
                pdec->width = s_video_decode_op.s_disp_frm_buf.u4_y_wd;
                pdec->height = s_video_decode_op.s_disp_frm_buf.u4_y_ht;

                pdec->u4_op_frm_ts++;
                framenum++;






               mblk_t *om = gethevc_as_yuvmsg(NULL, &s_video_decode_op, pdec);
			   android_ogl_display_set_yuv(env, om, pdec->winid, pdec->frameid,
			   pdec->width, pdec->height);
			   freemsg(om);
            }
            else
            {
                if((s_video_decode_op.u4_error_code >> IVD_FATALERROR) & 1)
                {
                    printf("Fatal error\n");
                    break;
                }
            }

        }
    }
    GETTIME(&sumend);
	ELAPSEDTIME(sumstart, sumend, sumtime, frequency);
	__android_log_print(ANDROID_LOG_ERROR, "decodeFrame",  "Frame number:%d;Every frame.Time:%5u;Fps:%5u\n",framenum,sumtime,1000000*framenum/sumtime);
//	GETTIME(&sumstart);
	framenum=0;
	free(pdec->pu1_bs_buf);


	return 1;
}
void HevcDecodeUnInit(JNIEnv* env, int phandle)
{
	HevcDecState *pdec=(HevcDecState *)phandle;
	/***********************************************************************/
	/*   Clear the decoder, close all the files, free all the memory       */
	/***********************************************************************/


	int i;
	int ret;

	{
		ivd_delete_ip_t s_delete_dec_ip;
		ivd_delete_op_t s_delete_dec_op;

		s_delete_dec_ip.e_cmd = IVD_CMD_DELETE;
		s_delete_dec_ip.u4_size = sizeof(ivd_delete_ip_t);
		s_delete_dec_op.u4_size = sizeof(ivd_delete_op_t);

		ret = ivd_cxa_api_function((iv_obj_t *)pdec->codec_obj, (void *)&s_delete_dec_ip,
								   (void *)&s_delete_dec_op);

		if(IV_SUCCESS != ret)
		{
			sprintf(pdec->ac_error_str, "Error in Codec delete");
			codec_exit(pdec->ac_error_str);
		}
	}
	/***********************************************************************/
	/*              Close all the files and free all the memory            */
	/***********************************************************************/
	{
		fclose(pdec->ps_ip_file);

		if(1 == pdec->s_app_ctx.u4_file_save_flag)
		{
			fclose(pdec->ps_op_file);
		}
		if(1 == pdec->s_app_ctx.u4_chksum_save_flag)
		{
			fclose(pdec->ps_op_chksum_file);
		}

	}

	if(0 == pdec->s_app_ctx.share_disp_buf)
	{
		free(pdec->ps_out_buf->pu1_bufs[0]);
	}

	for(i = 0; i < pdec->s_app_ctx.num_disp_buf; i++)
	{
		free(pdec->s_app_ctx.s_disp_buffers[i].pu1_bufs[0]);
	}

	free(pdec->ps_out_buf);
	free(pdec->pu1_bs_buf);
	if(pdec->s_app_ctx.display_thread_handle)
		free(pdec->s_app_ctx.display_thread_handle);

	android_ogl_display_deinit(env, pdec->winid, pdec->frameid);
	if (pdec->yuv_msg!=NULL)
		freemsg(pdec->yuv_msg);
	if (pdec->sws_ctx!=NULL)
	{
		sws_freeContext(pdec->sws_ctx);
		pdec->sws_ctx=NULL;
	}

	__android_log_print(ANDROID_LOG_ERROR, "videodec", "line: %d", __LINE__);
	free(pdec);
	return;
}

int HwH265DecodeInit(JNIEnv* env, jobject winid)
{
	HwDecState *pdec=(HwDecState*)malloc(sizeof(HwDecState));

	//test fps
	pdec->framenum=0;
	pdec->parsertime=0;
	pdec->displaytime=0;
	pdec->threetime=0;

	pdec->fpInFile=fopen("sdcard/test.265","rb");
	pdec->fpOutFile = fopen("sdcard/test.yuv","wb");
	pdec->frameid = android_ogl_display_init(env, winid);
	pdec->winid = winid;
	pdec->yuv_msg=NULL;
	pdec->outbuf.w=0;
	pdec->outbuf.h=0;
	pdec->sws_ctx=NULL;

	pdec->iRet = 0;
	pdec->iInputParam;
	pdec->pInputStream = NULL;
	pdec->uiChannelId = 0x00112233;
	pdec->iFrameIdx = 0;
	pdec->bStreamEnd = 0;
	pdec->LoadCount = 0;
	pdec->hDecoder = NULL;

	pdec->MultiThreadEnable = 0;	// default is single thread mode
	pdec->DispOutput = 1;

	pdec->MultiThreadEnable = 1;//multithread

	//local test
	if(pdec->fpInFile!=NULL)
	{
		fseek( pdec->fpInFile, 0, SEEK_END);
		pdec->iFileLen = ftell( pdec->fpInFile);
		fseek( pdec->fpInFile, 0, SEEK_SET);

		pdec->pInputStream = (unsigned char *) malloc(pdec->iFileLen);
		if (NULL == pdec->pInputStream)
		{
			fprintf(stderr, "Unable to malloc stream buffer (Size %d).\n", pdec->iFileLen);
			//free resource
			return -1;
		}

		fread(pdec->pInputStream, 1, pdec->iFileLen, pdec->fpInFile);
		pdec->pStream = pdec->pInputStream;
		/*create decode handle*/
		{
			pdec->stInitParam.uiChannelID = 0;
			pdec->stInitParam.iMaxWidth   = 1920;
			pdec->stInitParam.iMaxHeight  = 1080;
			pdec->stInitParam.iMaxRefNum  = 3;

			pdec->stInitParam.eThreadType = pdec->MultiThreadEnable? IH265D_MULTI_THREAD: IH265D_SINGLE_THREAD;
			pdec->stInitParam.eOutputOrder= pdec->DispOutput? IH265D_DISPLAY_ORDER:IH265D_DECODE_ORDER;

			pdec->stInitParam.MallocFxn  = HW265D_Malloc;
			pdec->stInitParam.FreeFxn    = HW265D_Free;
			pdec->stInitParam.LogFxn     = HW265D_Log;
		}
		pdec->iRet = IHW265D_Create(&pdec->hDecoder, &pdec->stInitParam);
		if (IHW265D_OK != pdec->iRet)
		{
			fprintf(stderr, "Unable to create decoder.\n");
			//free resource
			return-1;
		}

	}


	return (int)pdec;
}

int HwH265DecodeFrame(JNIEnv* env, int phandle, uint8_t* addr, int len)
{
	HwDecState *pdec=(HwDecState *)phandle;

	IH265DEC_INARGS stInArgs={0};
	IH265DEC_OUTARGS stOutArgs={0};

	/* count decoding time: start */
	pdec->StartTime = GetTime_ms();

	int framenum=0;
	TIMER sumstart,sumend;
	UWORD32 sumtime;
	TIMER onestart,oneend;
	UWORD32 onetime;




	while(!pdec->bStreamEnd)
	{
		GETTIME(&sumstart);
		GETTIME(&onestart);
		INT32 iNaluLen;
		H265DecLoadAU(pdec->pStream, pdec->iFileLen, &iNaluLen);

		stInArgs.eDecodeMode =  iNaluLen>0 ? IH265D_DECODE : IH265D_DECODE_END;
		stInArgs.pStream = pdec->pStream;
		stInArgs.uiStreamLen = iNaluLen;

		pdec->pStream += iNaluLen;
		pdec->iFileLen-= iNaluLen;
		pdec->LoadCount++;
		GETTIME(&oneend);
		ELAPSEDTIME(onestart, oneend, onetime, frequency);
		__android_log_print(ANDROID_LOG_ERROR, "decodeFrame",  "Time:%5u;",onetime);
		pdec->parsertime+=onetime;

		stOutArgs.eDecodeStatus = -1;
		stOutArgs.uiBytsConsumed = 0;

		// if return value if IH265D_NEED_MORE_BITS, read more bits from files
		while(stOutArgs.eDecodeStatus != IH265D_NEED_MORE_BITS)
		{
			// decode end
			if(stOutArgs.eDecodeStatus == IH265D_NO_PICTURE)
			{
				pdec->bStreamEnd = 1;
				break;
			}
			// output decoded pictures
			if (stOutArgs.eDecodeStatus == IH265D_GETDISPLAY)
			{
				// write output YUV to files
				/*if (pdec->fpOutFile != NULL)
				{
					UINT32 i;
					for (i=0;i<stOutArgs.uiDecHeight;i++)
					{
						fwrite(stOutArgs.pucOutYUV[0]+i*stOutArgs.uiYStride, 1, stOutArgs.uiDecWidth, pdec->fpOutFile);
					}
					for (i=0;i<((stOutArgs.uiDecHeight)>>1);i++)
					{
						fwrite(stOutArgs.pucOutYUV[1]+i*stOutArgs.uiUVStride, 1, stOutArgs.uiDecWidth>>1, pdec->fpOutFile);
					}
					for (i=0;i<((stOutArgs.uiDecHeight)>>1);i++)
					{
						fwrite(stOutArgs.pucOutYUV[2]+i*stOutArgs.uiUVStride, 1, stOutArgs.uiDecWidth>>1, pdec->fpOutFile);
					}
				}*/

				GETTIME(&onestart);
				mblk_t *om = hw_get_as_yuvmsg(NULL, pdec,&stOutArgs);
				android_ogl_display_set_yuv(env, om, pdec->winid, pdec->frameid,
						stOutArgs.uiDecWidth, stOutArgs.uiDecHeight);
				freemsg(om);
				pdec->iFrameIdx++;
				framenum++;
				pdec->framenum++;
				GETTIME(&oneend);
				ELAPSEDTIME(onestart, oneend, onetime, frequency);
				__android_log_print(ANDROID_LOG_ERROR, "decodeFrame",  "Display Time:%5u;",onetime);
				pdec->displaytime+=onetime;
			}

			// decode bins
			{
				stInArgs.pStream += stOutArgs.uiBytsConsumed;
				stInArgs.uiStreamLen -= stOutArgs.uiBytsConsumed;


				pdec->iRet = IHW265D_DecodeFrame(pdec->hDecoder, &stInArgs, &stOutArgs);


				if ((pdec->iRet != IHW265D_OK) && (pdec->iRet != IHW265D_NEED_MORE_BITS))
				{
					fprintf(stderr, "ERROR: IHW265D_DecodeFrame failed!\n");

					if (0 == pdec->iFileLen)
					{
						pdec->bStreamEnd = 1;
					}
					break;
				}
			}
		}

		GETTIME(&sumend);
		ELAPSEDTIME(sumstart, sumend, sumtime, frequency);
		__android_log_print(ANDROID_LOG_ERROR, "decodeFrame",  "Frame number:%d;Every frame.Time:%5u;Fps:%5u\n",framenum,sumtime,1000000*framenum/sumtime);
		//	GETTIME(&sumstart);
		framenum=0;
		pdec->threetime+=sumtime;
		if(pdec->framenum==3000)
		{
			__android_log_print(ANDROID_LOG_ERROR, "decodeFrame",  "Every 3000 frame.Time:%5u;Parser time:%u;Display time:%u;Fps:%5u\n",pdec->threetime,pdec->parsertime,pdec->displaytime,1000000*pdec->framenum/pdec->threetime);
			pdec->threetime=0;
			pdec->framenum=0;
		}
	}




	printf("Hello world!\n");
	/* count decoding time: end */
	pdec->EndTime = GetTime_ms();
	pdec->time = (INT32)(pdec->EndTime-pdec->StartTime);
	__android_log_print(ANDROID_LOG_INFO, "decodeFrame",  "\n uiDecWidth = %d, uiDecHeight = %d, time= %d ms\n", stOutArgs.uiDecWidth, stOutArgs.uiDecHeight, pdec->time);
	__android_log_print(ANDROID_LOG_INFO, "decodeFrame",  "%d frames\n",pdec->iFrameIdx);
	__android_log_print(ANDROID_LOG_INFO, "decodeFrame",  "fps: %d\n", pdec->iFrameIdx*1000/(pdec->time+1));

	printf("\n uiDecWidth = %d, uiDecHeight = %d, time= %d ms\n", stOutArgs.uiDecWidth, stOutArgs.uiDecHeight, pdec->time);
	printf("%d frames\n",pdec->iFrameIdx);
	printf("fps: %d\n", pdec->iFrameIdx*1000/(pdec->time+1));
	return 0;
}
void HwH265DecodeUnInit(JNIEnv* env, int phandle)
{
	HwDecState *pdec=(HwDecState *)phandle;
	if (pdec->fpInFile != 0)		fclose(pdec->fpInFile);
	if (pdec->fpOutFile != 0)     fclose(pdec->fpOutFile);
	if (pdec->hDecoder != NULL)	IHW265D_Delete(pdec->hDecoder);

	android_ogl_display_deinit(env, pdec->winid, pdec->frameid);

	if (pdec->pInputStream != NULL)
	{
		free(pdec->pInputStream);
		pdec->pInputStream = NULL;
	}

	free(pdec);

}
