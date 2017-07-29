/*
 * control.c
 *
 *  Created on: 2016��8��18��
 *      Author: Administrator
 */

#include "include/control.h"
#include <unistd.h>

void *camEncode(void *pcam)
{
	ControlContext *ct_ctx=(ControlContext *)pcam;
	//RTMP264_Send(&ct_ctx->cam);
	pthread_exit((void *) 0);
}

int init(void *pcam)
{
	//RTMP264_Connect("rtmp://0.0.0.0:1935/myapp/test1");
	ControlContext *ct_ctx=(ControlContext *)pcam;
	initCamera(&ct_ctx->cam);
	encode_init(&ct_ctx->cam);
	pthread_create(&(ct_ctx->threads[0]),NULL,camEncode,ct_ctx);
	pthread_join(ct_ctx->threads[0], NULL);
	return 0;
}

int close(void *pcam)
{
	RTMP264_Close();
	ControlContext *ct_ctx=(ControlContext *)pcam;
	encode_uninit(&ct_ctx->cam);
	closeCamera(&ct_ctx->cam);
	return 0;
}

int initCamera(Camera* cam)
{
	cam->device_name="/dev/video0";
	cam->yuv_filename="test.yuv";
	cam->hevc_filename="test.h264";
	cam->height=480;
	cam->width=640;
	cam->fd=0;
	cam->display_depth=5;
	cam->bmp_number=0;

	//for opencv;
	cam->num_bg = 0;
	cam->num_goal = 0;
	cam->flag_goal = 0;
	cam->num_run = 0;
	cam->num_cont = 0;

	open_camera(cam);
	init_camera(cam);
	//init_file(cam);
	start_capturing(cam);
	return 0;
}

int closeCamera(Camera* cam)
{
	stop_capturing(cam);
	//close_file(cam);
	uninit_camera(cam);
	close_camera(cam);
	return 0;
}


