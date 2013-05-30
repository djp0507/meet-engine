/*
 * Copyright (C) 2012 Roger Shen  rogershen@pptv.com
 *
 */


#ifndef FF_PLAYER_H_

#define FF_PLAYER_H_
#include <stdarg.h>

#include "errors.h"
#include "include-pp/IPlayer.h"
#include "loop.h"
#include "errors.h"
#include "audioplayer.h"
#include "packetqueue.h"
#include "ffstream.h"

using namespace android;

class AVFormatContext;
class AVStream;
class AVPacket;
class AVFrame;
enum AVDiscard;
class FFRender;
class FFPlayer : public IPlayer, MediaPlayerListener
{
public:
    FFPlayer();
    ~FFPlayer();

    status_t setDataSource(const char* url);
	status_t setDataSource(int32_t fd, int64_t offset, int64_t length);
	status_t setVideoSurface(void* surface);
	status_t prepare();
	status_t prepareAsync();
    status_t start();
    status_t stop();
    status_t pause();
    status_t reset();
    status_t seekTo(int32_t msec);
    status_t getVideoWidth(int32_t* w);
    status_t getVideoHeight(int32_t* h);
    status_t getCurrentPosition(int32_t* msec);
    status_t getDuration(int32_t* msec);
    status_t setAudioStreamType(int32_t type);
	status_t setLooping(int32_t loop);
	status_t setVolume(float leftVolume, float rightVolume);
    status_t setListener(MediaPlayerListener* listener);
	int32_t flags();
	bool isLooping();
    bool isPlaying();
	
	status_t startCompatibilityTest();
	void stopCompatibilityTest(){}
    status_t startP2PEngine(){return OK;}
    void stopP2PEngine(){}
	void disconnect(){}
	status_t suspend();
    status_t resume();
	
    void notify(int32_t msg, int32_t ext1 = 0, int32_t ext2 = 0);

	bool getMediaInfo(const char* url, MediaInfo* info);
	bool getMediaDetailInfo(const char* url, MediaInfo* info);
	bool getThumbnail(const char* url, MediaInfo* info);

private:
    friend struct FFEvent;

    enum Flags {
		CAN_SEEK_BACKWARD = 1,
		CAN_SEEK_FORWARD = 2,
		CAN_PAUSE = 4,
    };
	
	status_t prepare_l();
	status_t prepareAsync_l();
    status_t play_l();
	status_t stop_l();
    status_t reset_l();
    status_t seekTo_l();
    status_t pause_l();
    void notifyListener_l(int msg, int ext1 = 0, int ext2 = 0);
    void abortPrepare_l(status_t err);
    void cancelPlayerEvents_l();
	status_t prepareAudio_l();
	status_t prepareVideo_l();
	status_t decode_l(AVPacket* packet);
    static void setFFmpegLogCallback(void* ptr, int level, const char* fmt, va_list vl);
	bool isPacketLate_l(AVPacket* packet);
	void optimizeDecode_l(AVPacket* packet);
	int64_t getFramePTS_l(AVFrame* frame);
	
    void postPrepareEvent_l();
    void postStreamDoneEvent_l();
    void postVideoEvent_l(int64_t delayMs = -1);
    void postBufferingUpdateEvent_l();
    void postSeekingEvent_l(int64_t delayMs = -1);
    void postCheckAudioStatusEvent_l();
    void postBufferingStartEvent_l();
    void postBufferingEndEvent_l();
    void postSeekingCompleteEvent_l();

	void onPrepare();
    void onStreamDone();
    void onVideo();
    void onBufferingUpdate();
    void onCheckAudioStatus();
    void onPrepareAsync();
    void onSeeking();
    void onBufferingStart();
    void onBufferingEnd();
    void onSeekingComplete();

    FFPlayer(const FFPlayer &);
    FFPlayer &operator=(const FFPlayer &);

private:
    char* mUri;
    int64_t mDurationMs;
	uint32_t mVideoFrameRate;
    uint32_t mPlayerStatus;
    uint32_t mPlayerFlags;
    int32_t mVideoWidth, mVideoHeight;
    int32_t mVideoFormat;
    int64_t mVideoTimeMs;
	bool mLooping;
    int64_t mSeekTimeMs;
	bool mSeeking;
    bool mRenderFirstFrame;
    bool mGetFirstFrame;
	bool mIsBuffering;
	int64_t mAveVideoDecodeTimeMs;
	int64_t mCompensateMs;
	int64_t mMaxLatenessMs;
	int64_t mMinLatenessMs;
	int64_t mMaxMaxLatenessMs;
	int64_t mLatenessMs;
	AVDiscard mDiscardLevel;
	uint32_t mDiscardCount;
	int64_t mVideoPlayingTimeMs;

	FFStream* mDataStream;
    MediaPlayerListener* mListener;
    AudioPlayer* mAudioPlayer;
	void* mSurface;
    AVFrame* mVideoFrame;
	bool mIsVideoFrameDirty;
    FFRender* mVideoRenderer;
	bool mReachEndStream;
	
	AVFormatContext* mMovieFile;
	int32_t mAudioStreamIndex;
	int32_t mVideoStreamIndex;
	AVStream* mAudioStream;
    AVStream* mVideoStream;

    Loop::Event* mPrepareEvent;
	bool mPrepareEventPending;
    Loop::Event* mVideoEvent;
    bool mVideoEventPending;
    //Loop::Event* mStreamReadEvent;
	//bool mStreamReadEventPending;
    Loop::Event* mStreamDoneEvent;
    bool mStreamDoneEventPending;
    Loop::Event* mBufferingEvent;
    bool mBufferingEventPending;
    Loop::Event* mSeekingEvent;
    bool mSeekingEventPending;
    Loop::Event* mCheckAudioStatusEvent;
    bool mAudioStatusEventPending;
    Loop::Event* mBufferingStartEvent;
    bool mBufferingStartEventPending;
    Loop::Event* mBufferingEndEvent;
    bool mBufferingEndEventPending;
    Loop::Event* mSeekingCompleteEvent;
    bool mSeekingCompleteEventPending;
	
    Loop mQueue;
    pthread_mutex_t mLock;
    pthread_cond_t mPreparedCondition;
    status_t mPrepareResult;
    bool mRunningCompatibilityTest;
};


#endif

