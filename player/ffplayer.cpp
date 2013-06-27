/*
 * Copyright (C) 2012 Roger Shen  rogershen@pptv.com
 *
 */
#define __STDINT_LIMITS
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>

extern "C" {
	
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavutil/log.h"
	
} // end of extern C

#define LOG_TAG "FFPlayer"
#include "log.h"
#include "yuv_rgb.h"
#include "surface.h"
#include "platforminfo.h"
#include "utils.h"
#include "ffplayer.h"
#include "autolock.h"
#include "cpu-features.h"

static FFPlayer* sPlayer = NULL;

#ifdef OS_ANDROID
#include <jni.h>
PlatformInfo* gPlatformInfo = NULL;
JavaVM* gs_jvm = NULL;
#endif

extern "C" IPlayer* getPlayer(
#ifdef OS_ANDROID
	JavaVM* jvm, 
	PlatformInfo* platformInfo,
	bool startP2PEngine
#endif
                              ) {
#ifdef OS_ANDROID	
	gs_jvm = jvm;
	gPlatformInfo = platformInfo;
#endif
	return new FFPlayer();
}

struct FFEvent : public Loop::Event
{
    FFEvent(   FFPlayer *player, void (FFPlayer::*method)())
        : mPlayer(player),
          mMethod(method)
    {
    }

protected:
    virtual ~FFEvent() {}

    virtual void fire(Loop *queue, int64_t /* nowMs */)
    {
        (mPlayer->*mMethod)();
    }

private:
    FFPlayer *mPlayer;
    void (FFPlayer::*mMethod)();

    FFEvent(const FFEvent &);
    FFEvent &operator=(const FFEvent &);
};

class FFRender
{
public:
    FFRender(void* surface, uint32_t frameWidth, uint32_t frameHeight, int32_t format)
    {
        mSurface = surface;
        mFrameWidth = frameWidth;
        mFrameHeight = frameHeight;
        mFrameFormat = (PixelFormat)format;
        mSurfaceWidth = 0;
        mSurfaceHeight = 0;
        mSwsFlags = SWS_POINT;
	    mAveScaleTimeMs = 0;
        mConvertCtx = NULL;
        mSurfaceFrame = NULL;
        mScaleFrame = NULL;
        mScalePixels = NULL;
    }
    
    status_t init()
    {
#ifdef OS_ANDROID
    	if (Surface_open(mSurface)!= OK)
        {
            return ERROR;
    	}
    	if (Surface_getRes(&mSurfaceWidth, &mSurfaceHeight) != OK)
        {
            return ERROR;
    	}
#endif

#ifdef OS_IOS
        if (Surface_open(mSurface, mFrameWidth, mFrameHeight, mFrameFormat) != OK)
        {
            return ERROR;
        }
#endif
        adjust();
        return OK;
    }

    status_t width(uint32_t& surfaceWidth)
    {
        surfaceWidth = mOptiSurfaceWidth;
        return OK;
    }

    status_t height(uint32_t& surfaceHeight)
    {
        surfaceHeight = mOptiSurfaceHeight;
        return OK;
    }

    uint32_t swsMs()
    {
        return mAveScaleTimeMs;
    }

    status_t render(AVFrame* frame)
    {
#ifdef OS_ANDROID
        uint64_t cpuFeatures = android_getCpuFeatures();
        if ((cpuFeatures & ANDROID_CPU_ARM_FEATURE_NEON) != 0 &&
            (mFrameFormat == PIX_FMT_YUV420P ||
                mFrameFormat == PIX_FMT_NV12 ||
                mFrameFormat == PIX_FMT_NV21))
        {
            return render_neon(frame);
        }
        else
        {
            return render_sws(frame);
        }
#endif

#ifdef OS_IOS
        return render_opengl(frame);
#endif
    }
    
#ifdef OS_ANDROID
    //neon accelerate
    status_t render_neon(AVFrame* frame)
    {
        void* surfacePixels = NULL;
    	if (Surface_getPixels(NULL, NULL, &surfacePixels) != OK)
        {
            return ERROR;
    	}
        
        int64_t begin_scale = getNowMs();

        //Convert format
        switch(mFrameFormat)
        {
            case PIX_FMT_YUV420P:
            {
                LOGD("frame->data[0]:%p,frame->data[1]:%p,frame->data[2]:%p", ((((int32_t)frame->data[0])+0x20)&0xffffffe0), frame->data[1], frame->data[2]);
                LOGD("frame->linesize[0]:%d", frame->linesize[0]);
                LOGD("frame->width:%d, frame->height:%d", frame->width, frame->height);
                struct yuv_pack out = {surfacePixels, mOptiSurfaceWidth*4};
                struct yuv_planes in = {frame->data[0], frame->data[1], frame->data[2], frame->linesize[0]};
                //i420_rgb_neon (&out, &in, frame->width, frame->height);
                if(mOptiSurfaceWidth <= frame->width)
                {
                    i420_rgb_neon (&out, &in, mOptiSurfaceWidth, frame->height);
                }
                else
                {
                    //the real video info is not consistent with head info
                    LOGE("the real video info is not consistent with head info");
                }
                break;
            }
            case PIX_FMT_NV12:
            {
                struct yuv_pack out = {surfacePixels, mOptiSurfaceWidth*4};
                struct yuv_planes in = {frame->data[0], frame->data[1], frame->data[2], frame->linesize[0]};
                nv12_rgb_neon (&out, &in, mOptiSurfaceWidth, frame->height);
                break;
            }
            case PIX_FMT_NV21:
            {
                struct yuv_pack out = {surfacePixels, mOptiSurfaceWidth*4};
                struct yuv_planes in = {frame->data[0], frame->data[1], frame->data[2], frame->linesize[0]};
                nv21_rgb_neon (&out, &in, mOptiSurfaceWidth, frame->height);
                break;
            }
            default:
                LOGE("Video output format:%d does not support", mFrameFormat);
                return ERROR;
        }
        
        LOGD("before rendering frame");
        if(Surface_updateSurface() != OK)
    	{
            LOGE("Failed to render picture");
            return ERROR;
    	}
        LOGD("after rendering frame");
        
    	int64_t end_scale = getNowMs();
        int64_t costTime = end_scale-begin_scale;
        if(mAveScaleTimeMs == 0)
        {
            mAveScaleTimeMs = costTime;
        }
        else
        {
            mAveScaleTimeMs = (mAveScaleTimeMs*4+costTime)/5;
        }
    	LOGD("neon scale picture cost %lld[ms]", costTime);
    	LOGV("mAveScaleTimeMs %lld[ms]", mAveScaleTimeMs);

        //For debug
        /*
        char path[1024] = {0};
	    static int num=0;
        num++;
        sprintf(path, "/mnt/sdcard/frame_rgb_%d", num);
        LOGD("mSurfaceFrame->linesize[0]:%d, mOptiSurfaceHeight:%d", mSurfaceFrame->linesize[0], mOptiSurfaceHeight);
    	saveFrameRGB(mSurfaceFrame->data[0], mSurfaceFrame->linesize[0], mOptiSurfaceHeight, path);
    	*/
        return OK;
    }
#endif

    //use ffmpeg sws_scale
    status_t render_sws(AVFrame* frame)
    {
        if(mConvertCtx == NULL || mSurfaceFrame == NULL)
        {
            if(mConvertCtx != NULL)
            {
                sws_freeContext(mConvertCtx);
                mConvertCtx = NULL;
            }
            if(mSurfaceFrame != NULL)
            {
                avcodec_free_frame(&mSurfaceFrame);
            }
            //just do color format convertion
            //avoid doing scaling as it cost lots of cpu
            mConvertCtx = sws_getContext(mOptiSurfaceWidth,//mFrameWidth,
        								 mFrameHeight,
        								 mFrameFormat,
        								 mOptiSurfaceWidth,//mFrameWidth,
        								 mFrameHeight,//mOptiSurfaceHeight,
        								 PIX_FMT_RGB0,//PIX_FMT_RGB565,//
        								 mSwsFlags,
        								 NULL,
        								 NULL,
        								 NULL);
        	if (mConvertCtx == NULL) {
        		LOGE("create convert ctx failed, width:%d, height:%d, pix:%d", 
        					mOptiSurfaceWidth,
        					mFrameHeight,
        					mFrameFormat);
        		return ERROR;
        	}

            mSurfaceFrame = avcodec_alloc_frame();
            if (mSurfaceFrame == NULL)
            {
                LOGE("alloc frame failed");
                return ERROR;
            }

        }
        
        void* surfacePixels = NULL;
    	if (Surface_getPixels(NULL, NULL, &surfacePixels) != OK)
        {
            return ERROR;
    	}

        int ret = avpicture_fill((AVPicture *)mSurfaceFrame,
                            (uint8_t *)surfacePixels,
                            PIX_FMT_RGB0,//PIX_FMT_RGB565,//
                            mOptiSurfaceWidth,
                            mOptiSurfaceHeight);
    	if(ret < 0)
    	{
    		LOGE("avpicture_fill failed, ret:%d", ret);
    		return ERROR;
    	}
        
        // Convert the image
        int64_t begin_scale = getNowMs();	
        sws_scale(mConvertCtx,
            	      frame->data,
            	      frame->linesize,
                      0,
            	      frame->height,
            	      mSurfaceFrame->data,
                      mSurfaceFrame->linesize);
        
        LOGD("before rendering frame");
        if(Surface_updateSurface() != OK)
    	{
            LOGE("Failed to render picture");
            return ERROR;
    	}
        LOGD("after rendering frame");
        
    	int64_t end_scale = getNowMs();
        int64_t costTime = end_scale-begin_scale;
        if(mAveScaleTimeMs == 0)
        {
            mAveScaleTimeMs = costTime;
        }
        else
        {
            mAveScaleTimeMs = (mAveScaleTimeMs*4+costTime)/5;
        }
    	LOGD("sws scale picture cost %lld[ms]", costTime);
    	LOGV("mAveScaleTimeMs %lld[ms]", mAveScaleTimeMs);

        //For debug
        /*
        char path[1024] = {0};
	    static int num=0;
        num++;
        sprintf(path, "/mnt/sdcard/frame_rgb_%d", num);
        LOGD("mSurfaceFrame->linesize[0]:%d, mOptiSurfaceHeight:%d", mSurfaceFrame->linesize[0], mOptiSurfaceHeight);
    	saveFrameRGB(mSurfaceFrame->data[0], mSurfaceFrame->linesize[0], mOptiSurfaceHeight, path);
    	*/
        return OK;
    }

