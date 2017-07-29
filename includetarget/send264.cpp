/*
 * send264.cpp
 *
 *  Created on: 2017年3月1日
 *      Author: c
 */

#include "send264.h"
#include "sps_decode.h"
#include <sys/time.h>


#define RTMP_HEAD_SIZE   (sizeof(RTMPPacket)+RTMP_MAX_HEADER_SIZE)
#define BUFFER_SIZE 72768
#define GOT_A_NAL_CROSS_BUFFER BUFFER_SIZE+1
#define GOT_A_NAL_INCLUDE_A_BUFFER BUFFER_SIZE+2
#define NO_MORE_BUFFER_TO_READ BUFFER_SIZE+3

typedef struct timeval TIMER;
#define GETTIME(timer) gettimeofday(timer,NULL);
#define ELAPSEDTIME(s_start_timer,s_end_timer, s_elapsed_time, frequency) \
                   s_elapsed_time = ((s_end_timer.tv_sec - s_start_timer.tv_sec) * 1000000) + (s_end_timer.tv_usec - s_start_timer.tv_usec);


typedef struct _NaluUnit
{
	int type;
    int size;
	unsigned char *data;
}NaluUnit;

enum
{
	 VIDEO_CODECID_H264 = 7,
};

char * put_byte( char *output, uint8_t nVal )
{
	output[0] = nVal;
	return output+1;
}

char * put_be16(char *output, uint16_t nVal )
{
	output[1] = nVal & 0xff;
	output[0] = nVal >> 8;
	return output+2;
}

char * put_be24(char *output,uint32_t nVal )
{
	output[2] = nVal & 0xff;
	output[1] = nVal >> 8;
	output[0] = nVal >> 16;
	return output+3;
}
char * put_be32(char *output, uint32_t nVal )
{
	output[3] = nVal & 0xff;
	output[2] = nVal >> 8;
	output[1] = nVal >> 16;
	output[0] = nVal >> 24;
	return output+4;
}
char *  put_be64( char *output, uint64_t nVal )
{
	output=put_be32( output, nVal >> 32 );
	output=put_be32( output, nVal );
	return output;
}

char * put_amf_string( char *c, const char *str )
{
	uint16_t len = strlen( str );
	c=put_be16( c, len );
	memcpy(c,str,len);
	return c+len;
}
char * put_amf_double( char *c, double d )
{
	*c++ = AMF_NUMBER;  /* type: Number */
	{
		unsigned char *ci, *co;
		ci = (unsigned char *)&d;
		co = (unsigned char *)c;
		co[0] = ci[7];
		co[1] = ci[6];
		co[2] = ci[5];
		co[3] = ci[4];
		co[4] = ci[3];
		co[5] = ci[2];
		co[6] = ci[1];
		co[7] = ci[0];
	}
	return c+8;
}


unsigned int  m_nFileBufSize;
unsigned int  nalhead_pos;
RTMPMetadata metaData;
unsigned char *m_pFileBuf;
unsigned char *m_pFileBuf_tmp;
unsigned char* m_pFileBuf_tmp_old;	//used for realloc

void RTMP264_Close()
{
	if (m_pFileBuf != NULL)
	{
		free(m_pFileBuf);
	}
	if (m_pFileBuf_tmp != NULL)
	{
		free(m_pFileBuf_tmp);
	}
}

