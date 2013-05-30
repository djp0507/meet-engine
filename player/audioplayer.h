/*
 * Copyright (C) 2012 Roger Shen  rogershen@pptv.com
 *
 */
 
#ifndef FF_AUDIO_PLAYER_H
#define FF_AUDIO_PLAYER_H

#include <pthread.h>
#include <sys/types.h>

#include "include-pp/IPlayer.h"
#include "packetqueue.h"
#include "ffstream.h"

using namespace android;

class AVStream;
class AudioRender;
class AudioPlayer
{
public:
	AudioPlayer();
    AudioPlayer(FFStream* dataStream, AVStream* context, int32_t streamIndex);
    ~AudioPlayer();
	
	status_t prepare();
	status_t start();
	status_t stop();
	status_t pause();
	status_t flush();
	status_t setMediaTimeMs(int64_t timeMs);
    int32_t getStatus(){ return mPlayerStatus;}
	int64_t getMediaTimeMs(); 
    status_t setListener(MediaPlayerListener* listener);
    status_t seekTo(int64_t msec);

private:
	status_t start_l();
	status_t stop_l();
	status_t pause_l();
	status_t flush_l();
	void run();
    int32_t decode_l(AVPacket *packet);
	static void* handleThreadStart(void* ptr);
	void render_l(int16_t* buffer, uint32_t buffer_size);
	status_t join_l();
    void notifyListener_l(int msg, int ext1 = 0, int ext2 = 0);

private:
	bool mRunning;
    uint32_t mPlayerStatus;
    int16_t* mSamples;
    uint32_t mSamplesSize;
    int64_t mNumFramesPlayed;
    uint32_t mLatencyMs;
	int64_t mPositionTimeMediaMs;
	bool mReachEndStream;
	int64_t mOutputBufferingStartMs;
	int64_t mAvePacketDurationMs;
	int32_t mAudioStreamIndex;
	bool mSeeking;
    MediaPlayerListener* mListener;
	AudioRender* mRender;

	FFStream* mDataStream;
    AVStream* mAudioContext;
    int64_t mDurationMs;
	int64_t mLastPacketMs;
	
    pthread_t mThread;
    pthread_mutex_t mLock;
    pthread_cond_t mCondition;

};

#endif //FF_AUDIO_PLAYER_H