    status_t render_opengl(AVFrame* frame)
    {
    
        if (mFrameFormat == PIX_FMT_YUV420P || mFrameFormat == PIX_FMT_YUVJ420P)
        {
            if(Surface_displayPicture(frame) != OK)
            {
                LOGE("Failed to render picture");
                return ERROR;
            }
        }
        else
        {
            //TODO
            LOGE("picture format unsupported");
        }
        return OK;
    }

	~FFRender()
    {
        if(mConvertCtx != NULL)
        {
            sws_freeContext(mConvertCtx);
            mConvertCtx = NULL;
        }
        if(mSurfaceFrame != NULL)
        {
            avcodec_free_frame(&mSurfaceFrame);
            mSurfaceFrame = NULL;
        }
        if(mScalePixels != NULL)
        {
            free(mScalePixels);
            mScalePixels = NULL;
        }
        if(mScaleFrame != NULL)
        {
            avcodec_free_frame(&mScaleFrame);
            mScaleFrame = NULL;
        }
        
        Surface_close();
        mSurface = NULL;
        LOGD("FFRender destructor");
    }

private:
    void adjust()
    {
#ifdef OS_ANDROID
        if(mSurfaceWidth <= 480)
        {
            mOptiSurfaceWidth = ((mFrameWidth-1)&0xFFC0);//+64;
        }
        else
        {
            mOptiSurfaceWidth = ((mFrameWidth-1)&0xFFE0);//+32;
        }
        //height does not need round.
        mOptiSurfaceHeight = mFrameHeight;
#endif

#ifdef OS_IOS
        mOptiSurfaceWidth = mFrameWidth;
        mOptiSurfaceHeight = mFrameHeight;
#endif
            
        LOGD("mSurfaceWidth:%d", mSurfaceWidth);
        LOGD("mSurfaceHeight:%d", mSurfaceHeight);
        LOGD("mOptiSurfaceWidth:%d", mOptiSurfaceWidth);
        LOGD("mOptiSurfaceHeight:%d", mOptiSurfaceHeight);
    }
    
    //For debug
    void saveFrameRGB(void* data, int stride, int height, char* path)
    {
    	if(path==NULL) return;
    	
    	FILE *pFile;
    	LOGD("Start open file %s", path);
    	pFile=fopen(path, "wb");
    	if(pFile==NULL)
    	{
    		LOGE("open file %s failed", path);
    		return;
    	}
    	LOGD("open file %s success", path);

    	fwrite(data, 1, stride*height, pFile);
    	fclose(pFile);
    }


private:
    void* mSurface;
    uint32_t mFrameWidth;
    uint32_t mFrameHeight;
    PixelFormat mFrameFormat;
    struct SwsContext* mConvertCtx;
    AVFrame* mSurfaceFrame;
    AVFrame* mScaleFrame;
    void* mScalePixels;
    uint32_t mSurfaceWidth;
    uint32_t mSurfaceHeight;
    uint32_t mOptiSurfaceWidth;
    uint32_t mOptiSurfaceHeight;
    int mSwsFlags;
    int64_t mAveScaleTimeMs;
};

static int getNowSec() {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return (int)tv.tv_sec;
}

FFPlayer::FFPlayer()
{
	LOGD("FFPlayer constructor");
    mListener = NULL;
    mDurationMs = 0;
    mVideoFrameRate = 0;
    mPlayerFlags = 0;
    mVideoWidth = 0;
    mVideoHeight = 0;
    mVideoFormat = 0;
    mVideoTimeMs = 0;
    mLooping = false;
    mSeekTimeMs = 0;
    mSeeking = false;
    mRenderFirstFrame = true;
    mGetFirstFrame = true;
	mIsBuffering = false;
    mRunningCompatibilityTest = false;
    mPlayerStatus = MEDIA_PLAYER_IDLE;
	mAveVideoDecodeTimeMs = 0;
    mCompensateMs = 0;
	mMaxLatenessMs = 40;
	mMinLatenessMs = -10;
    mMaxMaxLatenessMs = 200;
    mLatenessMs = 0;
    mDiscardCount = 0;
    mDiscardLevel = AVDISCARD_NONE;
    mVideoPlayingTimeMs = 0;

    mUri = NULL;
    mDataStream = NULL;
    mAudioPlayer = NULL;
    mAudioStream = NULL;
    mSurface = NULL;
    mVideoFrame = NULL;
    mIsVideoFrameDirty = true;
    mVideoRenderer = NULL;
    mVideoStream = NULL;
	mMovieFile = NULL;
    mReachEndStream = false;
	mAudioStreamIndex = -1;
	mVideoStreamIndex = -1;

    mPrepareEvent = new FFEvent(this, &FFPlayer::onPrepare);
    mPrepareEventPending = false;
    //mStreamReadEvent = new FFEvent(this, &FFPlayer::onStreamRead);
	//mStreamReadEventPending = false;
    mVideoEvent = new FFEvent(this, &FFPlayer::onVideo);
    mVideoEventPending = false;
    mStreamDoneEvent = new FFEvent(this, &FFPlayer::onStreamDone);
    mStreamDoneEventPending = false;
    mBufferingEvent = new FFEvent(this, &FFPlayer::onBufferingUpdate);
    mBufferingEventPending = false;
    mSeekingEvent = new FFEvent(this, &FFPlayer::onSeeking);
    mSeekingEventPending = false;
    mCheckAudioStatusEvent = new FFEvent(this, &FFPlayer::onCheckAudioStatus);
    mAudioStatusEventPending = false;
    mBufferingStartEvent = new FFEvent(this, &FFPlayer::onBufferingStart);
    mBufferingStartEventPending = false;
    mBufferingEndEvent = new FFEvent(this, &FFPlayer::onBufferingEnd);
    mBufferingEndEventPending = false;
    mSeekingCompleteEvent = new FFEvent(this, &FFPlayer::onSeekingComplete);
    mSeekingCompleteEventPending = false;
    
    pthread_mutex_init(&mLock, NULL);
    pthread_cond_init(&mPreparedCondition, NULL);
	
    // register all codecs, demux and protocols
    avcodec_register_all();
    av_register_all();
    avformat_network_init();
	av_log_set_callback(setFFmpegLogCallback);
}

FFPlayer::~FFPlayer()
{
	LOGD("FFPlayer destructor");

    reset_l();

    if(mVideoEvent != NULL)
    {
        delete mVideoEvent;
        mVideoEvent = NULL;
    }
    //if(mStreamReadEvent != NULL)
    //{
    //    delete mStreamReadEvent;
    //    mStreamReadEvent = NULL;
    //}
    if(mStreamDoneEvent != NULL)
    {
        delete mStreamDoneEvent;
        mStreamDoneEvent = NULL;
    }
    if(mBufferingEvent != NULL)
    {
        delete mBufferingEvent;
        mBufferingEvent = NULL;
    }
    if(mSeekingEvent != NULL)
    {
        delete mSeekingEvent;
        mSeekingEvent = NULL;
    }
    if(mCheckAudioStatusEvent != NULL)
    {
        delete mCheckAudioStatusEvent;
        mCheckAudioStatusEvent = NULL;
    }
    if(mPrepareEvent != NULL)
    {
        delete mPrepareEvent;
        mPrepareEvent = NULL;
    }
    if(mBufferingStartEvent != NULL)
    {
        delete mBufferingStartEvent;
        mBufferingStartEvent = NULL;
    }
    if(mBufferingEndEvent != NULL)
    {
        delete mBufferingEndEvent;
        mBufferingEndEvent = NULL;
    }
    if(mSeekingCompleteEvent != NULL)
    {
        delete mSeekingCompleteEvent;
        mSeekingCompleteEvent = NULL;
    }
    
    pthread_mutex_destroy(&mLock);
    pthread_cond_destroy(&mPreparedCondition);
    avformat_network_deinit();
}

void FFPlayer::cancelPlayerEvents_l()
{
    LOGD("cancelPlayerEvents");
    mQueue.cancelEvent(mVideoEvent->eventID());
    mVideoEventPending = false;
    mQueue.cancelEvent(mCheckAudioStatusEvent->eventID());
    mAudioStatusEventPending = false;
}

status_t FFPlayer::setDataSource(const char *uri)
{
    if(uri == NULL ||
        (mPlayerStatus != MEDIA_PLAYER_IDLE &&
        mPlayerStatus != MEDIA_PLAYER_INITIALIZED))
    {
        return INVALID_OPERATION;
    }
    
    AutoLock autoLock(&mLock);
    
    int32_t size = strlen(uri);
    mUri = new char[size+1];
    memset(mUri, 0, size+1);
    strncpy(mUri, uri, size);
    mPlayerStatus = MEDIA_PLAYER_INITIALIZED;
    return OK;
}

status_t FFPlayer::setDataSource(int32_t fd, int64_t offset, int64_t length)
{
    return INVALID_OPERATION;
}

status_t FFPlayer::setVideoSurface(void* surface)
{
    if(mPlayerStatus != MEDIA_PLAYER_IDLE &&
        mPlayerStatus != MEDIA_PLAYER_INITIALIZED)
    {
        return INVALID_OPERATION;
    }
    
    mSurface = surface;
    return OK;
}

status_t FFPlayer::reset()
{
    if(mPlayerStatus == MEDIA_PLAYER_IDLE)
        return INVALID_OPERATION;
    
    AutoLock autoLock(&mLock);
    return reset_l();
}

status_t FFPlayer::prepare()
{
    LOGD("prepare");

    if(mPlayerStatus != MEDIA_PLAYER_INITIALIZED)
        return INVALID_OPERATION;
    
    AutoLock autoLock(&mLock);
    return prepare_l();
}

status_t FFPlayer::prepareAsync()
{
    LOGD("prepareAsync");

    if(mPlayerStatus != MEDIA_PLAYER_INITIALIZED)
        return INVALID_OPERATION;
    
    AutoLock autoLock(&mLock);
    return prepareAsync_l();
}

status_t FFPlayer::start()
{
    LOGD("start");

    if(mPlayerStatus != MEDIA_PLAYER_PREPARED &&
        mPlayerStatus != MEDIA_PLAYER_PAUSED)
        return INVALID_OPERATION;
    
    AutoLock autoLock(&mLock);
    return play_l();
}

status_t FFPlayer::stop()
{
    LOGD("stop");
    
    if(mPlayerStatus == MEDIA_PLAYER_IDLE ||
        mPlayerStatus == MEDIA_PLAYER_INITIALIZED)
        return INVALID_OPERATION;
    
    AutoLock autoLock(&mLock);
    return stop_l();
}

