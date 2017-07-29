/*
 * rtmpserver.cpp
 *
 *  Created on: 2017年3月1日
 *      Author: c
 */
#include<iostream>
#include"server.h"
using namespace std;

static void initMutex()
{
	pthread_mutex_init(&Nalulist.mutex, NULL);
}

int main(int argc, char **argv)
{
	int nStatus = RD_SUCCESS;
	int i;


	memset(&Nalulist, 0, sizeof(decoder_handle_t));
	initMutex();
	for (int i = 0; i < FRAME_MAX_CNT; i++)
	{
		Nalulist.frame[i].abFrameData = (char*)malloc(FRAME_MAX_LEN);
		Nalulist.frame[i].nFrameDataMaxSize = -1;
		Nalulist.frame[i].time=0;
		Nalulist.frame[i].sleeptime=0;
	}
	Nalulist.r_frame_idx = 0;
	Nalulist.w_frame_idx = 0;
	Nalulist.frame_cnt_unuse = FRAME_MAX_CNT;

	memset(&metaDataforclients,0,sizeof(RTMPMetadata));

	char DEFAULT_HTTP_STREAMING_DEVICE[] = "192.168.1.106";	// 0.0.0.0 is any device
	char *rtmpStreamingDevice = DEFAULT_HTTP_STREAMING_DEVICE;	// streaming device, default 0.0.0.0
	int nRtmpStreamingPort = 1935;	// port
	char *cert = NULL, *key = NULL;

	RTMP_debuglevel = RTMP_LOGINFO;

	for (i = 1; i < argc; i++)
    {
      if (!strcmp(argv[i], "-z"))
        RTMP_debuglevel = RTMP_LOGALL;
      else if (!strcmp(argv[i], "-c") && i + 1 < argc)
        cert = argv[++i];
      else if (!strcmp(argv[i], "-k") && i + 1 < argc)
        key = argv[++i];
    }

	initCamera(&control.cam);
	encode_init(&control.cam);

	if (cert && key)
		sslCtx = RTMP_TLS_AllocServerContext(cert, key);


	memset(&defaultRTMPRequest, 0, sizeof(RTMP_REQUEST));
	defaultRTMPRequest.rtmpport = -1;
	defaultRTMPRequest.protocol = RTMP_PROTOCOL_UNDEFINED;
	defaultRTMPRequest.bLiveStream = FALSE;	// is it a live stream? then we can't seek/resume
	defaultRTMPRequest.timeout = 300;	// timeout connection afte 300 seconds
	defaultRTMPRequest.bufferTime = 20 * 1000;

	signal(SIGINT, sigIntHandler);
	#ifndef WIN32
	signal(SIGPIPE, SIG_IGN);
	#endif

	InitSockets();

	// start text UI
	ThreadCreate(controlServerThread, 0);

	// start http streaming
	if ((rtmpServer = startServer(rtmpStreamingDevice, nRtmpStreamingPort)) == 0)
	{
		RTMP_Log(RTMP_LOGERROR, "Failed to start RTMP server, exiting!");
		return RD_FAILED;
	}

	RTMP_LogPrintf("RTMP Server for Test %s \n", RTMPDUMP_VERSION);
	RTMP_LogPrintf("(c) 2017 Xulei; license: GPL\n\n");
	RTMP_LogPrintf("rtmp://%s:%d\n", rtmpStreamingDevice,nRtmpStreamingPort);

	while (rtmpServer->state != STREAMING_STOPPED)
    {
		sleep(1);
    }
	RTMP_Log(RTMP_LOGDEBUG, "Done, exiting...");

	if (sslCtx)
		RTMP_TLS_FreeServerContext(sslCtx);

	CleanupSockets();
	pthread_mutex_lock(&(Nalulist.mutex));
	for (int i = 0; i < FRAME_MAX_CNT; i++)
	{
		free(Nalulist.frame[i].abFrameData);
		Nalulist.frame[i].abFrameData = NULL;
	}
	pthread_mutex_unlock(&(Nalulist.mutex));

	return nStatus;
}








