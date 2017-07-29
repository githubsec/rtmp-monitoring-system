/*******************************************************************************
--                                                                            --
--                    CedarX Multimedia Framework                             --
--                                                                            --
--          the Multimedia Framework for Linux/Android System                 --
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                         Softwinner Products.                               --
--                                                                            --
--                   (C) COPYRIGHT 2011 SOFTWINNER PRODUCTS                   --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
*******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "include/message.h"

static int TMessageDeepCopyMessage(message_t *pDesMsg, message_t *pSrcMsg)
{
    pDesMsg->command = pSrcMsg->command;
    pDesMsg->mpData = NULL;
    pDesMsg->mDataSize = 0;
    if(pSrcMsg->mpData && pSrcMsg->mDataSize>=0)
    {
        pDesMsg->mpData = malloc(pSrcMsg->mDataSize);
        if(pDesMsg->mpData)
        {
            pDesMsg->mDataSize = pSrcMsg->mDataSize;
            memcpy(pDesMsg->mpData, pSrcMsg->mpData, pSrcMsg->mDataSize);
        }
        else
        {
            return -1;
        }
    }
    return 0;
}

static int TMessageSetMessage(message_t *pDesMsg, message_t *pSrcMsg)
{
    pDesMsg->command = pSrcMsg->command;
    pDesMsg->mpData = pSrcMsg->mpData;
    pDesMsg->mDataSize = pSrcMsg->mDataSize;
    return 0;
}

static int TMessageIncreaseIdleMessageList(message_queue_t* pThiz)
{
	mBuffer *pMBDB = (mBuffer *)malloc(sizeof(mBuffer));
	if(NULL == pMBDB)
	{
		//ALOGE("(f:%s, l:%d) fatal error! malloc fail", __FUNCTION__, __LINE__);
		return -1;
	}

	pMBDB->mpBuffer = (char*)malloc(sizeof(message_t)*MAX_MESSAGE_ELEMENTS);
	if (NULL == pMBDB->mpBuffer)
	{
		//ALOGE("(f:%s, l:%d) Failed to alloc buffer size[%d]", __FUNCTION__, __LINE__, sizeof(message_t)*MAX_MESSAGE_ELEMENTS);
		free(pMBDB);
		return -1;
	}
	pMBDB->mSize = sizeof(message_t)*MAX_MESSAGE_ELEMENTS;
	list_add_tail(&pMBDB->mList, &pThiz->mMessageBufList);

	message_t *pMsgPtr = (message_t*)(pMBDB->mpBuffer);
	message_t *pMsg;
	int i;
	for(i=0;i<MAX_MESSAGE_ELEMENTS;i++)
	{
		pMsg = pMsgPtr++;
		list_add_tail(&pMsg->mList, &pThiz->mIdleMessageList);
	}
	return 0;
}

int message_create(message_queue_t* msg_queue)
{
	int         ret;
	ret = pthread_mutex_init(&msg_queue->mutex, NULL);
	if (ret!=0)
	{
		return -1;
	}
	pthread_condattr_t  condAttr;
	pthread_condattr_init(&condAttr);
	ret = pthread_cond_init(&msg_queue->mCondMessageQueueChanged, &condAttr);
	if(ret!=0)
	{
		//ALOGE("[%s](f:%s, l:%d) fatal error! pthread cond init fail", strrchr(__FILE__, '/')+1, __FUNCTION__, __LINE__);
		goto _err0;
	}
	msg_queue->mWaitMessageFlag = 0;
	INIT_LIST_HEAD(&msg_queue->mMessageBufList);
	INIT_LIST_HEAD(&msg_queue->mIdleMessageList);
	INIT_LIST_HEAD(&msg_queue->mReadyMessageList);
	if(0!=TMessageIncreaseIdleMessageList(msg_queue))
	{
		goto _err1;
	}
	msg_queue->message_count = 0;

	return 0;
_err1:
	pthread_cond_destroy(&msg_queue->mCondMessageQueueChanged);
_err0:
	pthread_mutex_destroy(&msg_queue->mutex);
	return -1;
}


void message_destroy(message_queue_t* msg_queue)
{
	pthread_mutex_lock(&msg_queue->mutex);
	pthread_mutex_destroy(&msg_queue->mutex);
}


void flush_message(message_queue_t* msg_queue)
{
	pthread_mutex_lock(&msg_queue->mutex);
	pthread_mutex_unlock(&msg_queue->mutex);
}

/*******************************************************************************
Function name: put_message
Description:
    Do not accept mpData when call this function.
Parameters:

Return:

Time: 2015/3/6
*******************************************************************************/
int put_message(message_queue_t* msg_queue, message_t *msg_in)
{
    message_t message;
    memset(&message, 0, sizeof(message_t));
    message.command = msg_in->command;
    message.mpData = NULL;
    message.mDataSize = 0;
    return putMessageWithData(msg_queue, &message);
}

