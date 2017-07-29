/*
 * sdl.h
 *
 *  Created on: 2017年3月10日
 *      Author: c
 */

#ifndef SDL_H_
#define SDL_H_
#ifdef __cplusplus
extern "C"
{
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
#include <SDL2/SDL.h>
#ifdef __cplusplus
};
#endif




#define TAG_TYPE_SCRIPT 18
#define TAG_TYPE_AUDIO  8
#define TAG_TYPE_VIDEO  9


typedef unsigned char byte;
typedef unsigned int uint;

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





typedef struct DecState{
	AVCodec *av_codec;
	AVCodecContext *av_context;
	AVCodecParserContext *pCodecParserCtx;

	enum AVPixelFormat output_pix_fmt;
	enum AVCodecID codec;
	struct SwsContext *img_convert_ctx;

	//sdl
	int screen_w,screen_h;
	SDL_Window *screen;
	SDL_Renderer* sdlRenderer;
	SDL_Texture* sdlTexture;
	SDL_Rect sdlRect;
	SDL_Thread *video_tid;
	SDL_Event event;
	int sdl_init;
}DecState;

int decode_init();
int decode(int , char* ,int );
void DecodeUnInit(int );
int flv_video(char *,int ,int );


#endif /* SDL_H_ */