status_t FFPlayer::pause()
{
    if(mPlayerStatus != MEDIA_PLAYER_STARTED )
        return INVALID_OPERATION;

    AutoLock autoLock(&mLock);
    return pause_l();
}

status_t FFPlayer::seekTo(int32_t msec)
{
    if(mPlayerStatus != MEDIA_PLAYER_PREPARED &&
        mPlayerStatus != MEDIA_PLAYER_STARTED &&
        mPlayerStatus != MEDIA_PLAYER_PAUSED)
        return INVALID_OPERATION;
    
    if (mPlayerFlags & (CAN_SEEK_FORWARD |CAN_SEEK_BACKWARD))
    {
        LOGD("seekto:%d ms", msec);
        mSeekTimeMs = msec;
        postSeekingEvent_l();
    }
    return OK;
}

bool FFPlayer::isPlaying()
{
    return (mPlayerStatus == MEDIA_PLAYER_STARTED);
}

status_t FFPlayer::getVideoWidth(int32_t *w)
{
    if(mVideoRenderer != NULL)
    {
        //Use optimized size from render
        uint32_t optWidth = 0;
        mVideoRenderer->width(optWidth);
        *w = optWidth;
    }
    else
    {
        *w = mVideoWidth;
    }
    return OK;
}

status_t FFPlayer::getVideoHeight(int32_t *h)
{
    if(mVideoRenderer != NULL)
    {
        //Use optimized size from render
        uint32_t optHeight = 0;
        mVideoRenderer->height(optHeight);
        *h = optHeight;
    }
    else
    {
        *h = mVideoHeight;
    }
    return OK;
}

status_t FFPlayer::getCurrentPosition(int32_t* positionMs)
{
    if(mSeekingEventPending ||mSeeking )
    {
        *positionMs = (int32_t)mSeekTimeMs;
    }
    else
    {
        if(mVideoStream != NULL)
        {
            //get video time
            *positionMs = (int32_t)mVideoTimeMs;
        }
        else if(mAudioPlayer != NULL)
        {
            //get audio time
            *positionMs = (int32_t)mAudioPlayer->getMediaTimeMs();
        }
        else
        {
            LOGE("No available time reference");
        }
    }
    return OK;
}

status_t FFPlayer::getDuration(int32_t *durationUs)
{
    *durationUs = mDurationMs;
    return OK;
}

status_t FFPlayer::setAudioStreamType(int32_t type)
{
    return OK;
}

status_t FFPlayer::setLooping(int32_t shouldLoop)
{
    mLooping = (shouldLoop!=0);
    return OK;
}

bool FFPlayer::isLooping()
{
    return mLooping;
}

status_t FFPlayer::setVolume(float leftVolume, float rightVolume)
{
    return OK;
}

status_t FFPlayer::setListener(MediaPlayerListener* listener)
{
    mListener = listener;
    return OK;
}

status_t FFPlayer::flags()
{
    return mPlayerFlags;
}

status_t FFPlayer::suspend()
{
	return INVALID_OPERATION;
}

status_t FFPlayer::resume()
{
    return INVALID_OPERATION;
}

//////////////////////////////////////////////////////////////////
void FFPlayer::onPrepare()
{
	LOGD("RUN FFPlayer::onPrepareAsyncEvent()");
    
    AutoLock autoLock(&mLock);
    
    if (!mPrepareEventPending)
        return;
    mPrepareEventPending = false;

	// Open stream
	mDataStream = new FFStream();
    mDataStream->setListener(this);
    mMovieFile = mDataStream->open(mUri);
    if(mMovieFile == NULL)
    {
     	LOGE("open stream :%s failed", mUri);
        abortPrepare_l(ERROR);
        return;
    }
    
    mDurationMs = mDataStream->getDurationMs();
    LOGD("mDurationMs:%lld", mDurationMs);
    if(mDurationMs > 0)
    {
        //vod
        //update mediasource capability
        mPlayerFlags = CAN_SEEK_BACKWARD |
                        CAN_SEEK_FORWARD |
                        CAN_PAUSE;
    }

    if(prepareAudio_l() != OK)
    {
     	LOGE("Initing audio decoder failed");
        abortPrepare_l(ERROR);
        return;
    }

    if(prepareVideo_l() != OK)
    {
     	LOGE("Initing video decoder failed");
        abortPrepare_l(ERROR);
        return;
    }

    //mPrepareResult = OK;
    //mPlayerStatus = MEDIA_PLAYER_PREPARED;
    //pthread_cond_broadcast(&mPreparedCondition);
    //notifyListener_l(MEDIA_PREPARED);

    mDataStream->start();
    if(mDataStream->getURLType() != FFStream::TYPE_LOCAL_FILE)
    {
        postBufferingUpdateEvent_l();
    }
}

void FFPlayer::onVideo()
{
    LOGD("on video event");

    AutoLock autoLock(&mLock);
    
    if (!mVideoEventPending)
        return;
    mVideoEventPending = false;

    if(mIsVideoFrameDirty)
    {
        AVPacket* pPacket = NULL;
        status_t ret = mDataStream->getPacket(mVideoStreamIndex, &pPacket);
        if(ret == FFSTREAM_OK)
        {
            if(mGetFirstFrame)
            {
                if(!pPacket->flags & AV_PKT_FLAG_KEY)
                {
                    LOGD("Discard non-key packet for first packet");
                    //free packet resource
        	        av_free_packet(pPacket);
                    av_free(pPacket);
                    pPacket = NULL;
    	            postVideoEvent_l(0); //trigger next onvideo event asap.
                    return;
                }
                mGetFirstFrame = false;
            }
            else
            {
                if(!mIsBuffering)
                {
                    if(isPacketLate_l(pPacket))
                    {
                        //free packet resource
            	        av_free_packet(pPacket);
                        av_free(pPacket);
                        pPacket = NULL;
        	            postVideoEvent_l(0); //trigger next onvideo event asap.
                        return;
                    }

                    optimizeDecode_l(pPacket);
                }
            }
            
            //decoding
            LOGD("decode before");
            status_t ret = decode_l(pPacket);
            LOGD("decode after");

            //free packet resource
	        av_free_packet(pPacket);
            av_free(pPacket);
            pPacket = NULL;

            if(ret != OK)
            {
	            postVideoEvent_l();
                return;
            }
        }
        else if(ret == FFSTREAM_ERROR_FLUSHING)
        {
	        LOGD("FFSTREAM_ERROR_FLUSHING");
            avcodec_flush_buffers(mVideoStream->codec);
            av_free(pPacket);
            pPacket = NULL;
	        postVideoEvent_l();
            return;
        }
        else if(ret == FFSTREAM_ERROR_BUFFERING)
		{
	        LOGD("video queue no data");
	        postVideoEvent_l();
            return;
		}
		else if(ret == FFSTREAM_ERROR_EOF)
		{
		    mReachEndStream = true;
		    LOGD("reach video stream end");
            if(mAudioStream == NULL)
            {
                postStreamDoneEvent_l();
            }
            else
            {
                //do not post video event, wait audio player finishing playing.
            }
            return;
        }
        else
        {
            LOGE("read video packet error:%d", ret);
            return;
        }
    }

    if(mPlayerStatus == MEDIA_PLAYER_PAUSED)
    {
        //support frame update during paused.
        LOGV("render frame begin");	    
        mVideoRenderer->render(mVideoFrame);
        mIsVideoFrameDirty = true;
        LOGV("on video event end: render frame end");
        //mRenderFrameDuringPaused = false;
        return;
    }

    if(mRenderFirstFrame)
    {
        LOGV("render frame begin:mRenderFirstFrame");	    
        mVideoRenderer->render(mVideoFrame);
        mIsVideoFrameDirty = true;
        LOGV("on video event end: render frame end");
        mRenderFirstFrame = false;

        //update audioplayer initial media time
        //video outputs before audio
    	if (mAudioPlayer != NULL)
        {
            int64_t pts = getFramePTS_l(mVideoFrame);
            int64_t frameTimeMs = (int64_t)(pts*1000*av_q2d(mVideoStream->time_base));
            mAudioPlayer->setMediaTimeMs(frameTimeMs);
    	}
    }

    if(mIsBuffering)
    {
        postVideoEvent_l();
        return;
    }
    
    int64_t videoFramePTS = getFramePTS_l(mVideoFrame);    
    mVideoPlayingTimeMs = (int64_t)(videoFramePTS*1000*av_q2d(mVideoStream->time_base));
    LOGD("video frame pts:%lld", mVideoPlayingTimeMs);
    //if(mDurationMs == 0)
    //{
    //    //broadcast
    //    if(mLastFrameMs-frameMs > 500) //0.5s
    //    {
    //        //m3u8
    //        mPlayedDurationMs = mVideoPlayingTimeMs;
    //        LOGD("Video mPlayedDurationMs:%lld", mPlayedDurationMs);
    //    }
    //    mLastFrameMs = frameMs;
    //}
    //mVideoPlayingTimeMs = mPlayedDurationMs+frameMs;
    
    int64_t audioPlayingTimeMs = mAudioPlayer->getMediaTimeMs();
    mLatenessMs = audioPlayingTimeMs - mVideoPlayingTimeMs + mVideoRenderer->swsMs();
	
    LOGD("videoPlayingTimeMs: %lld, audioPlayingTimeMs: %lld, mLatenessMs: %lld",
		    mVideoPlayingTimeMs, audioPlayingTimeMs, mLatenessMs);
    
    mVideoTimeMs = audioPlayingTimeMs;

    if (mLatenessMs > mMaxLatenessMs && mLatenessMs < mMaxMaxLatenessMs)
    {
        if(mLatenessMs < 80)
        {
            mDiscardLevel = AVDISCARD_NONREF;
            mDiscardCount = 2;
        }
        else if(mLatenessMs < 120)
        {
            mDiscardLevel = AVDISCARD_NONREF;
            mDiscardCount = 3;
            if(mVideoFrameRate >30)
            {
                mDiscardCount =4; 
            }
        }
        else if(mLatenessMs < 160)
        {
            mDiscardLevel = AVDISCARD_BIDIR;
            mDiscardCount = 4;
            if(mVideoFrameRate >30)
            {
                mDiscardCount = 5; 
            }
        }
        else if(mLatenessMs < 200)
        {
            mDiscardLevel = AVDISCARD_BIDIR;
            mDiscardCount = 5;
            if(mVideoFrameRate >30)
            {
                mDiscardCount = 6; 
            }
        }
        
        mIsVideoFrameDirty = true;
        postVideoEvent_l(0);
            
        // We're more than 40ms(default) late.
        LOGD("we're late by %lld ms, mDiscardLevel:%d, mDiscardCount:%d", mLatenessMs, mDiscardLevel, mDiscardCount);
        return;
    }

    if (mLatenessMs < mMinLatenessMs)
	{
        // We're more than 10ms early.
        LOGV("we're early by %lld ms", mLatenessMs);

        //still drop frame if it is too early for error handling.
        if(mLatenessMs < -60000)//60s
        {
            LOGE("incorrect pts in frame, drop it");
            mIsVideoFrameDirty = true;
            postVideoEvent_l(0);
            return;
        }

        postVideoEvent_l();
        return;
    }

    LOGV("we're render frame begin");	    
    mVideoRenderer->render(mVideoFrame);
    mIsVideoFrameDirty = true;
    if(mLatenessMs >= mMaxMaxLatenessMs)
    {
        mDiscardLevel = AVDISCARD_BIDIR;
        mDiscardCount = 6;
        if(mVideoFrameRate >30)
        {
            mDiscardCount = 7; 
        }
    }
    else
    {
        mDiscardLevel = AVDISCARD_NONE;
        mDiscardCount = 0;
    }
    LOGV("we're on video event end: render frame end");

	postVideoEvent_l();
}