int get_message(message_queue_t* msg_queue, message_t *msg_out)
{
	pthread_mutex_lock(&msg_queue->mutex);
	if(list_empty(&msg_queue->mReadyMessageList))
	{
		pthread_mutex_unlock(&msg_queue->mutex);
		return -1;
	}
	message_t   *pMessageEntry = list_first_entry(&msg_queue->mReadyMessageList, message_t, mList);
	TMessageSetMessage(msg_out, pMessageEntry);
	list_move_tail(&pMessageEntry->mList, &msg_queue->mIdleMessageList);
	msg_queue->message_count--;

    pthread_mutex_unlock(&msg_queue->mutex);
	return 0;
}

int putMessageWithData(message_queue_t* msg_queue, message_t *msg_in)
{
    int ret = 0;
	pthread_mutex_lock(&msg_queue->mutex);
	if(list_empty(&msg_queue->mIdleMessageList))
	{
		//ALOGW("(f:%s, l:%d) idleMessageList are all used, malloc more!", __FUNCTION__, __LINE__);

		if(0!=TMessageIncreaseIdleMessageList(msg_queue))
		{
			pthread_mutex_unlock(&msg_queue->mutex);
			return -1;
		}
	}
	message_t   *pMessageEntry = list_first_entry(&msg_queue->mIdleMessageList, message_t, mList);
	if(0==TMessageDeepCopyMessage(pMessageEntry, msg_in))
	{
		list_move_tail(&pMessageEntry->mList, &msg_queue->mReadyMessageList);
		msg_queue->message_count++;
		//ALOGV("(f:%s, l:%d) new msg command[%d], para[%d][%d] pData[%p]size[%d]", __FUNCTION__, __LINE__,
		//	pMessageEntry->command, pMessageEntry->para0, pMessageEntry->para1, pMessageEntry->mpData, pMessageEntry->mDataSize);
		if(msg_queue->mWaitMessageFlag)
		{
			pthread_cond_signal(&msg_queue->mCondMessageQueueChanged);
		}
	}
	else
	{
		ret = -1;
	}
    pthread_mutex_unlock(&msg_queue->mutex);
	return ret;
}

int get_message_count(message_queue_t* msg_queue)
{
	int message_count;

	pthread_mutex_lock(&msg_queue->mutex);
	message_count = msg_queue->message_count;
	pthread_mutex_unlock(&msg_queue->mutex);

	return message_count;
}

int TMessage_WaitQueueNotEmpty(message_queue_t* msg_queue, unsigned int timeout)
{
	int message_count;
	pthread_mutex_lock(&msg_queue->mutex);
	msg_queue->mWaitMessageFlag = 1;
	if(timeout <= 0)
	{
		while(list_empty(&msg_queue->mReadyMessageList))
		{
			pthread_cond_wait(&msg_queue->mCondMessageQueueChanged, &msg_queue->mutex);
		}
	}
	else
	{
		if(list_empty(&msg_queue->mReadyMessageList))
		{
			/*int ret = pthread_cond_timeout_np(&msg_queue->mCondMessageQueueChanged, &msg_queue->mutex, timeout);
			if(ETIMEDOUT == ret)
			{
				//ALOGD("(f:%s, l:%d) pthread cond timeout np timeout[%d]", __FUNCTION__, __LINE__, ret);
			}
			else if(0 == ret)
			{
			}
			else
			{
				//ALOGE("(f:%s, l:%d) fatal error! pthread cond timeout np[%d]", __FUNCTION__, __LINE__, ret);
			}*/
		}
	}
	msg_queue->mWaitMessageFlag = 0;
	message_count = msg_queue->message_count;
	pthread_mutex_unlock(&msg_queue->mutex);
	return message_count;
}

