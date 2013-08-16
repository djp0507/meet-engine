/*
 * Copyright (C) 2012 Roger Shen  rogershen@pptv.com
 *
 */


#ifndef FF_STREAM_H_

#define FF_STREAM_H_

#include <pthread.h>
#include "player.h"
#include "packetqueue.h"

#define FFSTREAM_OK 0
#define FFSTREAM_ERROR -1
#define FFSTREAM_ERROR_EOF -2
#define FFSTREAM_ERROR_BUFFERING -3
#define FFSTREAM_ERROR_STREAMINDEX -4
#define FFSTREAM_ERROR_FLUSHING -4

#define FFSTREAM_INITED 1
#define FFSTREAM_PREPARED 1
#define FFSTREAM_STARTED 2
#define FFSTREAM_PAUSED 3
#define FFSTREAM_STOPPED 4


//using namespace android;


class AVFormatContext;
class AVPacket;
class FFStream
{
public:
	FFStream();
	~FFStream();

	enum URL_TYPE
	{
		TYPE_LOCAL_FILE,
		TYPE_HTTP,
		TYPE_UNKNOWN
	};
	
	status_t selectAudioChannel(int32_t index);
	AVFormatContext* open(char* uri);
	status_t start();
	status_t stop();
	status_t seek(int64_t seekTimeMs);

    status_t setListener(MediaPlayerListener* listener);
	status_t getPacket(int32_t streamIndex, AVPacket** packet);
	status_t status(){return mStatus;}
	status_t isSeeking(){return mSeeking;}
	int64_t getDurationMs(){return mDurationMs;}
	int64_t getCachedDurationMs(){return mCachedDurationMs;}
	AVStream* getAudioStream(){return mAudioStream;}
	AVStream* getVideoStream(){return mVideoStream;}
	int32_t getAudioStreamIndex(){return mAudioStreamIndex;}
	int32_t getVideoStreamIndex(){return mVideoStreamIndex;}
	URL_TYPE getURLType(){return mUrlType;}
	
private:
	status_t flush_l();
	static void* handleThreadStart(void* ptr);
	void run();
	status_t join_l();
	static status_t interrupt_l(void *ctx);
    void notifyListener_l(int msg, int ext1 = 0, int ext2 = 0);

private:
	status_t mStatus;
	bool mRunning;
	AVFormatContext* mMovieFile;
    int64_t mDurationMs;
	uint32_t mStreamsCount;
    MediaPlayerListener* mListener;
	uint32_t mBufferSize;
	uint32_t mMinBufferCount;
	uint32_t mMaxBufferSize;
	bool mReachEndStream;
	bool mIsBuffering;
	int64_t mSeekTimeMs;
	bool mSeeking;
	int64_t mCachedDurationMs;
	URL_TYPE mUrlType;
	
	//audio
	int32_t mAudioChannelSelected;
	int32_t mAudioStreamIndex;
	AVStream* mAudioStream;
    PacketQueue mAudioQueue;
    //pthread_mutex_t mAudioQueueLock;
	//int32_t mAudioBufferSize;

	//video
	int32_t mVideoStreamIndex;
	AVStream* mVideoStream;
    PacketQueue mVideoQueue;
    //pthread_mutex_t mVideoQueueLock;
	//int32_t mVideoBufferSize;
	
    pthread_t mThread;
    pthread_cond_t mCondition;
    pthread_mutex_t mLock;
};


#endif

