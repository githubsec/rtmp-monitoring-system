/*
 * sdl.cpp
 *
 *  Created on: 2017年3月10日
 *      Author: c
 */

#include <stdio.h>
#include<sdl.h>
//Linux...



//Refresh Event
#define SFM_REFRESH_EVENT  (SDL_USEREVENT + 1)

#define SFM_BREAK_EVENT  (SDL_USEREVENT + 2)

int thread_exit=0;
int thread_pause=0;

FILE *fp_in;
FILE *fp_out=fopen("test.yuv","w+");

AVPacket avpkt;
AVFrame *orig=NULL;
AVFrame *pFrameYUV=NULL;

int sfp_refresh_thread(void *opaque){
	thread_exit=0;
	thread_pause=0;

	while (!thread_exit) {
		if(!thread_pause){
			SDL_Event event;
			event.type = SFM_REFRESH_EVENT;
			SDL_PushEvent(&event);
		}
		SDL_Delay(10);
	}
	thread_exit=0;
	thread_pause=0;
	//Break
	SDL_Event event;
	event.type = SFM_BREAK_EVENT;
	SDL_PushEvent(&event);

	return 0;
}


int sdl_init(int phandle)
{
	DecState *pdec=(DecState *)phandle;
	unsigned char *out_buffer;
	out_buffer=(unsigned char *)av_malloc(av_image_get_buffer_size(AV_PIX_FMT_YUV420P,  pdec->av_context->width, pdec->av_context->height,1));
	av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize,out_buffer,
		AV_PIX_FMT_YUV420P,pdec->av_context->width, pdec->av_context->height,1);


	pdec->img_convert_ctx = sws_getContext(pdec->av_context->width, pdec->av_context->height,
			pdec->av_context->pix_fmt,
			pdec->av_context->width, pdec->av_context->height, AV_PIX_FMT_YUV420P, SWS_BICUBIC, NULL, NULL, NULL);


	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
		printf( "Could not initialize SDL - %s\n", SDL_GetError());
		return -1;
	}
	//SDL 2.0 Support for multiple windows
	pdec->screen_w = 640;
	pdec->screen_h = 480;
	pdec->screen = SDL_CreateWindow("Simplest ffmpeg player's Window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			pdec->screen_w, pdec->screen_h,SDL_WINDOW_OPENGL);

	if(!pdec->screen) {
		printf("SDL: could not create window - exiting:%s\n",SDL_GetError());
		return -1;
	}
	pdec->sdlRenderer = SDL_CreateRenderer(pdec->screen, -1, 0);
	//IYUV: Y + U + V  (3 planes)
	//YV12: Y + V + U  (3 planes)
	pdec->sdlTexture = SDL_CreateTexture(pdec->sdlRenderer, SDL_PIXELFORMAT_IYUV, SDL_TEXTUREACCESS_STREAMING,pdec->av_context->width,pdec->av_context->height);

	pdec->sdlRect.x=0;
	pdec->sdlRect.y=0;
	pdec->sdlRect.w=pdec->screen_w;
	pdec->sdlRect.h=pdec->screen_h;

	pdec->video_tid = SDL_CreateThread(sfp_refresh_thread,NULL,NULL);
	return 0;
}