void FFPlayer::onBufferingUpdate()
{    
    AutoLock autoLock(&mLock);
    if (!mBufferingEventPending) {
        return;
    }
    mBufferingEventPending = false;

    if(mDataStream == NULL || mDurationMs < 0)
    {
        LOGE("Invalid buffering status");
        //postBufferingUpdateEvent_l();
        return;
    }

    if(mDurationMs == 0)
    {
        LOGD("It is broadcasting, do not update buffering status");
        return;
    }
    
    int64_t cachedDurationMs = mDataStream->getCachedDurationMs();
	int percent100 = (int)(cachedDurationMs*100/mDurationMs)+1; // 1 is for compensation.
	if(percent100>100) percent100 = 100;
	LOGD("onBufferingUpdate, percent: %d", percent100);
	
    notifyListener_l(MEDIA_BUFFERING_UPDATE, percent100);

    postBufferingUpdateEvent_l();
}

void FFPlayer::onSeeking()
{
	LOGD("onSeeking");
    
    AutoLock autoLock(&mLock);
    
    if (!mSeekingEventPending)
        return;
    
    if(mVideoStream != NULL)
    {
        mVideoTimeMs = mSeekTimeMs;
    }
    mSeekingEventPending = false;
    
    if(!mSeeking)
    {
        mSeeking = true; //doing seek.
        seekTo_l();
        if(mVideoStream != NULL)
        {
            mRenderFirstFrame = true;
            mGetFirstFrame = true;
        }
    }
    else
    {
        postSeekingEvent_l(100);
    }
}

void FFPlayer::onStreamDone()
{
    LOGD("onStreamDone");

    AutoLock autoLock(&mLock);
    if (!mStreamDoneEventPending) {
        return;
    }
    mStreamDoneEventPending = false;

    if (mLooping)
    {
        //TODO:
        LOGE("Loop is not supported");
    }
    else
    {
        //Current thread should not stop itself.
        //stop_l();
        notifyListener_l(MEDIA_PLAYBACK_COMPLETE);
    }
}

void FFPlayer::onCheckAudioStatus()
{
    AutoLock autoLock(&mLock);
    
    if (!mAudioStatusEventPending)
        return;
    mAudioStatusEventPending = false;

    if (mAudioPlayer->getStatus() == MEDIA_PLAYER_PLAYBACK_COMPLETE)
    {
        LOGD("Audio reaches stream end");
        postStreamDoneEvent_l();
    }
    else
    {
        postCheckAudioStatusEvent_l();
    }
}

void FFPlayer::onBufferingStart()
{
	LOGD("onBufferingStart");
    
    AutoLock autoLock(&mLock);
    
    if (!mBufferingStartEventPending)
        return;
    mBufferingStartEventPending = false;

    mIsBuffering = true;
    if(mPlayerStatus == MEDIA_PLAYER_PREPARING)
    {
        //Do nothing
    }
    else
    {
        if(mDataStream->getURLType() != FFStream::TYPE_LOCAL_FILE)
        {
            LOGD("MEDIA_INFO_BUFFERING_START");
            notifyListener_l(MEDIA_INFO, MEDIA_INFO_BUFFERING_START);
        }
    }
}

void FFPlayer::onBufferingEnd()
{
	LOGD("onBufferingEnd");
    
    AutoLock autoLock(&mLock);
    
    if (!mBufferingEndEventPending)
        return;
    mBufferingEndEventPending = false;

    mIsBuffering = false;
    if(mPlayerStatus == MEDIA_PLAYER_PREPARING)
    {
        mPrepareResult = OK;
        mPlayerStatus = MEDIA_PLAYER_PREPARED;
        pthread_cond_broadcast(&mPreparedCondition);
        LOGD("MEDIA_PREPARED");
        notifyListener_l(MEDIA_PREPARED);
    }
    else
    {
        if(mDataStream->getURLType() != FFStream::TYPE_LOCAL_FILE)
        {
            LOGD("MEDIA_INFO_BUFFERING_END");
            notifyListener_l(MEDIA_INFO, MEDIA_INFO_BUFFERING_END);
        }
    }
}

void FFPlayer::onSeekingComplete()
{
	LOGD("onSeekingComplete");
    
    AutoLock autoLock(&mLock);
    
    if (!mSeekingCompleteEventPending)
        return;
    mSeekingCompleteEventPending = false;

    mSeeking = false;
    LOGD("send event MEDIA_SEEK_COMPLETE");
    notifyListener_l(MEDIA_SEEK_COMPLETE);
}

void FFPlayer::notify(int32_t msg, int32_t ext1, int32_t ext2)
{
    switch(msg)
    {
        case MEDIA_INFO:
        {
            if(ext1 == MEDIA_INFO_BUFFERING_START)
            {
                postBufferingStartEvent_l();
            }
            else if(ext1 == MEDIA_INFO_BUFFERING_END)
            {
                postBufferingEndEvent_l();
            }
            break;
        }
        case MEDIA_SEEK_COMPLETE:
            postSeekingCompleteEvent_l();
            break;
        default:
            break;
    }
}

////////////////////////////////////////////////////////////////
status_t FFPlayer::pause_l()
{
    cancelPlayerEvents_l();

    if (mAudioPlayer != NULL)
    {
        status_t ret = mAudioPlayer->pause();
        if(ret != OK)
        {
     	    LOGE("pause audio player failed");
            return ret;
        }
    }

    mPlayerStatus = MEDIA_PLAYER_PAUSED;
    return OK;
}

status_t FFPlayer::seekTo_l()
{
    if(mAudioPlayer != NULL)
    {
        mAudioPlayer->seekTo(mSeekTimeMs);
    }
    
    if(mDataStream->seek(mSeekTimeMs) != OK)
    {
        notifyListener_l(MEDIA_ERROR, MEDIA_ERROR_UNKNOWN, 0);
        mPlayerStatus = MEDIA_PLAYER_STATE_ERROR;
        return ERROR;
    }

    if (mPlayerStatus == MEDIA_PLAYER_PAUSED)
    {
        LOGD("seeking while paused");
    
        postVideoEvent_l();//to display seek target frame
        //mRenderFrameDuringPaused = true;
    }
    
    mDiscardLevel = AVDISCARD_NONE;
    mDiscardCount = 0;
    mIsVideoFrameDirty = true;
    return OK;
}

void FFPlayer::postPrepareEvent_l()
{
    if (mPrepareEventPending) {
        return;
    }
    mPrepareEventPending = true;
    mQueue.postEvent(mPrepareEvent);
}

void FFPlayer::postVideoEvent_l(int64_t delayMs)
{
    if (mVideoEventPending) {
        return;
    }
    mVideoEventPending = true;
    mQueue.postEventWithDelay(mVideoEvent, delayMs < 0 ? 10 : delayMs);
}

void FFPlayer::postStreamDoneEvent_l()
{
    if (mStreamDoneEventPending) {
        return;
    }
    mStreamDoneEventPending = true;
    mQueue.postEvent(mStreamDoneEvent);
}

void FFPlayer::postBufferingUpdateEvent_l()
{
    if (mBufferingEventPending) 
    {
        return;
    }
    mBufferingEventPending = true;
    mQueue.postEventWithDelay(mBufferingEvent, 1000); //1sec
}

void FFPlayer::postSeekingEvent_l(int64_t delayMs)
{

    if (mSeekingEventPending) {
        return;
    }
    mSeekingEventPending = true;
    if(delayMs == -1)
    {
        mQueue.postEvent(mSeekingEvent);
    }
    else
    {
        mQueue.postEventWithDelay(mSeekingEvent, delayMs);
    }
}


void FFPlayer::postCheckAudioStatusEvent_l() {
    if (mAudioStatusEventPending) {
        return;
    }
    mAudioStatusEventPending = true;
    mQueue.postEventWithDelay(mCheckAudioStatusEvent, 1000);//1sec
}

void FFPlayer::postBufferingStartEvent_l() {

    if (mBufferingStartEventPending) {
        return;
    }
    mBufferingStartEventPending = true;
    mQueue.postEvent(mBufferingStartEvent);
}

void FFPlayer::postBufferingEndEvent_l() {

    if (mBufferingEndEventPending) {
        return;
    }
    mBufferingEndEventPending = true;
    mQueue.postEvent(mBufferingEndEvent);
}

void FFPlayer::postSeekingCompleteEvent_l() {

    if (mSeekingCompleteEventPending) {
        return;
    }
    mSeekingCompleteEventPending = true;
    mQueue.postEvent(mSeekingCompleteEvent);
}

void FFPlayer::abortPrepare_l(status_t err)
{
    notifyListener_l(MEDIA_ERROR, MEDIA_ERROR_UNKNOWN, err);

    mPrepareResult = err;
    mPlayerStatus = MEDIA_PLAYER_STATE_ERROR;
    pthread_cond_broadcast(&mPreparedCondition);
}