int SendPacket(unsigned int nPacketType,unsigned char *data,unsigned int size,unsigned int nTimestamp,void*lparam)
{
	RTMP *m_pRtmp=(RTMP*)lparam;
	RTMPPacket* packet;
	/*������ڴ�ͳ�ʼ��,lenΪ���峤��*/
	packet = (RTMPPacket *)malloc(RTMP_HEAD_SIZE+size);
	memset(packet,0,RTMP_HEAD_SIZE);
	/*�����ڴ�*/
	packet->m_body = (char *)packet + RTMP_HEAD_SIZE;
	packet->m_nBodySize = size;
	memcpy(packet->m_body,data,size);
	packet->m_hasAbsTimestamp = 0;
	packet->m_packetType = nPacketType; /*�˴�Ϊ����������һ������Ƶ,һ������Ƶ*/
	packet->m_nInfoField2 = m_pRtmp->m_stream_id;
	packet->m_nChannel = 0x04;

	packet->m_headerType = RTMP_PACKET_SIZE_LARGE;
	if (RTMP_PACKET_TYPE_AUDIO ==nPacketType && size !=4)
	{
		packet->m_headerType = RTMP_PACKET_SIZE_MEDIUM;
	}
	packet->m_nTimeStamp = nTimestamp;
	/*����*/
	int nRet =0;
	if (RTMP_IsConnected(m_pRtmp))
	{
		nRet = RTMP_SendPacket(m_pRtmp,packet,TRUE); /*TRUEΪ�Ž��Ͷ���,FALSE�ǲ��Ž��Ͷ���,ֱ�ӷ���*/
	}
	/*�ͷ��ڴ�*/
	free(packet);
	return nRet;
}

/**
 * ������Ƶ��sps��pps��Ϣ
 *
 * @param pps �洢��Ƶ��pps��Ϣ
 * @param pps_len ��Ƶ��pps��Ϣ����
 * @param sps �洢��Ƶ��pps��Ϣ
 * @param sps_len ��Ƶ��sps��Ϣ����
 *
 * @�ɹ��򷵻� 1 , ʧ���򷵻�0
 */
int SendVideoSpsPps(unsigned char *pps,int pps_len,unsigned char * sps,int sps_len,int nTimeStamp,void*lparam)
{
	RTMP *m_pRtmp=(RTMP*)lparam;
	int time=0;
	RTMPPacket * packet=NULL;//rtmp��ṹ
	unsigned char * body=NULL;
	int i;
	packet = (RTMPPacket *)malloc(RTMP_HEAD_SIZE+1024);
	//RTMPPacket_Reset(packet);//����packet״̬
	memset(packet,0,RTMP_HEAD_SIZE+1024);
	packet->m_body = (char *)packet + RTMP_HEAD_SIZE;
	body = (unsigned char *)packet->m_body;
	i = 0;
	body[i++] = 0x17;
	body[i++] = 0x00;

	body[i++] = 0x00;
	body[i++] = 0x00;
	body[i++] = 0x00;

	/*AVCDecoderConfigurationRecord*/
	body[i++] = 0x01;
	body[i++] = sps[1];
	body[i++] = sps[2];
	body[i++] = sps[3];
	body[i++] = 0xff;

	/*sps*/
	body[i++]   = 0xe1;
	body[i++] = (sps_len >> 8) & 0xff;
	body[i++] = sps_len & 0xff;
	memcpy(&body[i],sps,sps_len);
	i +=  sps_len;

	/*pps*/
	body[i++]   = 0x01;
	body[i++] = (pps_len >> 8) & 0xff;
	body[i++] = (pps_len) & 0xff;
	memcpy(&body[i],pps,pps_len);
	i +=  pps_len;

	packet->m_packetType = RTMP_PACKET_TYPE_VIDEO;
	packet->m_nBodySize = i;
	packet->m_nChannel = 0x04;
	packet->m_nTimeStamp = nTimeStamp;
	packet->m_hasAbsTimestamp = 0;
	packet->m_headerType = RTMP_PACKET_SIZE_MEDIUM;
	packet->m_nInfoField2 = m_pRtmp->m_stream_id;

	/*���÷��ͽӿ�*/
	int nRet = RTMP_SendPacket(m_pRtmp,packet,TRUE);
	free(packet);    //�ͷ��ڴ�
	return nRet;
}


