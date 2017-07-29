
#ifndef COMMON_H
#define COMMON_H
#ifdef __cplusplus
#define __STDC_CONSTANT_MACROS 1
#endif
#include <string.h>
#include <jni.h>
#include <android/log.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "config.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"

typedef unsigned char bool_t;
#undef TRUE
#undef FALSE
#define TRUE 1
#define FALSE 0
#define H264_START_CODE         0x00000001

#define  LOG_TAG    "H264DEC"
#define  LOGV(...)  __android_log_print(ANDROID_LOG_INFO,LOG_TAG,__VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR,LOG_TAG,__VA_ARGS__)

typedef struct MSVideoSize{
	int width,height;
} MSVideoSize;

typedef struct _MSPicture{
	int w,h;
	uint8_t *planes[4]; /*we usually use 3 planes, 4th is for compatibility */
	int strides[4];	/*with ffmpeg's swscale.h */
}MSPicture;

typedef struct _MSPicture YuvBuf; /*for backward compatibility*/

#endif /* COMMON_H */