status_t FFPlayer::prepareAudio_l()
{
	mAudioStreamIndex = mDataStream->getAudioStreamIndex();
	mAudioStream = mDataStream->getAudioStream();
	if (mAudioStreamIndex == -1 || mAudioStream == NULL)
    {
        LOGD("No audio stream");
        
        //init audioplayer for time reference
        mAudioPlayer = new AudioPlayer();
        mAudioPlayer->setListener(this);
        status_t ret = mAudioPlayer->prepare();
        if(ret != OK)
        {
         	LOGE("prepare audio player failed");
            return ret;
        }
	}
    else
    {
    	LOGD("Audio time_base: %d/%d ", mAudioStream->time_base.num, mAudioStream->time_base.den);
    	// Get a pointer to the codec context for the video stream
    	AVCodecContext* codec_ctx = mAudioStream->codec;
    	AVCodec* codec = avcodec_find_decoder(codec_ctx->codec_id);
    	if (codec == NULL)
        {
            LOGE("Failed to find decoder:%d", codec_ctx->codec_id);
    		return ERROR;
    	}
        
    	// Open codec
    	codec_ctx->request_sample_fmt = AV_SAMPLE_FMT_S16;
    	codec_ctx->request_channel_layout = AV_SAMPLE_FMT_S16;
    	if (avcodec_open(codec_ctx, codec) < 0)
        {
            LOGE("Failed to open decoder");
    		return ERROR;
    	}

        //init audioplayer
        mAudioPlayer = new AudioPlayer(mDataStream, mAudioStream, mAudioStreamIndex);
        mAudioPlayer->setListener(this);
        status_t ret = mAudioPlayer->prepare();
        if(ret != OK)
        {
         	LOGE("prepare audio player failed");
            return ret;
        }
    }
	return OK;
}

status_t FFPlayer::prepareVideo_l()
{
	mVideoStreamIndex = mDataStream->getVideoStreamIndex();
	mVideoStream = mDataStream->getVideoStream();
	if (mVideoStreamIndex == -1 || mVideoStream == NULL)
    {
        LOGD("No video stream");
        notifyListener_l(MEDIA_SET_VIDEO_SIZE, 0, 0);
		return OK;
	}
    else
    {
    	LOGD("Video time_base: %d/%d ", mVideoStream->time_base.num, mVideoStream->time_base.den);
    	// Get a pointer to the codec context for the video stream
    	AVCodecContext* codec_ctx = mVideoStream->codec;
    	AVCodec* codec = avcodec_find_decoder(codec_ctx->codec_id);
    	if (codec == NULL)
        {
    		return ERROR;
    	}
    	
    	// Open codec
    	if (avcodec_open(codec_ctx, codec) < 0)
        {
    		return ERROR;
    	}
    	
    	mVideoWidth = codec_ctx->width;
    	mVideoHeight = codec_ctx->height;
        mVideoFormat = (int32_t)codec_ctx->pix_fmt;
        if(mVideoStream->avg_frame_rate.den > 0)
        {
            mVideoFrameRate = mVideoStream->avg_frame_rate.num/mVideoStream->avg_frame_rate.den;
        }
        else
        {
            mVideoFrameRate = 25;//give a guessed value.
        }
        LOGD("mVideoWidth: %d", mVideoWidth);
        LOGD("mVideoHeight: %d", mVideoHeight);
        LOGD("mVideoFormat: %d", mVideoFormat);
        LOGD("mVideoFrameRate:%d", mVideoFrameRate);
        
        //init videoplayer related data.
        mVideoFrame = avcodec_alloc_frame();
        mIsVideoFrameDirty = true;
        mVideoRenderer = new FFRender(mSurface, mVideoWidth, mVideoHeight, mVideoFormat);
        if(mVideoRenderer->init() != OK)
        {
         	LOGE("Initing video render failed");
            return ERROR;
        }
    
        uint32_t surfaceWidth = 0, surfaceHeight = 0;
        mVideoRenderer->width(surfaceWidth);
        mVideoRenderer->height(surfaceHeight);
        notifyListener_l(MEDIA_SET_VIDEO_SIZE, surfaceWidth, surfaceHeight);
    }

	return OK;
}

void FFPlayer::notifyListener_l(int msg, int ext1, int ext2)
{
    if (mListener != NULL)
    {
        mListener->notify(msg, ext1, ext2);
    }
    else
    {
	     LOGE("mListener is null");
	}
}

status_t FFPlayer::play_l()
{
    //if(mPlayerStatus == MEDIA_PLAYER_PREPARED)
    //{
    //    mDataStream->start();
    //}
    
    if (mAudioPlayer != NULL)
    {
        if(mAudioPlayer->start() != OK)
        {
            LOGE("audio player starts failed");
            return ERROR;
        }
        
        if(mAudioStream != NULL)
        {
            postCheckAudioStatusEvent_l();
        }
        else
        {
            LOGD("no audio");
        }
    }

    if (mVideoRenderer != NULL &&
        mVideoStream != NULL)
    {
        // Kick off video playback event
        postVideoEvent_l();
    }
    else
    {
        LOGD("no video");
    }

    mPlayerStatus = MEDIA_PLAYER_STARTED;
    return OK;
}

status_t FFPlayer::reset_l()
{
    if(stop_l() != OK)
    {
        return ERROR;
    }
    
    if(mUri != NULL)
    {
        delete mUri;
        mUri = NULL;
    }

    mSurface = NULL;
    mPlayerStatus = MEDIA_PLAYER_IDLE;
    return OK;
}

status_t FFPlayer::stop_l()
{    
    mQueue.stop();
	
    // Shutdown audio first
    if(mAudioPlayer != NULL)
    {
        delete mAudioPlayer;
        mAudioPlayer = NULL;
    }
    if(mAudioStream != NULL)
    {
        LOGD("avcodec_close audio");
        avcodec_close(mAudioStream->codec);
        mAudioStream = NULL;
    }
      
    if(mVideoFrame != NULL)
    {
        avcodec_free_frame(&mVideoFrame);
        mVideoFrame = NULL;
    }
    if(mVideoRenderer != NULL)
    {
        delete mVideoRenderer;
        mVideoRenderer = NULL;
    }
    if(mVideoStream != NULL)
    {
        LOGD("avcodec_close video");
        avcodec_close(mVideoStream->codec);
        mVideoStream = NULL;
    }
    
	if(mDataStream != NULL)
    {
        // Close stream
        LOGD("stop stream");
        mDataStream->stop();
        delete mDataStream;
        mDataStream = NULL;
    }
    
    mReachEndStream = false;
	mAudioStreamIndex = -1;
	mVideoStreamIndex = -1;
        
    mDurationMs = 0;
    mVideoFrameRate = 0;
    mPlayerStatus = 0;
    mPlayerFlags = 0;
    mVideoWidth = 0;
    mVideoHeight = 0;
    mVideoFormat = 0;
    mVideoTimeMs = 0;
    mLooping = false;
    mSeekTimeMs = 0;
    //mRenderFrameDuringPaused = false;
    
    //the STOPPED status never be used. use INITIALIZED instead.
	//mPlayerStatus = MEDIA_PLAYER_STOPPED;
    mPlayerStatus = MEDIA_PLAYER_INITIALIZED;

    LOGD("Finish stop_l");
    return OK;
}

status_t FFPlayer::prepare_l()
{
    status_t err = prepareAsync_l();

    if (err != OK) {
        return err;
    }

    while (mPlayerStatus == MEDIA_PLAYER_PREPARING)
    {
        pthread_cond_wait(&mPreparedCondition, &mLock);
    }
    return mPrepareResult;
}

status_t FFPlayer::prepareAsync_l()
{    
    if (mPlayerStatus == MEDIA_PLAYER_PREPARING) {
        return ERROR; 
    }
	
    if (mPlayerStatus == MEDIA_PLAYER_PREPARED) {
        return OK;
    }

    mQueue.start();
    postPrepareEvent_l();
    
    mPlayerStatus = MEDIA_PLAYER_PREPARING;
    return OK;
}


status_t FFPlayer::decode_l(AVPacket *packet)
{
    int32_t gotPicture = 0;
	
	// Decode video frame
	int64_t begin_decode = getNowMs();
	int len = avcodec_decode_video2(mVideoStream->codec,
        						 mVideoFrame,
        						 &gotPicture,
        						 packet);
	int64_t end_decode = getNowMs();
    int64_t costTime = end_decode-begin_decode;
    if(mAveVideoDecodeTimeMs == 0)
    {
        mAveVideoDecodeTimeMs = costTime;
    }
    else
    {
        mAveVideoDecodeTimeMs = (mAveVideoDecodeTimeMs*4+costTime)/5;
    }
	LOGD("decode video cost %lld[ms]", costTime);
	LOGD("mAveVideoDecodeTimeMs %lld[ms]", mAveVideoDecodeTimeMs);

    #if (LOG_NDEBUG==0)
    int64_t pts = (int64_t)(packet->pts*1000*av_q2d(mVideoStream->time_base));
    int64_t dts = (int64_t)(packet->dts*1000*av_q2d(mVideoStream->time_base));
	LOGD("decoding packet->pts:%lld ms", pts);
	LOGD("decoding packet->dts:%lld ms", dts);
    int64_t frame_pts = (int64_t)(mVideoFrame->pts*1000*av_q2d(mVideoStream->time_base));
	LOGD("mVideoFrame->pts:%lld", frame_pts);
    int64_t frame_pkt_pts = (int64_t)(mVideoFrame->pkt_pts*1000*av_q2d(mVideoStream->time_base));
    int64_t frame_pkt_dts = (int64_t)(mVideoFrame->pkt_dts*1000*av_q2d(mVideoStream->time_base));
	LOGD("mVideoFrame->pkt_pts:%lld", frame_pkt_pts);
	LOGD("mVideoFrame->pkt_dts:%lld", frame_pkt_dts);
    #endif

	if (len >= 0)
    {
		if(gotPicture!=0)
		{
		    LOGD("got a decoded frame");
            mIsVideoFrameDirty = false;
		    return OK;
		}
        else
        {
            LOGD("Frame is not available for output"); //like B frame
        }
	}
    else
    {
	    LOGE("Failed to decode video frame with ret:%d", len);    
    }
	return ERROR;
    
	/*
	if (packet->dts == AV_NOPTS_VALUE && mFrame->opaque
			&& *(uint64_t*) mFrame->opaque != AV_NOPTS_VALUE) {
		pts = *(uint64_t *) mFrame->opaque;
	} else if (packet->dts != AV_NOPTS_VALUE) {
		pts = packet->dts;
	} else {
		pts = 0;
	}
	pts *= av_q2d(mStream->time_base);
	LOGV("Video pts:%d", pts);
	*/
}