int decode_init()
{
	DecState *pdec=(DecState*)malloc(sizeof(DecState));
	av_register_all();
	avformat_network_init();
	avcodec_register_all();

	pdec->sdl_init=0;
	pdec->av_context= NULL;
	pdec->pCodecParserCtx=NULL;
	pdec->codec=AV_CODEC_ID_H264;
	pdec->output_pix_fmt=AV_PIX_FMT_YUV420P;
	pdec->av_codec=avcodec_find_decoder(pdec->codec);
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

	//pFrame=av_frame_alloc();
	pFrameYUV=av_frame_alloc();


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

	if(pdec->sdl_init<=10){
		pdec->sdl_init++;
			if(avcodec_decode_video2(pdec->av_context,orig,&got_picture,&avpkt) != len)
			{
				if (len <= 4 ||
					(((addr[4]&0x1f) != 7) && ((addr[4]&0x1f) != 8)
					&&((addr[3]&0x1f) != 7) && ((addr[3]&0x1f) != 8)))
						printf("decodeFrame", "decode error ,addr[3,4](%x,%x)",  addr[3], addr[4]);
				return 0;
			}

			if (pdec->sdl_init==10)
			{
				sdl_init(phandle);
				//Y, U, V
			//	int i;
			//	for(i=0;i<orig->height;i++)
				//{
				//(orig->data[0]+orig->linesize[0]*i,1,orig->width,fp_out);
				//}
				//for(i=0;i<orig->height/2;i++)
				//{
				//	fwrite(orig->data[1]+orig->linesize[1]*i,1,orig->width/2,fp_out);
				//}
			//	for(i=0;i<orig->height/2;i++)
				//{
				//	fwrite(orig->data[2]+orig->linesize[2]*i,1,orig->width/2,fp_out);
			//	}
			}
			av_free_packet(&avpkt);
	}
	else{
		SDL_WaitEvent(&pdec->event);
		if(pdec->event.type==SFM_REFRESH_EVENT){
			if(avcodec_decode_video2(pdec->av_context,orig,&got_picture,&avpkt) != len)
			{
				if (len <= 4 ||
					(((addr[4]&0x1f) != 7) && ((addr[4]&0x1f) != 8)
					&&((addr[3]&0x1f) != 7) && ((addr[3]&0x1f) != 8)))
						printf("decodeFrame", "decode error ,addr[3,4](%x,%x)",  addr[3], addr[4]);
				return 0;
			}

			if (got_picture)
			{
				sws_scale(pdec->img_convert_ctx, (const unsigned char* const*)orig->data, orig->linesize, 0, pdec->av_context->height, pFrameYUV->data, pFrameYUV->linesize);
				//SDL---------------------------
				SDL_UpdateTexture( pdec->sdlTexture, NULL, pFrameYUV->data[0], pFrameYUV->linesize[0] );
				SDL_RenderClear( pdec->sdlRenderer );
				//SDL_RenderCopy( sdlRenderer, sdlTexture, &sdlRect, &sdlRect );
				SDL_RenderCopy(pdec-> sdlRenderer, pdec->sdlTexture, NULL, NULL);
				SDL_RenderPresent( pdec->sdlRenderer );
				//SDL End-----------------------
				//Y, U, V
			//	int i;
			//	for(i=0;i<orig->height;i++)
				//{
				//(orig->data[0]+orig->linesize[0]*i,1,orig->width,fp_out);
				//}
				//for(i=0;i<orig->height/2;i++)
				//{
				//	fwrite(orig->data[1]+orig->linesize[1]*i,1,orig->width/2,fp_out);
				//}
			//	for(i=0;i<orig->height/2;i++)
				//{
				//	fwrite(orig->data[2]+orig->linesize[2]*i,1,orig->width/2,fp_out);
			//	}
			}
			av_free_packet(&avpkt);

		}else if(pdec->event.type==SDL_KEYDOWN){
			//Pause
			if(pdec->event.key.keysym.sym==SDLK_SPACE)
				thread_pause=!thread_pause;
		}else if(pdec->event.type==SDL_QUIT){
			thread_exit=1;
		}else if(pdec->event.type==SFM_BREAK_EVENT){
			return 0;
		}
	}

	return 1;
}

void DecodeUnInit(int phandle)
{
	DecState *pdec=(DecState *)phandle;

	sws_freeContext(pdec->img_convert_ctx);

	SDL_Quit();
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

	free(pdec);
	return;
}

//reverse_bytes - turn a BigEndian byte array into a LittleEndian integer
uint reverse_bytes(byte *p, char c) {
    int r = 0;
    int i;
    for (i=0; i<c; i++)
        r |= ( *(p+i) << (((c-1)*8)-8*i));
    return r;
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