int SendPpsSps(unsigned int nTimeStamp,void*lparam)
{
	unsigned char *pps = (unsigned char*)malloc(metaData.nPpsLen+9);
	memset(pps,0,metaData.nPpsLen+9);

	unsigned char *sps = (unsigned char*)malloc(metaData.nSpsLen+9);
	memset(sps,0,metaData.nSpsLen+9);

	int bRet;
	int i = 0;
	sps[i++] = 0x17;// 1:Iframe  7:AVC
	sps[i++] = 0x01;// AVC NALU
	sps[i++] = 0x00;
	sps[i++] = 0x00;
	sps[i++] = 0x00;
	// NALU size
	sps[i++] = metaData.nSpsLen>>24 &0xff;
	sps[i++] = metaData.nSpsLen>>16 &0xff;
	sps[i++] = metaData.nSpsLen>>8 &0xff;
	sps[i++] = metaData.nSpsLen&0xff;
	// NALU data
	memcpy(&sps[i],metaData.Sps,metaData.nSpsLen);

	bRet = SendPacket(RTMP_PACKET_TYPE_VIDEO,sps,i+metaData.nSpsLen,nTimeStamp,lparam);
	i = 0;
	pps[i++] = 0x17;// 1:Iframe  7:AVC
	pps[i++] = 0x01;// AVC NALU
	pps[i++] = 0x00;
	pps[i++] = 0x00;
	pps[i++] = 0x00;
	// NALU size
	pps[i++] = metaData.nPpsLen>>24 &0xff;
	pps[i++] = metaData.nPpsLen>>16 &0xff;
	pps[i++] = metaData.nPpsLen>>8 &0xff;
	pps[i++] = metaData.nPpsLen&0xff;
	// NALU data
	memcpy(&pps[i],metaData.Pps,metaData.nPpsLen);
	bRet = SendPacket(RTMP_PACKET_TYPE_VIDEO,pps,i+metaData.nPpsLen,nTimeStamp,lparam);




	free(pps);
	free(sps);
	return bRet;
}

/**
 * ����H264���֡
 *
 * @param data �洢���֡����
 * @param size ���֡�Ĵ�С
 * @param bIsKeyFrame ��¼��֡�Ƿ�Ϊ�ؼ�֡
 * @param nTimeStamp ��ǰ֡��ʱ���
 *
 * @�ɹ��򷵻� 1 , ʧ���򷵻�0
 */
int SendH264Packet(unsigned char *data,unsigned int size,int bIsKeyFrame,unsigned int nTimeStamp,void*lparam)
{
	if(data == NULL && size<11)
	{
		return false;
	}

	unsigned char *body = (unsigned char*)malloc(size+9);
	memset(body,0,size+9);

	int i = 0;
	if(bIsKeyFrame)
	{
		body[i++] = 0x17;// 1:Iframe  7:AVC
		body[i++] = 0x01;// AVC NALU
		body[i++] = 0x00;
		body[i++] = 0x00;
		body[i++] = 0x00;


		// NALU size
		body[i++] = size>>24 &0xff;
		body[i++] = size>>16 &0xff;
		body[i++] = size>>8 &0xff;
		body[i++] = size&0xff;
		// NALU data
		memcpy(&body[i],data,size);

		SendPpsSps(nTimeStamp,lparam);
		//SendVideoSpsPps(metaData.Pps,metaData.nPpsLen,metaData.Sps,metaData.nSpsLen,nTimeStamp,lparam);
	}
	else
	{
		body[i++] = 0x27;// 2:Pframe  7:AVC
		body[i++] = 0x01;// AVC NALU
		body[i++] = 0x00;
		body[i++] = 0x00;
		body[i++] = 0x00;


		// NALU size
		body[i++] = size>>24 &0xff;
		body[i++] = size>>16 &0xff;
		body[i++] = size>>8 &0xff;
		body[i++] = size&0xff;
		// NALU data
		memcpy(&body[i],data,size);
	}


	int bRet = SendPacket(RTMP_PACKET_TYPE_VIDEO,body,i+size,nTimeStamp,lparam);

	free(body);

	return bRet;
}

