/*
 * Copyright (C) 2012 Roger Shen  rogershen@pptv.com
 *
 */

#define LOG_TAG "PacketQueue"

extern "C" {
	
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
	
} // end of extern C

#include "log.h"
#include "packetqueue.h"

PacketQueue::PacketQueue()
{
    mCachedSize = 0;
    mCount = 0;
}

PacketQueue::~PacketQueue()
{
    flush();
    LOGD("PacketQueue destructor");
}

status_t PacketQueue::put(AVPacket* pkt)
{
    if(pkt == NULL)
    {
        return ERROR;
    }
	mPacketList.Append(pkt);
    mCachedSize += pkt->size;
    mCount++;
    LOGD("mCount:%d", mCount);
	
    return OK;
}

AVPacket* PacketQueue::get()
{
    AVPacket* pPacket = (AVPacket*)mPacketList.Remove(0);
    if(pPacket != NULL)
    {
        mCachedSize -= pPacket->size;
        mCount--;
    }
    else
    {
        mCachedSize = 0;
        mCount = 0;
    }
    return pPacket;
}

void PacketQueue::flush()
{
    while(!mPacketList.IsEmpty())
    {
        AVPacket* pPacket = (AVPacket*)mPacketList.Remove(0);
        av_free_packet(pPacket);
        av_free(pPacket);
        pPacket = NULL;
        mCount--;
    }
}

uint32_t PacketQueue::size()
{
    return mCachedSize;
}

uint32_t PacketQueue::count()
{
    return mCount;
}

