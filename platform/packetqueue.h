/*
 * Copyright (C) 2012 Roger Shen  rogershen@pptv.com
 *
 */
 
#ifndef FF_PACKETQUEUE_H
#define FF_PACKETQUEUE_H
#include "list.h"
#include "errors.h"

class AVPacket;
class PacketQueue
{
public:
	PacketQueue();
	~PacketQueue();
	
	status_t put(AVPacket* pkt);
	AVPacket* get();
	uint32_t size();
	uint32_t count();
	void flush();
	
private:
    uint32_t mCachedSize;
    uint32_t mCount;
	List mPacketList;
};

#endif // FF_PACKETQUEUE_H