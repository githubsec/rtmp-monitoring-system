/*
 * rtmprecive.cpp
 *
 *  Created on: 2017年2月22日
 *      Author: c
 */

#include <stdio.h>
#include<string.h>
#include<stdlib.h>
#include "rtmp_sys.h"
#include "log.h"
#include "sdl.h"

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






FILE *fpvideo=fopen("video.h264","wb");

int main(int argc, char* argv[])
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

	FILE *fp=fopen("receive.flv","wb");
	if (!fp){
		RTMP_LogPrintf("Open File Error.\n");
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
	if(!RTMP_SetupURL(rtmp,"rtmp://live.hkstv.hk.lxdns.com/live/hks")) //rtmp://live.hkstv.hk.lxdns.com/live/hks
	{
		RTMP_Log(RTMP_LOGERROR,"SetupURL Err\n");
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
		//fwrite(buf,1,nRead,fp);

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

