/*
 * send264.h
 *
 *  Created on: 2017年3月1日
 *      Author: c
 */

#ifndef INCLUDE_SEND264_H_
#define INCLUDE_SEND264_H_
#include <stdio.h>
#include <stdlib.h>
#include "rtmp.h"
#include "rtmp_sys.h"
#include "amf.h"
#include "camera.h"
#include "control.h"

typedef struct _RTMPMetadata
{
	// video, must be h264 type
	unsigned int    nWidth;
	unsigned int    nHeight;
	unsigned int    nFrameRate;
	unsigned int    nSpsLen;
	unsigned char   *Sps;
	unsigned int    nPpsLen;
	unsigned char   *Pps;
} RTMPMetadata,*LPRTMPMetadata;


#define FRAME_MAX_CNT 30
#define FRAME_MAX_LEN 300*1024

typedef struct __frame_data {
	char* abFrameData;
	int nFrameDataMaxSize;
	int time;
	int sleeptime;
}frame_data_t;

typedef struct __decoder_handle {
	frame_data_t frame[FRAME_MAX_CNT];
	int r_frame_idx;
	int w_frame_idx;
	int frame_cnt_unuse;
	pthread_mutex_t mutex;
}decoder_handle_t;

int RTMP264_Send(Camera *,void*,decoder_handle_t*,RTMPMetadata*);

void RTMP264_Close();
void putdata(decoder_handle_t*,void*,RTMPMetadata*);




#endif /* INCLUDE_SEND264_H_ */