int ReadOneNaluFromBuf(NaluUnit &nalu,Camera *cam)
{
	if(nalhead_pos==cam->encode.pkt.size)
	{
		av_free_packet(&cam->encode.pkt);
		encode(cam);
		nalhead_pos=0;
	}
	int naltail_pos=nalhead_pos;
	while(nalhead_pos<cam->encode.pkt.size)
	{
		//search for nal header
		if(cam->encode.pkt.data[nalhead_pos++] == 0x00 &&
				cam->encode.pkt.data[nalhead_pos++] == 0x00)
		{
			if(cam->encode.pkt.data[nalhead_pos++] == 0x01)
				goto gotnal_head;
			else
			{
				//cuz we have done an i++ before,so we need to roll back now
				nalhead_pos--;
				if(cam->encode.pkt.data[nalhead_pos++] == 0x00 &&
						cam->encode.pkt.data[nalhead_pos++] == 0x01)
					goto gotnal_head;
				else
					continue;
			}
		}
		else
			continue;

		//search for nal tail which is also the head of next nal
gotnal_head:
		//normal case:the whole nal is in this m_pFileBuf
		naltail_pos = nalhead_pos;
		while (naltail_pos<cam->encode.pkt.size)
		{
			if(cam->encode.pkt.data[naltail_pos++] == 0x00 &&
					cam->encode.pkt.data[naltail_pos++] == 0x00 )
			{
				if(cam->encode.pkt.data[naltail_pos++] == 0x01)
				{
					nalu.size = (naltail_pos-3)-nalhead_pos;
					break;
				}
				else
				{
					naltail_pos--;
					if(cam->encode.pkt.data[naltail_pos++] == 0x00 &&
							cam->encode.pkt.data[naltail_pos++] == 0x01)
					{
						nalu.size = (naltail_pos-4)-nalhead_pos;
						break;
					}
				}
			}
		}
		if(naltail_pos>=cam->encode.pkt.size)
			nalu.size = cam->encode.pkt.size-nalhead_pos;

		nalu.type = cam->encode.pkt.data[nalhead_pos]&0x1f;
		if(nalu.type==0x06)
		{
			nalhead_pos=nalu.size+nalhead_pos;
			continue;
		}
		memset(m_pFileBuf_tmp,0,nalu.size);
		memcpy(m_pFileBuf_tmp,cam->encode.pkt.data+nalhead_pos,nalu.size);
		nalu.data=m_pFileBuf_tmp;
		nalhead_pos=nalu.size+nalhead_pos;
		return TRUE;
	}
	return TRUE;
}