static void decodeNAL(uint8_t* dst, uint8_t* src, int32_t size )
{
    uint8_t* end = src + size;
    while(src < end)
    {
        if(src < end-3 &&
            src[0] == 0x00 &&
            src[1] == 0x00 &&
            src[2] == 0x03 )
        {
            *dst++ = 0x00;
            *dst++ = 0x00;

            src += 3;
            continue;
        }
        *dst++ = *src++;
    }
}

//defines functions, structures for handling streams of bits
typedef struct bs_s
{
    uint8_t* p_start;
    uint8_t* p;
    uint8_t* p_end;

    int32_t i_left; /* i_count number of available bits */
} bs_t;

static inline void bs_init( bs_t* s, void* pData, int32_t iData )
{
    s->p_start = (uint8_t*)pData;
    s->p       = s->p_start;
    s->p_end   = s->p_start + iData;
    s->i_left  = 8;
}

static inline uint32_t bs_read( bs_t *s, int i_count )
{
     static const uint32_t i_mask[33] =
     {  0x00,
        0x01,      0x03,      0x07,      0x0f,
        0x1f,      0x3f,      0x7f,      0xff,
        0x1ff,     0x3ff,     0x7ff,     0xfff,
        0x1fff,    0x3fff,    0x7fff,    0xffff,
        0x1ffff,   0x3ffff,   0x7ffff,   0xfffff,
        0x1fffff,  0x3fffff,  0x7fffff,  0xffffff,
        0x1ffffff, 0x3ffffff, 0x7ffffff, 0xfffffff,
        0x1fffffff,0x3fffffff,0x7fffffff,0xffffffff};
    int      i_shr;
    uint32_t i_result = 0;

    while( i_count > 0 )
    {
        if( s->p >= s->p_end )
        {
            break;
        }

        if( ( i_shr = s->i_left - i_count ) >= 0 )
        {
            /* more in the buffer than requested */
            i_result |= ( *s->p >> i_shr )&i_mask[i_count];
            s->i_left -= i_count;
            if( s->i_left == 0 )
            {
                s->p++;
                s->i_left = 8;
            }
            return( i_result );
        }
        else
        {
            /* less in the buffer than requested */
           i_result |= (*s->p&i_mask[s->i_left]) << -i_shr;
           i_count  -= s->i_left;
           s->p++;
           s->i_left = 8;
        }
    }

    return( i_result );
}

static inline uint32_t bs_read1( bs_t *s )
{
    if( s->p < s->p_end )
    {
        unsigned int i_result;

        s->i_left--;
        i_result = ( *s->p >> s->i_left )&0x01;
        if( s->i_left == 0 )
        {
            s->p++;
            s->i_left = 8;
        }
        return i_result;
    }

    return 0;
}

static inline int bs_read_ue( bs_t *s )
{
    int i = 0;

    while( bs_read1( s ) == 0 && s->p < s->p_end && i < 32 )
    {
        i++;
    }
    return( ( 1 << i) - 1 + bs_read( s, i ) );
}

static inline int bs_read_se( bs_t *s )
{
    int val = bs_read_ue( s );

    return val&0x01 ? (val+1)/2 : -(val/2);
}

// NAL unit types
enum {
    NAL_SLICE = 1,
    NAL_DPA,
    NAL_DPB,
    NAL_DPC,
    NAL_IDR_SLICE,
    NAL_SEI,
    NAL_SPS,
    NAL_PPS,
    NAL_AUD,
    NAL_END_SEQUENCE,
    NAL_END_STREAM,
    NAL_FILLER_DATA,
    NAL_SPS_EXT,
    NAL_AUXILIARY_SLICE = 19
};

bool FFPlayer::isPacketLate_l(AVPacket* packet)
{
    if(mAudioPlayer == NULL)
        return false;
    
    if(packet->size <= 0)
    {
        return true;
    }
    
    if(mVideoStream->codec->codec_id == AV_CODEC_ID_H264)
    {
        #if (LOG_NDEBUG==0)
        int64_t pts = (int64_t)(packet->pts*1000*av_q2d(mVideoStream->time_base));
        int64_t dts = (int64_t)(packet->dts*1000*av_q2d(mVideoStream->time_base));
    	LOGD("decoding packet->pts:%lld ms", pts);
    	LOGD("decoding packet->dts:%lld ms", dts);
        #endif
        LOGD("AV_CODEC_ID_H264, size:%d", packet->size);
        LOGD("packet->pts:%lld", packet->pts);
        LOGD("packet->dts:%lld", packet->dts);
        LOGD("packet->flags:%d", packet->flags);
        
        uint8_t* pNAL = NULL;
        int32_t sizeNAL = 0;
        LOGD("mMovieFile->iformat->name:%s", mMovieFile->iformat->name);
        if(strstr(mMovieFile->iformat->name, "matroska") != NULL ||
            strstr(mMovieFile->iformat->name, "mp4") != NULL ||
            strstr(mMovieFile->iformat->name, "flv") != NULL)
        {
            uint8_t* pExtra = mVideoStream->codec->extradata;
            if(pExtra[0] != 1 || mVideoStream->codec->extradata_size < 7)
            {
                LOGE("Invalid AVCC");
                return false;
            }
            int32_t avccLengthSize = (pExtra[4] & 0x03) + 1;
            LOGD("avccLengthSize:%d", avccLengthSize);
            pNAL = packet->data+avccLengthSize;
            sizeNAL = packet->size-avccLengthSize;
        }
        else
        {
            //other types input stream
            for(int32_t offset=0; offset < packet->size-3; offset++ )
            {
                if(packet->data[offset] == 0x00 &&
                    packet->data[offset+1] == 0x00 &&
                    packet->data[offset+2] == 0x01)
                {
                    //match it
                    LOGD("got 0x000001");
                    pNAL = packet->data+offset+3;
                    sizeNAL = packet->size-offset-3;
                    break;
                }
            }
        }
        
        int32_t nalForb = pNAL[0]>>7;
        if(nalForb == 1)
        {
            LOGE("Packet is corrupted");
            return true;
        }
        
        int32_t nalType = pNAL[0]&0x1f;
        int32_t nalReferrenceIDC = (pNAL[0]>>5)&0x3;
        LOGD("nalType:%d", nalType);
        LOGD("nalReferrenceIDC:%d", nalReferrenceIDC);
        if(nalType >= NAL_SLICE && nalType <= NAL_IDR_SLICE)
        {
            if(mDiscardLevel == AVDISCARD_NONREF && mDiscardCount > 0)
            {
                if(nalReferrenceIDC == 0)
                {
                    mDiscardCount--;
                    LOGD("Discard packet as video playing under AVDISCARD_NONREF, mDiscardCount:%d,", mDiscardCount);
                    return true;
                }
            }
            else if(mDiscardLevel == AVDISCARD_BIDIR && mDiscardCount > 0)
            {
                uint8_t* pVCL = pNAL+1;
                int32_t sizeVCL = sizeNAL-1;
                // do not convert the whole frame
                int32_t decSize = sizeVCL<60?sizeVCL:60;
                uint8_t decData[decSize];
                decodeNAL(decData, pVCL, decSize);
                bs_t s;
                bs_init(&s, decData, decSize);
                bs_read_ue(&s);
                int32_t sliceType = bs_read_ue(&s);

                if(sliceType == 1 || sliceType == 6 ||nalReferrenceIDC == 0)
                {
                    //BLOCK_FLAG_TYPE_B;
                    mDiscardCount--;
                    LOGE("Discard packet as video playing under AVDISCARD_BIDIR, mDiscardCount:%d,", mDiscardCount);
                    return true;
                }
            }
            else if(mDiscardLevel == AVDISCARD_NONKEY && mDiscardCount > 0)
            {
                uint8_t* pVCL = pNAL+1;
                int32_t sizeVCL = sizeNAL-1;
                // do not convert the whole frame
                int32_t decSize = sizeVCL<60?sizeVCL:60;
                uint8_t decData[decSize];
                decodeNAL(decData, pVCL, decSize);
                bs_t s;
                bs_init(&s, decData, decSize);
                bs_read_ue(&s);
                int32_t sliceType = bs_read_ue(&s);

                if(sliceType != 2 && sliceType != 7)
                {
                    //not BLOCK_FLAG_TYPE_I;
                    mDiscardCount--;
                    LOGE("Discard packet as video playing under AVDISCARD_NONKEY, mDiscardCount:%d,", mDiscardCount);
                    return true;
                }
            }
            
            if(packet->flags & AV_PKT_FLAG_KEY)
            {               
                //do not discard key frame
                return false;
            }
            else if(nalReferrenceIDC == 0)
            {
                int64_t packetPTS = 0;
                if(packet->pts == AV_NOPTS_VALUE)
                {
                    LOGV("pPacket->pts is AV_NOPTS_VALUE");
                    packetPTS = packet->dts;
                }
                else
                {
                    packetPTS = packet->pts;
                }
                if(packetPTS == AV_NOPTS_VALUE)
                {
                    return false;
                }
                int64_t packetTimeMs = (int64_t)(packetPTS*1000*av_q2d(mVideoStream->time_base));
                int64_t audioPlayingTimeMs = mAudioPlayer->getMediaTimeMs();
                int64_t latenessMs = audioPlayingTimeMs - packetTimeMs;

                //as audio time is playing, so time*2
                latenessMs = latenessMs + mVideoRenderer->swsMs() + mAveVideoDecodeTimeMs*2;
                LOGD("packetTimeMs: %lld, audioPlayingTimeMs: %lld, latenessMs: %lld, maxPacketLatenessMs: %lld",
            		    packetTimeMs, audioPlayingTimeMs, latenessMs, latenessMs);

                if(latenessMs > mMaxLatenessMs)
                {
                    LOGD("discard packet as packet is late");
                    return true;
                }
            }
        }
        
        /* keep the code for use later
        if(nalType >= NAL_SLICE && nalType <= NAL_IDR_SLICE)
        {
            LOGD("packet with NAL video frame");
            uint8_t* pVCL = pNAL+1;
            int32_t sizeVCL = sizeNAL-1;
            // do not convert the whole frame
            int32_t decSize = sizeVCL<60?sizeVCL:60;
            uint8_t decData[decSize];
            decodeNAL(decData, pVCL, decSize);
            bs_t s;
            bs_init(&s, decData, decSize);
            bs_read_ue(&s);
            int32_t sliceType = bs_read_ue(&s);
            LOGD("sliceType:%d", sliceType);

            switch(sliceType)
            {
            case 0: case 5:
                //BLOCK_FLAG_TYPE_P;
                LOGD("packet with VCL P frame");
                LOGD("packet->pts:%lld", packet->pts);
                LOGD("packet->dts:%lld", packet->dts);
                break;
            case 1: case 6:
                //BLOCK_FLAG_TYPE_B;
                LOGD("packet with VCL B frame");
                LOGD("packet->pts:%lld", packet->pts);
                LOGD("packet->dts:%lld", packet->dts);
                break;
            case 2: case 7:
                //BLOCK_FLAG_TYPE_I;
                LOGD("packet with VCL I frame");
                LOGD("packet->pts:%lld", packet->pts);
                LOGD("packet->dts:%lld", packet->dts);
                break;
            case 3: case 8:
                //BLOCK_FLAG_TYPE_P;
                LOGD("packet with VCL SP frame");
                break;
            case 4: case 9:
                //BLOCK_FLAG_TYPE_I;
                LOGD("packet with VCL SI frame");
                break;
            default:
                LOGD("packet with VCL unknown frame");
                break;
            }
        }
        else if(nalType == NAL_SPS)
        {
            LOGD("packet with NAL SPS");
            //Do nothing
        }
        else if(nalType == NAL_PPS)
        {
            LOGD("packet with NAL PPS");
            //Do nothing
        }
        else if(nalType == NAL_SEI)
        {
            LOGD("packet with NAL SEI");
            //Do nothing
        }
        else
        {
            LOGD("packet with NAL others");
            //Do nothing
        }
        */
    }
    
    return false;
}

