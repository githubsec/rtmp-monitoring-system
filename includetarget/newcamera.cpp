/*
 * newcamera.cpp
 *
 *  Created on: 2016��8��18��
 *      Author: Administrator
 */
#include <iostream>
#include "include/control.h"
using namespace std;

FILE *fp_send1;
void *controlThread(void* pControlData){
	ControlContext *ct_ctx=(ControlContext *)pControlData;

	char ich;
	while (1)
	{
		ich = getchar();
		switch (ich)
		{
		case 'q':
			printf("Exiting\n");
			close(&ct_ctx->cam);
			exit(0);
			break;
		default:
			printf("Unknown command \'%c\', ignoring\n", ich);
	}
	}
	return (void*) 0;
}

int main()
{
	int ret;
	ControlContext control;
	fp_send1 = fopen("cuc_ieschool.h264", "rb");
	pthread_create(&(control.threads[0]),NULL,controlThread,&control);
	ret=init(&control);
	if(ret)
	{
		printf("Open error!");
		return -1;
	}


	return 0;
}