int RTMP264_Send(Camera *cam,void*lparam,decoder_handle_t* Nalulist,RTMPMetadata *metaDataforclients)
{
	int ret;
	TIMER now,last_update;
	int encodetime;
	int sleeptime=0;

	memset(&metaData,0,sizeof(RTMPMetadata));
	m_pFileBuf_tmp=(unsigned char*)malloc(BUFFER_SIZE);

	NaluUnit naluUnit;

	nalhead_pos=0;
	encode(cam);
	//SPS
	ReadOneNaluFromBuf(naluUnit,cam);
	metaData.nSpsLen = naluUnit.size;
	metaData.Sps=NULL;
	metaData.Sps=(unsigned char*)malloc(naluUnit.size);
	memcpy(metaData.Sps,naluUnit.data,naluUnit.size);

	metaDataforclients->nPpsLen=naluUnit.size;
	metaDataforclients->Sps=NULL;
	metaDataforclients->Sps=(unsigned char*)malloc(naluUnit.size);
	memcpy(metaDataforclients->Sps,naluUnit.data,naluUnit.size);
	//SAVA_Nal(naluUnit);

	pthread_mutex_lock(&(Nalulist->mutex));
	if (Nalulist->w_frame_idx >= FRAME_MAX_CNT)
		Nalulist->w_frame_idx = 0;
	int w_frame_idx = Nalulist->w_frame_idx;
	if (Nalulist->frame[w_frame_idx].abFrameData != NULL)
	{
		memcpy(Nalulist->frame[w_frame_idx].abFrameData, naluUnit.data, naluUnit.size);
		Nalulist->frame[w_frame_idx].nFrameDataMaxSize = naluUnit.size;
		Nalulist->frame[w_frame_idx].time=0;
		Nalulist->frame[w_frame_idx].sleeptime=0;
	}
	Nalulist->w_frame_idx++;
	Nalulist->frame_cnt_unuse--;
	pthread_mutex_unlock(&(Nalulist->mutex));

	//PPS
	ReadOneNaluFromBuf(naluUnit,cam);
	metaData.nPpsLen = naluUnit.size;
	metaData.Pps=NULL;
	metaData.Pps=(unsigned char*)malloc(naluUnit.size);
	memcpy(metaData.Pps,naluUnit.data,naluUnit.size);

	metaDataforclients->nPpsLen = naluUnit.size;
	metaDataforclients->Pps=NULL;
	metaDataforclients->Pps=(unsigned char*)malloc(naluUnit.size);
	memcpy(metaDataforclients->Pps,naluUnit.data,naluUnit.size);



	//SAVA_Nal
	pthread_mutex_lock(&(Nalulist->mutex));
	if (Nalulist->w_frame_idx >= FRAME_MAX_CNT)
		Nalulist->w_frame_idx = 0;
	w_frame_idx = Nalulist->w_frame_idx;
	if (Nalulist->frame[w_frame_idx].abFrameData != NULL)
	{
		memcpy(Nalulist->frame[w_frame_idx].abFrameData, naluUnit.data, naluUnit.size);
		Nalulist->frame[w_frame_idx].nFrameDataMaxSize = naluUnit.size;
		Nalulist->frame[w_frame_idx].time=0;
		Nalulist->frame[w_frame_idx].sleeptime=0;
	}
	Nalulist->w_frame_idx++;
	Nalulist->frame_cnt_unuse--;
	pthread_mutex_unlock(&(Nalulist->mutex));

	// ����SPS,��ȡ��Ƶͼ��?����Ϣ
	int width = 0,height = 0, fps=0;
	h264_decode_sps(metaData.Sps,metaData.nSpsLen,width,height,fps);
	metaData.nWidth = width;
	metaData.nHeight = height;
	metaDataforclients->nWidth = width;
	metaDataforclients->nHeight = height;


	//printf("%d,%d\n",width,height);
	if(fps)
	{
		metaData.nFrameRate = fps;
		metaDataforclients->nFrameRate=fps;
	}
	else
	{
		metaData.nFrameRate = 25;
		metaDataforclients->nFrameRate=25;
	}

	//����PPS,SPS

	//if(ret!=1)
	//	return FALSE;

	unsigned int tick = 0;
	unsigned int tick_gap = 1000/metaData.nFrameRate;
	ReadOneNaluFromBuf(naluUnit,cam);

	pthread_mutex_lock(&(Nalulist->mutex));
	if (Nalulist->w_frame_idx >= FRAME_MAX_CNT)
		Nalulist->w_frame_idx = 0;
	w_frame_idx = Nalulist->w_frame_idx;
	if (Nalulist->frame[w_frame_idx].abFrameData != NULL)
	{
		memcpy(Nalulist->frame[w_frame_idx].abFrameData, naluUnit.data, naluUnit.size);
		Nalulist->frame[w_frame_idx].nFrameDataMaxSize = naluUnit.size;
		Nalulist->frame[w_frame_idx].time=tick;
		Nalulist->frame[w_frame_idx].sleeptime=0;
	}
	Nalulist->w_frame_idx++;
	Nalulist->frame_cnt_unuse--;
	pthread_mutex_unlock(&(Nalulist->mutex));

	ret=SendVideoSpsPps(metaData.Pps,metaData.nPpsLen,metaData.Sps,metaData.nSpsLen,tick,lparam);

	int bKeyframe  = (naluUnit.type== 0x05) ? TRUE : FALSE;
	while(SendH264Packet(naluUnit.data,naluUnit.size,bKeyframe,tick,lparam))
	{
got_sps_pps:
		//if(naluUnit.size==8581)
		//printf("NALU size:%8d\n",naluUnit.size);
		GETTIME(&last_update);
		if(!ReadOneNaluFromBuf(naluUnit,cam))
				goto end;

		if(naluUnit.type == 0x07 || naluUnit.type == 0x08)
			goto got_sps_pps;
		bKeyframe  = (naluUnit.type == 0x05) ? TRUE : FALSE;
		tick +=tick_gap;
		GETTIME(&now);
		ELAPSEDTIME(last_update,now, encodetime, frequency);
		//cout<<"Gap:"<<tick_gap<<endl;
	//	cout<<"sumtime:"<<encodetime<<endl;
		sleeptime=tick_gap-encodetime/1000;
		if(sleeptime<=0)
			sleeptime=0;
		msleep(sleeptime);

		pthread_mutex_lock(&(Nalulist->mutex));
		if (Nalulist->w_frame_idx >= FRAME_MAX_CNT)
			Nalulist->w_frame_idx = 0;
		w_frame_idx = Nalulist->w_frame_idx;
		if (Nalulist->frame[w_frame_idx].abFrameData != NULL)
		{
			memcpy(Nalulist->frame[w_frame_idx].abFrameData, naluUnit.data, naluUnit.size);
			Nalulist->frame[w_frame_idx].nFrameDataMaxSize = naluUnit.size;
			Nalulist->frame[w_frame_idx].time=tick;
			Nalulist->frame[w_frame_idx].sleeptime=sleeptime;
		}
		Nalulist->w_frame_idx++;
		Nalulist->frame_cnt_unuse--;
		pthread_mutex_unlock(&(Nalulist->mutex));

		//msleep(tick_gap-now+last_update);
		//msleep(40);
	}
	end:
	free(metaData.Sps);
	free(metaData.Pps);

	return TRUE;
}


