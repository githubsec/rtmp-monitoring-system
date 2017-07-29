/*
 * server.h
 *
 *  Created on: 2017年4月9日
 *      Author: c
 */

#ifndef SERVER_H_
#define SERVER_H_
#include <cstdlib>
#include <cstring>
#include <math.h>
#include <limits.h>

#include <signal.h>
#include <getopt.h>
#include <sys/time.h>
#include <assert.h>
#include "rtmp_sys.h"
#include "log.h"
#include "thread.h"
#include "control.h"
#include "send264.h"


#ifdef linux
#include <linux/netfilter_ipv4.h>
#endif

#ifndef WIN32
#include <sys/types.h>
#include <sys/wait.h>
#endif

#define RD_SUCCESS		0
#define RD_FAILED		1
#define RD_INCOMPLETE		2
#define PACKET_SIZE 1024*1024
#define InitSockets()
#define	CleanupSockets()
#define DUPTIME	5000	/* interval we disallow duplicate requests, in msec */

enum
{
  STREAMING_ACCEPTING,
  STREAMING_IN_PROGRESS,
  STREAMING_STOPPING,
  STREAMING_STOPPED
};

typedef struct
{
  int socket;
  int state;
  int streamID;
  int arglen;
  int argc;
  uint32_t filetime;	/* time of last download we started */
  AVal filename;	/* name of last download */
  char *connect;
} STREAMING_SERVER;

typedef struct
{
	STREAMING_SERVER * server;
	int sockfd;
} CLIENT_SERVER;

typedef struct
{
  char *hostname;
  int rtmpport;
  int protocol;
  int bLiveStream;		// is it a live stream? then we can't seek/resume
  long int timeout;		// timeout connection afte 300 seconds
  uint32_t bufferTime;

  char *rtmpurl;
  AVal playpath;
  AVal swfUrl;
  AVal tcUrl;
  AVal pageUrl;
  AVal app;
  AVal auth;
  AVal swfHash;
  AVal flashVer;
  AVal subscribepath;
  uint32_t swfSize;

  uint32_t dStartOffset;
  uint32_t dStopOffset;
  uint32_t nTimeStamp;
} RTMP_REQUEST;

extern RTMPMetadata metaDataforclients;
extern decoder_handle_t Nalulist;
extern RTMP_REQUEST defaultRTMPRequest;
extern STREAMING_SERVER *rtmpServer;	// server structure pointer
extern void *sslCtx;
extern ControlContext control;

static const AVal av_dquote = AVC("\"");
static const AVal av_escdquote = AVC("\\\"");
#define STR2AVAL(av,str)	av.av_val = str; av.av_len = strlen(av.av_val)
#define SAVC(x) static const AVal av_##x = AVC(#x)
#define HTON16(x)  ((x>>8&0xff)|(x<<8&0xff00))
#define HTON24(x)  ((x>>16&0xff)|(x<<16&0xff0000)|x&0xff00)
#define HTON32(x)  ((x>>24&0xff)|(x>>8&0xff00)|\
(x<<8&0xff0000)|(x<<24&0xff000000))

SAVC(app);
SAVC(connect);
SAVC(flashVer);
SAVC(swfUrl);
SAVC(pageUrl);
SAVC(tcUrl);
SAVC(fpad);
SAVC(capabilities);
SAVC(audioCodecs);
SAVC(videoCodecs);
SAVC(videoFunction);
SAVC(objectEncoding);
SAVC(_result);
SAVC(createStream);
SAVC(getStreamLength);
SAVC(play);
SAVC(fmsVer);
SAVC(mode);
SAVC(level);
SAVC(code);
SAVC(description);
SAVC(secureToken);
SAVC(onStatus);
SAVC(status);

static const AVal av_NetStream_Play_Start = AVC("NetStream.Play.Start");
static const AVal av_Started_playing = AVC("Started playing");
static const AVal av_NetStream_Play_Stop = AVC("NetStream.Play.Stop");
static const AVal av_Stopped_playing = AVC("Stopped playing");
SAVC(details);
SAVC(clientid);

static const AVal av_NetStream_Authenticate_UsherToken = AVC("NetStream.Authenticate.UsherToken");
static const AVal av_NetStream_Play_Reset = AVC("NetStream.Play.Reset");


STREAMING_SERVER *startServer(const char *address, int port);
void sigIntHandler(int sig);
TFTYPE controlServerThread(void *unused);




#endif /* SERVER_H_ */