void FFPlayer::optimizeDecode_l(AVPacket* packet)
{
    //notify ffmpeg to do more optimization.
    if(mDiscardCount > 0)
    {
        //mDiscardCount --;
        if(packet->flags & AV_PKT_FLAG_KEY)
        {
            //do not skip loop filter for key frame
            mVideoStream->codec->skip_loop_filter = AVDISCARD_DEFAULT;
        }
        else
        {
            mVideoStream->codec->skip_loop_filter = AVDISCARD_ALL;
        }
        mVideoStream->codec->skip_frame = mDiscardLevel;
        LOGD("notify ffmpeg to do more optimization. mDiscardLevel:%d, mDiscardCount:%d,", mDiscardLevel, mDiscardCount);
    }
    else
    {
        mVideoStream->codec->skip_loop_filter = AVDISCARD_DEFAULT;
        mVideoStream->codec->skip_frame = AVDISCARD_DEFAULT;
    }
}

int64_t FFPlayer::getFramePTS_l(AVFrame* frame)
{
    int64_t pts = 0;
    if(av_frame_get_best_effort_timestamp(mVideoFrame) != AV_NOPTS_VALUE)
    {
        LOGV("use av_frame_get_best_effort_timestamp");
        pts = av_frame_get_best_effort_timestamp(frame);
    }
    else if(frame->pts != AV_NOPTS_VALUE)
    {
        LOGV("use frame->pts");
        pts = frame->pts;
    }
    else if(frame->pkt_pts != AV_NOPTS_VALUE)
    {
        LOGV("use frame->pkt_pts");
        pts = frame->pkt_pts;
    }
    else if(frame->pkt_dts != AV_NOPTS_VALUE)
    {
        LOGV("use frame->pkt_dts");
        pts = frame->pkt_dts;
    }
    return pts;
}
#ifdef OS_ANDROID
status_t FFPlayer::startCompatibilityTest()
{
    LOGD("startCompatibilityTest ffplayer");
    status_t ret = ERROR;
    AVFormatContext* movieFile = avformat_alloc_context();
    int maxLen = 300;
    const char* fileName = "lib/libsample.so";
    int32_t pathLen = strlen(gPlatformInfo->app_path)+strlen(fileName)+1;
    if(pathLen >= maxLen) return ERROR;
    char path[maxLen];
    memset(path, 0, maxLen);
    strcat(path, gPlatformInfo->app_path);
    strcat(path, fileName);
    LOGD("path:%s", path);
    if(!avformat_open_input(&movieFile, path, NULL, NULL))
    {
        AVStream* videoStream = NULL;
        uint32_t streamsCount = movieFile->nb_streams;
        int32_t videoStreamIndex = 0;
        for (int32_t j = 0; j < streamsCount; j++)
        {
    		if (movieFile->streams[j]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
            {
    		    videoStreamIndex = j;
                videoStream = movieFile->streams[videoStreamIndex];
                break;
    		}
    	}
        
        if(videoStream != NULL)
        {
            videoStream->codec->pix_fmt = PIX_FMT_YUV420P;
            AVFrame* videoFrame = avcodec_alloc_frame();
            
            AVCodecContext* codec_ctx = videoStream->codec;
        	AVCodec* codec = avcodec_find_decoder(codec_ctx->codec_id);
        	if (codec != NULL && avcodec_open(codec_ctx, codec) >= 0)
            {
                LOGD("getpriority before:%d", getpriority(PRIO_PROCESS, 0));
                LOGD("sched_getscheduler:%d", sched_getscheduler(0));
                int videoThreadPriority = -16;
                if(setpriority(PRIO_PROCESS, 0, videoThreadPriority) != 0)
                {
                    LOGE("set video thread priority failed");
                }
                LOGD("getpriority after:%d", getpriority(PRIO_PROCESS, 0));
    
            	int64_t begin_test = getNowMs();
                int64_t testCostMs = 0;
                int32_t picCount = 0;
                int32_t minFPS = 25;
                int64_t minTestMs = 1200;
                while(picCount<minFPS && testCostMs<minTestMs)
                {
                    AVPacket* pPacket = (AVPacket*)av_malloc(sizeof(AVPacket));
                    int32_t ret = av_read_frame(movieFile, pPacket);
                    if (pPacket->stream_index == videoStreamIndex)
                    {
                        //video
                    	LOGV("video Packet->pts:%lld", pPacket->pts);
                    	LOGV("video Packet->dts:%lld", pPacket->dts);
                        
                        // Decode video frame
                    	int64_t begin_decode = getNowMs();
                        int32_t gotPicture = 0;
                    	int len = avcodec_decode_video2(videoStream->codec,
                            						 videoFrame,
                            						 &gotPicture,
                            						 pPacket);
                    	int64_t end_decode = getNowMs();
                        int64_t costTime = end_decode-begin_decode;
                    	LOGD("decode video cost %lld[ms]", costTime);
                        if (len >= 0)
                        {
                    		if(gotPicture!=0)
                    		{
                    		    picCount++;
                    		}
                    	}
                    }
                    if(pPacket != NULL)
                    {
                        //free packet resource
            	        av_free_packet(pPacket);
                        av_free(pPacket);
                        pPacket = NULL;
                    }
                	testCostMs = getNowMs() - begin_test;
                }
                LOGI("picCount:%d, testCostMs:%lld", picCount, testCostMs);
                ret = (testCostMs<850)?OK:ERROR; //it is an experience value, do not change it.
                
                if(setpriority(PRIO_PROCESS, 0, 0) != 0)
                {
                    LOGE("set video thread priority back failed");
                }
                LOGD("getpriority after:%d", getpriority(PRIO_PROCESS, 0));

                /*
                {
                    FILE *pFile;
                    char* path = "/data/data/com.pplive.meetplayer/test.txt";
                	LOGD("Start open file %s", path);
                	pFile=fopen(path, "wb");
                	if(pFile==NULL)
                	{
                		LOGE("open file %s failed", path);
                		return ERROR;
                	}
                	LOGD("open file %s success", path);

                    int64_t data2 = 0x123;
                    int data[4] = {(int)testCostMs,data2};
                	fwrite(data, 1, 8, pFile);
                	fclose(pFile);
                }
                */
                
                if(codec_ctx != NULL)
                {
                    LOGD("avcodec_close video codec");
                    avcodec_close(codec_ctx);
                }
            }
            
            if(videoFrame != NULL)
            {
                LOGD("avcodec_free_frame");
                avcodec_free_frame(&videoFrame);
                videoFrame = NULL;
            }
        }
    }
    else
    {
        LOGE("avformat_open_input failed");
    }
    
    if(movieFile != NULL)
    {
        // Close stream
        LOGD("avformat_close_input");
        avformat_close_input(&movieFile);
    }
    return ret;
}
#endif
bool FFPlayer::getMediaInfo(const char* url, MediaInfo* info)
{
    if(url == NULL || info == NULL) return false;
    
    bool ret = false;

    struct stat buf;
    int32_t iresult = stat(url, &buf);
    if(iresult == 0)
    {
        info->size_byte = buf.st_size;
    }
    else
    {
        return ret;
    }
    
    AVFormatContext* movieFile = avformat_alloc_context();
    LOGD("check file %s", url);
    if(!avformat_open_input(&movieFile, url, NULL, NULL))
    {
        if(movieFile->duration <= 0)
        {
        	if(avformat_find_stream_info(movieFile, NULL) >= 0)
            {
                ret = true;
                info->duration_ms = movieFile->duration*1000/AV_TIME_BASE;
        	}
            else
            {
                LOGE("avformat_find_stream_info failed");
            }
        }
        else
        {
            ret = true;
            info->duration_ms = movieFile->duration*1000/AV_TIME_BASE;
        }
    }
    else
    {
        LOGE("avformat_open_input failed");
    }
    
    if(movieFile != NULL)
    {
        // Close stream
        LOGD("avformat_close_input");
        avformat_close_input(&movieFile);
    }
    LOGD("File duration:%d", info->duration_ms);
    LOGD("File size:%lld", info->size_byte);
    return ret;
}

bool generateThumbnail(AVFrame* videoFrame, 
    int32_t* thumbnail,
    int32_t width , 
    int32_t height)
{
    int32_t srcWidth = width<height?width:height;
    int32_t srcHeight = srcWidth;
    
    struct SwsContext* convertCtx = sws_getContext(videoFrame->width,
								 videoFrame->height,
								 (PixelFormat)videoFrame->format,
								 width,
								 height,
								 PIX_FMT_BGRA,
								 SWS_POINT,
								 NULL,
								 NULL,
								 NULL);
	if (convertCtx == NULL)
    {
		LOGE("create convert ctx failed");
		return false;
	}
    LOGD("videoFrame->width:%d", videoFrame->width);
    LOGD("videoFrame->height:%d", videoFrame->height);
    LOGD("videoFrame->format:%d", videoFrame->format);

    bool result = false;
    AVFrame* surfaceFrame = avcodec_alloc_frame();
    if (surfaceFrame != NULL)
    {
        int ret = avpicture_fill((AVPicture*)surfaceFrame,
                            (uint8_t*)thumbnail,
                            PIX_FMT_BGRA,
                            width,
                            height);
        if(ret >= 0)
        {
            // Convert the image	
            sws_scale(convertCtx,
            	      videoFrame->data,
            	      videoFrame->linesize,
                      0,
            	      videoFrame->height,
            	      surfaceFrame->data,
                      surfaceFrame->linesize);
            result = true;
            LOGD("sws_scale thumbnail success");
        }
        else
        {
        	LOGE("avpicture_fill failed, ret:%d", ret);
        }
    }
    else
    {
        LOGE("alloc frame failed");
    }

    if(surfaceFrame != NULL)
    {
        //do not free data[];
        av_free(surfaceFrame);
        surfaceFrame = NULL;
    }
    if(convertCtx != NULL)
    {
        sws_freeContext(convertCtx);
        convertCtx = NULL;
    }
    /*
    {
        static bool pass = false;
        if(!pass)
        {
            FILE *pFile;
            char* path = "/sdcard/test.rgb565";
        	LOGD("Start open file %s", path);
        	pFile=fopen(path, "wb");
        	if(pFile==NULL)
        	{
        		LOGE("open file %s failed", path);
        		return result;
        	}
        	LOGD("open file %s success", path);

        	fwrite(thumbnail, 1, width*height*2, pFile);
        	fclose(pFile);
            pass = true;
        }
    }
    */
    return result;
}

bool FFPlayer::getMediaDetailInfo(const char* url, MediaInfo* info)
{
    if(url == NULL || info == NULL) return false;
    
    bool ret = false;

    struct stat buf;
    int32_t iresult = stat(url, &buf);
    if(iresult == 0)
    {
        info->size_byte = buf.st_size;
    }
    else
    {
        return ret;
    }
    
    AVFormatContext* movieFile = avformat_alloc_context();
    LOGD("check file %s", url);
    if(!avformat_open_input(&movieFile, url, NULL, NULL))
    {
    	if(avformat_find_stream_info(movieFile, NULL) >= 0)
        {
            ret = true;
            info->duration_ms = movieFile->duration*1000/AV_TIME_BASE;
            info->format_name = movieFile->iformat->name;

            uint32_t streamsCount = movieFile->nb_streams;
            LOGD("streamsCount:%d", streamsCount);

            info->audio_channels = 0;
            info->video_channels = 0;
        	for (int32_t i = 0; i < streamsCount; i++)
            {
        		if (movieFile->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
                {
                    info->audio_channels++;
                    AVStream* stream = movieFile->streams[i];
                    if(stream != NULL)
                    {
                        AVCodecContext* codec_ctx = stream->codec;
                        if(codec_ctx != NULL)
                        {
                        	AVCodec* codec = avcodec_find_decoder(codec_ctx->codec_id);
                        	if (codec != NULL)
                            {
                                info->audio_name = codec->name;
                            }
                        }
                    }
        		}
                else if(movieFile->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
                {
                    info->video_channels++;
                    AVStream* stream = movieFile->streams[i];
                    if(stream == NULL)
                    {
                        LOGE("stream is NULL");
                        continue;
                    }
                    AVCodecContext* codec_ctx = stream->codec;
                    if(codec_ctx == NULL)
                    {
                        LOGE("codec_ctx is NULL");
                        continue;
                    }
                    info->width = codec_ctx->width;
                    info->height = codec_ctx->height;
                    
                	AVCodec* codec = avcodec_find_decoder(codec_ctx->codec_id);
                	if (codec == NULL)
                    {
                        LOGE("avcodec_find_decoder failed");
                        continue;
                    }
                    
                    info->video_name = codec->name;
                }
        	}
    	}
        else
        {
            LOGE("avformat_find_stream_info failed");
        }
    }
    else
    {
        LOGE("avformat_open_input failed");
    }
    
    if(movieFile != NULL)
    {
        // Close stream
        LOGD("avformat_close_input");
        avformat_close_input(&movieFile);
    }
    LOGD("File duration:%d", info->duration_ms);
    LOGD("File size:%lld", info->size_byte);
    LOGD("width:%d", info->width);
    LOGD("height:%d", info->height);
    LOGD("format name:%s", info->format_name!=NULL?info->format_name:"");
    LOGD("audio name:%s", info->audio_name!=NULL?info->audio_name:"");
    LOGD("video name:%s", info->video_name!=NULL?info->video_name:"");
    LOGD("audio_channels:%d", info->audio_channels);
    LOGD("video_channels:%d", info->video_channels);
    return ret;
}

bool FFPlayer::getThumbnail(const char* url, MediaInfo* info)
{
    if(url == NULL || info == NULL) return false;
    
    bool ret = false;

    struct stat buf;
    int32_t iresult = stat(url, &buf);
    if(iresult == 0)
    {
        info->size_byte = buf.st_size;
    }
    else
    {
        return ret;
    }
    
    AVFormatContext* movieFile = avformat_alloc_context();
    LOGD("check file %s", url);
    if(!avformat_open_input(&movieFile, url, NULL, NULL))
    {
    	if(avformat_find_stream_info(movieFile, NULL) >= 0)
        {
            info->duration_ms = movieFile->duration*1000/AV_TIME_BASE;
            info->format_name = movieFile->iformat->name;

            uint32_t streamsCount = movieFile->nb_streams;
            LOGD("streamsCount:%d", streamsCount);

            info->audio_channels = 0;
            info->video_channels = 0;
        	for (int32_t i = 0; i < streamsCount; i++)
            {
        		if (movieFile->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO
                    && info->audio_channels == 0)
                {
                    info->audio_channels++;
                    AVStream* stream = movieFile->streams[i];
                    if(stream != NULL)
                    {
                        AVCodecContext* codec_ctx = stream->codec;
                        if(codec_ctx != NULL)
                        {
                        	AVCodec* codec = avcodec_find_decoder(codec_ctx->codec_id);
                        	if (codec != NULL)
                            {
                                info->audio_name = codec->name;
                            }
                        }
                    }
        		}
                else if(movieFile->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO 
                    && info->video_channels == 0)
                {
                    info->video_channels++;
                    AVStream* stream = movieFile->streams[i];
                    if(stream == NULL)
                    {
                        LOGE("stream is NULL");
                        continue;
                    }
                    AVCodecContext* codec_ctx = stream->codec;
                    if(codec_ctx == NULL)
                    {
                        LOGE("codec_ctx is NULL");
                        continue;
                    }
                    info->width = codec_ctx->width;
                    info->height = codec_ctx->height;
                    
                	AVCodec* codec = avcodec_find_decoder(codec_ctx->codec_id);
                	if (codec == NULL)
                    {
                        LOGE("avcodec_find_decoder failed");
                        continue;
                    }
                    
                    info->video_name = codec->name;
                	if (avcodec_open(codec_ctx, codec) >= 0)
                    {
                        int32_t seekPosition = 15;
                        if(info->duration_ms > 0 && info->duration_ms < seekPosition*1000)
                        {
                            seekPosition = info->duration_ms/1000;
                        }
                        if(avformat_seek_file(movieFile,
                            -1, 
                            INT64_MIN, 
                            seekPosition*AV_TIME_BASE, //in AV_TIME_BASE
                            INT64_MAX, 
                            0) >= 0)
                        {
                            bool tryThumbnail = true;
                            while(tryThumbnail)
                            {
                                AVPacket* pPacket = (AVPacket*)av_malloc(sizeof(AVPacket));
                                memset(pPacket, 0, sizeof(AVPacket));
                                int32_t readRet = av_read_frame(movieFile, pPacket);
                                if(readRet != 0)
                                {
                                    LOGE("av_read_frame error: %d", readRet);
                                    return ret;
                                }
                                
                                if (pPacket->stream_index == i)
                                {
                                    AVFrame* videoFrame = avcodec_alloc_frame();
                                    int32_t gotPicture = 0;
                                	int len = avcodec_decode_video2(codec_ctx,
                                        						 videoFrame,
                                        						 &gotPicture,
                                        						 pPacket);
                                    if (len >= 0 && gotPicture != 0)
                                    {
                                        LOGD("got picture");
                                        //todo:get 96x96 argb8888 data.
                                        info->thumbnail_width = 96; 
                                        info->thumbnail_height = 96;
                                        info->thumbnail = (int*)malloc(info->thumbnail_width*info->thumbnail_height*4);
                                        if(generateThumbnail(videoFrame, info->thumbnail, info->thumbnail_width, info->thumbnail_height))
                                        {
                                            ret = true;
                                            LOGD("got thumbnail");
                                        }
                                        else
                                        {
                                            LOGE("failed to get thumbnail");
                                        }
                                        tryThumbnail = false;
                                	}
               
                                    if(videoFrame != NULL)
                                    {
                                        LOGD("avcodec_free_frame");
                                        avcodec_free_frame(&videoFrame);
                                        videoFrame = NULL;
                                    }
                                }
                                if(pPacket != NULL)
                                {
                                    //free packet resource
                                    LOGD("av_free_packet");
                        	        av_free_packet(pPacket);
                                    av_free(pPacket);
                                    pPacket = NULL;
                                }
                            }
                        }
                        
                        if(codec_ctx != NULL)
                        {
                            LOGD("avcodec_close video codec");
                            avcodec_close(codec_ctx);
                        }
                    }
                }
        	}
    	}
        else
        {
            LOGE("avformat_find_stream_info failed");
        }
    }
    else
    {
        LOGE("avformat_open_input failed");
    }
    
    if(movieFile != NULL)
    {
        // Close stream
        LOGD("avformat_close_input");
        avformat_close_input(&movieFile);
    }
    LOGD("File duration:%d", info->duration_ms);
    LOGD("File size:%lld", info->size_byte);
    LOGD("width:%d", info->width);
    LOGD("height:%d", info->height);
    LOGD("format name:%s", info->format_name!=NULL?info->format_name:"");
    LOGD("audio name:%s", info->audio_name!=NULL?info->audio_name:"");
    LOGD("video name:%s", info->video_name!=NULL?info->video_name:"");
    LOGD("thumbnail_width:%d", info->thumbnail_width);
    LOGD("thumbnail_height:%d", info->thumbnail_height);
    LOGD("thumbnail:%d", (info->thumbnail!=NULL));
    LOGD("audio_channels:%d", info->audio_channels);
    LOGD("video_channels:%d", info->video_channels);
    return ret;
}
