/*
 * control.h
 *
 *  Created on: 2016��8��18��
 *      Author: Administrator
 */

#ifndef CONTROL_H_
#define CONTROL_H_
#include <stdio.h>
#include <stdlib.h>
#include "message.h"
#include <pthread.h>
#include <string.h>
#include "camera.h"
#include "send264.h"

enum options{
	ENCODE,
	PLAY,
	SAVE,
	SAVEBMP,
	CLOSE
};

enum error{
	CT_ERROR=-1
};

typedef struct Control
{
	pthread_mutex_t inframe_mutex;
	pthread_mutex_t encoder_mutex;
	pthread_mutex_t display_mutex;
	pthread_mutex_t msg_mutex;
	pthread_mutex_t msg_sync_mutex;
	int msg_id_index;
	int encodeflag;
	int playflag;
	int saveflag;
	pthread_t threads[5];
	Camera cam;
	int client_num;
	message_queue_t m_queue;
}ControlContext;

int init(void*);
int close(void*);

int initCamera(Camera*);
int closeCamera(Camera*);


#endif /* CONTROL_H_ */