int RTMP_Send_Clients(void*lparam,void *data)
{
	NaluUnit naluUnit;
	frame_data_t *nalu=(frame_data_t *)data;
	int bKeyframe ;


	naluUnit.data=(unsigned char *)nalu->abFrameData;
	naluUnit.size=nalu->nFrameDataMaxSize;;
	naluUnit.type = nalu->abFrameData[0]&0x1f;
	//ReadOneNaluFromBuf(naluUnit,cam);
	//SAVA_Nal(naluUnit);

	bKeyframe  = (naluUnit.type== 0x05) ? TRUE : FALSE;
	SendH264Packet(naluUnit.data,naluUnit.size,bKeyframe,nalu->time,lparam);
	msleep(nalu->sleeptime);
	return TRUE;
}

void putdata(decoder_handle_t* Nalulist,void*lparam,RTMPMetadata *metaData)
{
	SendVideoSpsPps(metaData->Pps,metaData->nPpsLen,metaData->Sps,metaData->nSpsLen,0,lparam);
	SendH264Packet(metaData->Sps,metaData->nSpsLen,1,0,lparam);
	SendH264Packet(metaData->Pps,metaData->nPpsLen,1,0,lparam);


	int r_frame_idx=0;
	r_frame_idx=Nalulist->frame[0].time>Nalulist->frame[FRAME_MAX_CNT-1].time?0:(FRAME_MAX_CNT-1);
	while(1)
		{
			pthread_mutex_lock(&(Nalulist->mutex));
			if (Nalulist->frame_cnt_unuse >= FRAME_MAX_CNT)
			{
				pthread_mutex_unlock(&(Nalulist->mutex));
				usleep(1000);
				continue;
			}
			//r_frame_idx = Nalulist->r_frame_idx;
			RTMP_Send_Clients(lparam,&Nalulist->frame[r_frame_idx]);

			r_frame_idx++;
			if (r_frame_idx >= FRAME_MAX_CNT)
				r_frame_idx = 0;

			Nalulist->r_frame_idx++;
			if (Nalulist->r_frame_idx >= FRAME_MAX_CNT)
				Nalulist->r_frame_idx = 0;
			Nalulist->frame_cnt_unuse++;
			pthread_mutex_unlock(&(Nalulist->mutex));
		}
}

