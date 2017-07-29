/*
 * camera.h
 *
 *  Created on: 2016骞�6鏈�16鏃�
 *      Author: c
 */

#ifndef CAMERA_H_
#define CAMERA_H_

#define __STDC_CONSTANT_MACROS
#include <iostream>
#include <linux/videodev2.h>
#include<fcntl.h>
#include<unistd.h>
#include <cstdlib>
#include<cstring>
#include<cstdio>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <errno.h>
#include<pthread.h>
#include <sys/time.h>
#include<opencv2/opencv.hpp>
#include "list.h"
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include<libavutil/time.h>
#ifdef __cplusplus
};
#endif

using namespace std;
using namespace cv;


#define MAX_BUFFER_ELEMENTS 5
struct Buffer{
	void *start;
	unsigned int length;
};

typedef struct mBuffer_queue
{
	struct list_head    inBufferList;
	struct list_head    inReadyBufferList;
	struct list_head    mBufList;   //DynamicBuffer, sizeof(mBuffer)*MAX_MESSAGE_ELEMENTS
	int                 buffer_count;
	pthread_mutex_t     mutex;
	pthread_cond_t      mCondBufferQueueChanged;
	int                 mWaitBufferFlag;
}mBuffer_queue;

struct ffmpegencode
{
	AVCodec *pCodec;
	AVCodecContext *pCodecCtx;
	FILE *hevc_out;
	AVFrame *pFrame;
	AVPacket pkt;
	int framecnt;

	AVOutputFormat *ofmt;
	AVFormatContext  *ofmt_ctx ;
	AVStream *out_stream;
	AVPacket enc_pkt;
	int64_t start_time;
};

typedef struct Camera{
	struct ffmpegencode encode;
	char *device_name;
	char *yuv_filename;
	char *hevc_filename;
	int bmp_number;
	int height;
	int width;
	int fd;
	int display_depth;
	int frame_number;
	FILE *yuv_fp;
	Buffer *buffers;
	mBuffer_queue buffer_list;
	unsigned char *y420p_buffer;

	//for opencv
	int num_bg;
	int num_goal;
	Mat foreground;
	int flag_goal;
	int num_run;
	int num_cont;

	vector<Rect> rts;
}Camera;

typedef struct timeval TIMER;
#define GETTIME(timer) gettimeofday(timer,NULL);
#define ELAPSEDTIME(s_start_timer,s_end_timer, s_elapsed_time, frequency) \
                   s_elapsed_time = ((s_end_timer.tv_sec - s_start_timer.tv_sec) * 1000000) + (s_end_timer.tv_usec - s_start_timer.tv_usec);

void errno_exit(const char *s);
void open_camera(Camera*);
void close_camera(Camera*);
int read_and_encode_frame(Camera*);
void start_capturing(Camera*);
void stop_capturing(Camera*);
void init_camera(Camera*);
void uninit_camera(Camera*);
void encode_frame(Camera*,unsigned int,int);
//void encode_frame(Camera*,unsigned int,unsigned int);
void init_mmap(Camera*);
void init_file(Camera*);
void close_file(Camera*);
void save_frame(Camera*,unsigned int);
int convert_yuyv_to_yuv420p(unsigned char *, unsigned char *, unsigned int, unsigned int);
int encode_init(Camera*);
int encode(Camera*);
void encode_uninit(Camera*);



#endif /* CAMERA_H_ */
