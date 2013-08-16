
#define __STDINT_LIMITS
#include <sys/resource.h>
#include <stdint.h>
#include <unistd.h>
#ifdef OS_ANDROID
#include <sys/sysinfo.h>
#endif
#include <sched.h>
#include <sys/time.h>

extern "C"
{
#include "libavformat/avformat.h"
}

#include "cpu-features.h"
#include "errors.h"
#include "log.h"
#include "utils.h"
#include "autolock.h"
#include "ffstream.h"

#define LOG_TAG "FFStream"

#define FF_PLAYER_MIN_FRAME_COUNT_DEFAULT 100 //4s
#define FF_PLAYER_MAX_QUEUE_SIZE_DEFAULT 1024*1024*10 //10m

FFStream::FFStream()
{
    mMovieFile = NULL;
    mBufferSize = 0;
    mMinBufferCount = FF_PLAYER_MIN_FRAME_COUNT_DEFAULT;
    mMaxBufferSize = FF_PLAYER_MAX_QUEUE_SIZE_DEFAULT;
    mStatus = FFSTREAM_INITED;
    mRunning = false;
    mReachEndStream = false;
    mIsBuffering = false;
    mSeeking = false;
    mSeekTimeMs = 0;
    mCachedDurationMs = 0;
    mListener = NULL;
    mUrlType = TYPE_UNKNOWN;

    mAudioStream = NULL;
    mVideoStream = NULL;
    mAudioChannelSelected = -1;
	mAudioStreamIndex = -1;
	mVideoStreamIndex = -1;
    mStreamsCount = 0;
    mDurationMs = 0;
        
    pthread_cond_init(&mCondition, NULL);
    pthread_mutex_init(&mLock, NULL);
    
    //audio
    //pthread_mutex_init(&mAudioQueueLock, NULL);
    //mAudioBufferSize = 0;

    //video
    //pthread_mutex_init(&mVideoQueueLock, NULL);
    //mVideoBufferSize = 0;
}

FFStream::~FFStream()
{
    if(mStatus == FFSTREAM_STARTED)
    {
        stop();
    }
    
    //pthread_mutex_destroy(&mAudioQueueLock);
    //pthread_mutex_destroy(&mVideoQueueLock);
    pthread_cond_destroy(&mCondition);
    pthread_mutex_destroy(&mLock);
}

status_t FFStream::selectAudioChannel(int32_t index)
{
    mAudioChannelSelected = index;
    return OK;
}

AVFormatContext* FFStream::open(char* uri)
{    
    AutoLock autoLock(&mLock);
    if(mStatus != FFSTREAM_INITED)
        return NULL;
#ifdef OS_ANDROID
    uint64_t cpuFeatures = android_getCpuFeatures();
    if ((cpuFeatures & ANDROID_CPU_ARM_FEATURE_NEON) == 0)
    {
        //to give low cpu powered device enough time for decoding.
        mMaxBufferSize = 1024*1024*4;
        LOGD("mMaxBufferSize:%u", mMaxBufferSize);
    }
#endif
    
	// Open steam
	LOGI("open url:%s", uri);
    mMovieFile = avformat_alloc_context(); 
    AVIOInterruptCB cb = {interrupt_l, this}; 
    mMovieFile->interrupt_callback = cb; 
	if(avformat_open_input(&mMovieFile, uri, NULL, NULL) != 0)
    {
        LOGE("open url failed");
        return NULL;
	}
    else
    {
        LOGI("open url success");
    }

    //check url type
    if(strncmp(uri, "/", 1) == 0)
    {
        mUrlType = TYPE_LOCAL_FILE;
        mMinBufferCount = 25;
    }
    else if(strncmp(uri, "http", 4) == 0)
    {
        mUrlType = TYPE_HTTP;
    }
    
    if(mUrlType == TYPE_LOCAL_FILE)
    {
    	// Retrieve stream information after disable variant streams, like m3u8 
    	if(avformat_find_stream_info(mMovieFile, NULL) < 0)
        {
            LOGE("avformat_find_stream_info failed");
            avformat_close_input(&mMovieFile);
            return NULL;
    	}
        else
        {
            LOGD("avformat_find_stream_info success");
        }
    }
    
    mStreamsCount = mMovieFile->nb_streams;
    LOGD("mStreamsCount:%d", mStreamsCount);
    
	for (int32_t i = 0; i < mStreamsCount; i++)
    {
		if (mMovieFile->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            if(mAudioStreamIndex == -1 && (mAudioChannelSelected==-1 || mAudioChannelSelected==i))
            {
    		    mAudioStreamIndex = i;
                LOGD("mAudioStreamIndex:%d", mAudioStreamIndex);
                mAudioStream = mMovieFile->streams[mAudioStreamIndex];
            }
            else
            {
                //by default, use the first audio stream, and discard others.
                mMovieFile->streams[i]->discard = AVDISCARD_ALL;
                LOGI("Discard audio stream:%d", i);
            }
		}
	}

    //Some audio file includes video stream as album. we need to skip it.
    //Todo: support displaying album picture when playing audio file
    if(mMovieFile->iformat->name != NULL
        && strcmp(mMovieFile->iformat->name, "mp3") != 0
        && strcmp(mMovieFile->iformat->name, "ogg") != 0
        && strcmp(mMovieFile->iformat->name, "wmav1") != 0
        && strcmp(mMovieFile->iformat->name, "wmav2") != 0)
    {
    	for (int32_t j = 0; j < mStreamsCount; j++)
        {
    		if (mMovieFile->streams[j]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
            {
                if(mVideoStreamIndex == -1)
                {
    			    mVideoStreamIndex = j;
                    LOGD("mVideoStreamIndex:%d", mVideoStreamIndex);
                    mVideoStream = mMovieFile->streams[mVideoStreamIndex];
                }
                else
                {
                    //by default, use the first video stream, and discard others.
                    mMovieFile->streams[j]->discard = AVDISCARD_ALL;
                    LOGI("Discard video stream:%d", j);
                }
    		}
    	}
    }

    if(mAudioStreamIndex == -1 && mVideoStreamIndex == -1)
    {
        LOGE("No stream");
        avformat_close_input(&mMovieFile);
        return NULL;
    }

    if(mUrlType != TYPE_LOCAL_FILE)
    {
        //It is PPTV online video,
        //do not call avformat_find_stream_info to reduce 
        //video buffering time
        if(mVideoStream != NULL)
        {
            mVideoStream->codec->pix_fmt = PIX_FMT_YUV420P;
        }
    }
    
	mDurationMs = mMovieFile->duration*1000/AV_TIME_BASE;
    LOGD("file duration: %lld(ms)", mDurationMs);

    mStatus = FFSTREAM_PREPARED;
    return mMovieFile;
}

status_t FFStream::start()
{
    AutoLock autoLock(&mLock);
    if(mStatus != FFSTREAM_PREPARED)
        return ERROR;
    
    pthread_create(&mThread, NULL, handleThreadStart, this);

    mReachEndStream = false;
    mStatus = FFSTREAM_STARTED;
    return OK;
}

status_t FFStream::stop()
{
    if(mStatus == FFSTREAM_STOPPED)
        return OK;
    
    mRunning = false;
    mStatus = FFSTREAM_STOPPED;
    pthread_cond_signal(&mCondition);
    
    flush_l();
    join_l();
    
	if(mMovieFile != NULL)
    {
        // Close stream
        LOGD("avformat_close_input");
        avformat_close_input(&mMovieFile);
    }
    return OK;
}

status_t FFStream::seek(int64_t seekTimeMs)
{
    AutoLock autoLock(&mLock);
    mSeekTimeMs = seekTimeMs;
    mSeeking = true;
    mReachEndStream = false;
    pthread_cond_signal(&mCondition);
    return OK;
}

status_t FFStream::setListener(MediaPlayerListener* listener)
{
    mListener = listener;
    return OK;
}

status_t FFStream::getPacket(int32_t streamIndex, AVPacket** packet)
{
    AutoLock autoLock(&mLock);
    if(mSeeking)
    {
		LOGD("queue seeking");
        return FFSTREAM_ERROR_BUFFERING;
    }
    
    if(streamIndex == mAudioStreamIndex)
    {
        if(mIsBuffering)
        {
            if(!mReachEndStream)
            {
                return FFSTREAM_ERROR_BUFFERING;
            }
            else
            {
                mIsBuffering = false;
                LOGD("MEDIA_INFO_BUFFERING_END because of stream end");
                notifyListener_l(MEDIA_INFO, MEDIA_INFO_BUFFERING_END);
            }
        }
        
        AVPacket* pPacket = mAudioQueue.get();
        if(pPacket != NULL)
        {
            if (pPacket->data && !strcmp((char*)pPacket->data, "FLUSH_AUDIO"))
            {
                *packet = pPacket;
                return FFSTREAM_ERROR_FLUSHING;
            }
            else
            {
                mBufferSize-=pPacket->size;
                *packet = pPacket;
                return FFSTREAM_OK;
            }
        }
		else
		{
		    if(mReachEndStream)
            {
                return FFSTREAM_ERROR_EOF;
            }
            else
            {
		        LOGD("audio queue empty");
                if(!mIsBuffering)
                {
                    mIsBuffering = true;
                    LOGD("MEDIA_INFO_BUFFERING_START");
                    notifyListener_l(MEDIA_INFO, MEDIA_INFO_BUFFERING_START);
                }
                return FFSTREAM_ERROR_BUFFERING;
            }
		}
    }
    else if(streamIndex == mVideoStreamIndex)
    {
        AVPacket* pPacket = mVideoQueue.get();
        if(pPacket != NULL)
        {
            if (pPacket->data && !strcmp((char*)pPacket->data, "FLUSH_VIDEO"))
            {
                *packet = pPacket;
                return FFSTREAM_ERROR_FLUSHING;
            }
            else
            {
                mBufferSize-=pPacket->size;
                *packet = pPacket;
                return FFSTREAM_OK;
            }
        }
		else
		{
		    if(mReachEndStream)
            {
                return FFSTREAM_ERROR_EOF;
            }
            else
            {
		        LOGD("video queue empty");
                if(!mIsBuffering)
                {
                    mIsBuffering = true;
                    LOGD("MEDIA_INFO_BUFFERING_START");
                    notifyListener_l(MEDIA_INFO, MEDIA_INFO_BUFFERING_START);
                }
                return FFSTREAM_ERROR_BUFFERING;
            }
		}
    }
    else
    {
        LOGE("Unknown stream index");
        return FFSTREAM_ERROR_STREAMINDEX;
    }
}

void FFStream::run()
{
    mIsBuffering = true;
    LOGD("MEDIA_INFO_BUFFERING_START");
    notifyListener_l(MEDIA_INFO, MEDIA_INFO_BUFFERING_START);
    
    while(mRunning)
    {
        if(mStatus == FFSTREAM_PAUSED ||mReachEndStream)
        {
            int64_t waitUs = 10000ll;//10ms
            struct timespec ts;
            ts.tv_sec = 0;
            ts.tv_nsec = waitUs*1000ll;
            AutoLock autoLock(&mLock);
            pthread_cond_timedwait_relative_np(&mCondition, &mLock, &ts);
            continue;
        }
        else if(mStatus == FFSTREAM_STOPPED)
        {
            break;
        }
        else
        {
            if(mSeeking)
            {
                AutoLock autoLock(&mLock);
                if(avformat_seek_file(mMovieFile,
                                    -1, 
                                    INT64_MIN, 
                                    (mSeekTimeMs/1000)*AV_TIME_BASE, //in AV_TIME_BASE
                                    INT64_MAX, 
                                    AVSEEK_FLAG_BACKWARD) < 0)
                {
                    LOGE("Seek to:%lld failed");
                    mSeeking = false;
                    continue;
                }
                
                flush_l();

                if(mAudioStream != NULL)
                {
                    AVPacket* flushAudioPkt = (AVPacket*)av_malloc(sizeof(AVPacket));
                    av_init_packet(flushAudioPkt);
                    flushAudioPkt->size = 0;
                    flushAudioPkt->data = (uint8_t*)"FLUSH_AUDIO";
                    mAudioQueue.put(flushAudioPkt);
                }
                
                if(mVideoStream != NULL)
                {
                    AVPacket* flushVideoPkt = (AVPacket*)av_malloc(sizeof(AVPacket));
                    av_init_packet(flushVideoPkt);
                    flushVideoPkt->size = 0;
                    flushVideoPkt->data = (uint8_t*)"FLUSH_VIDEO";
                    mVideoQueue.put(flushVideoPkt);
                }
                
                mSeeking = false;
                LOGD("send event MEDIA_SEEK_COMPLETE");
                notifyListener_l(MEDIA_SEEK_COMPLETE);
                continue;
            }
            
            if(mIsBuffering &&
                (mAudioStream!=NULL?mAudioQueue.count() > mMinBufferCount:true) &&
                (mVideoStream!=NULL?mVideoQueue.count() > mMinBufferCount:true))
            {
                mIsBuffering = false;
                LOGD("MEDIA_INFO_BUFFERING_END");
                notifyListener_l(MEDIA_INFO, MEDIA_INFO_BUFFERING_END);
            }

            LOGD("mBufferSize:%d", mBufferSize);
            if(mBufferSize > mMaxBufferSize)
            {
                LOGD("Buffering reaches max size");
                if(mIsBuffering)
                {
                    mMaxBufferSize*=2;
                    LOGI("Double max buffer size to:%d", mMaxBufferSize);
                }
                else
                {
                    struct timespec ts;
                    ts.tv_sec = 1; //1s
                    ts.tv_nsec = 0;
                    AutoLock autoLock(&mLock);
                    pthread_cond_timedwait_relative_np(&mCondition, &mLock, &ts);
                }
                continue;
            }
            LOGD("av_read_frame before");
            AVPacket* pPacket = (AVPacket*)av_malloc(sizeof(AVPacket));
            memset(pPacket, 0, sizeof(AVPacket));
            int32_t ret = av_read_frame(mMovieFile, pPacket);
            LOGD("av_read_frame after, ret:%d", ret);

            AutoLock autoLock(&mLock);
            if(mStatus == FFSTREAM_STOPPED)
            {
                //this is to fix ffmpeg issue that av_read_frame does not return
                // AVERROR_EXIT after stop by application as expected.
                av_free_packet(pPacket);
                av_free(pPacket);
                LOGE("FFSTREAM_STOPPED");
                break;
            }

            if(mSeeking)
            {
                av_free_packet(pPacket);
                av_free(pPacket);
                LOGD("Seek during reading frame");
                continue;
            }
            
            if(ret == AVERROR_EOF)
            {
                av_free_packet(pPacket);
                av_free(pPacket);
                
                //end of stream
                mReachEndStream = true;
                LOGE("reach end of stream");
                continue;
            }
            else if(ret == AVERROR_EXIT)
            {
                //May never come here
                //abort read
                mReachEndStream = true;
                av_free_packet(pPacket);
                av_free(pPacket);
                continue;
            }
            else if(ret == AVERROR(EIO) || ret == AVERROR(ENOMEM))
            {
                LOGE("read error: %d", ret);
                mReachEndStream = true;
                if(pPacket->data)
                {
                    av_free_packet(pPacket);
                }
                av_free(pPacket);
                continue;
            }
            else if(ret == AVERROR_INVALIDDATA ||
                    pPacket->size <= 0 ||
                    pPacket->size > mMaxBufferSize ||
                    pPacket->data == NULL)
            {
                LOGE("invalid packet");
                LOGD("ret:%d", ret);
                
                if(pPacket->data)
                {
                    av_free_packet(pPacket);
                }
                av_free(pPacket);
                
                int64_t waitUs = 10000ll;//10ms
                struct timespec ts;
                ts.tv_sec = 0;
                ts.tv_nsec = waitUs*1000ll;
                pthread_cond_timedwait_relative_np(&mCondition, &mLock, &ts);
                continue;
            }
            else if(ret < 0)
            {
                //error
                mReachEndStream = true;
                av_free_packet(pPacket);
                av_free(pPacket);
                LOGE("read frame error:%d", ret);
                break;
            }
            
        	if (pPacket->stream_index == mAudioStreamIndex)
            {
                //audio
                #if (NDEBUG==0)
                int64_t pts = (int64_t)(pPacket->pts*1000*av_q2d(mAudioStream->time_base));
                int64_t dts = (int64_t)(pPacket->dts*1000*av_q2d(mAudioStream->time_base));
            	LOGD("audio Packet->pts:%lld ms", pts);
            	LOGD("audio Packet->dts:%lld ms", dts);
            	#endif
                mAudioQueue.put(pPacket);
                mBufferSize+=pPacket->size;

                //update cached duration
                int64_t packetPTS = 0;
                if(pPacket->pts == AV_NOPTS_VALUE)
                {
                    LOGD("pPacket->pts is AV_NOPTS_VALUE");
                    packetPTS = pPacket->dts;
                }
                else
                {
                    packetPTS = pPacket->pts;
                }
                if(packetPTS == AV_NOPTS_VALUE)
                {
                    mCachedDurationMs += (pPacket->duration*1000*av_q2d(mAudioStream->time_base));
                }
                mCachedDurationMs = (int64_t)(packetPTS*1000*av_q2d(mAudioStream->time_base));
                LOGD("mCachedDurationMs:%lld", mCachedDurationMs);
            }
            else if (pPacket->stream_index == mVideoStreamIndex)
            {
                //video
                #if (NDEBUG==0)
                int64_t pts = (int64_t)(pPacket->pts*1000*av_q2d(mVideoStream->time_base));
                int64_t dts = (int64_t)(pPacket->dts*1000*av_q2d(mVideoStream->time_base));
            	LOGD("video Packet->pts:%lld ms", pts);
            	LOGD("video Packet->dts:%lld ms", dts);
            	#endif
                mVideoQueue.put(pPacket);
                mBufferSize+=pPacket->size;
            }
            else
            {
                // Free the packet that was allocated by av_read_frame
                av_free_packet(pPacket);
                av_free(pPacket);
                //LOGD("Unknown stream type:%d", pPacket->stream_index);
            }
        }
    }

    LOGD("exit stream thread");
}

status_t FFStream::flush_l()
{
    mAudioQueue.flush();
    mVideoQueue.flush();
    mBufferSize = 0;
    return OK;
}

status_t FFStream::join_l()
{
    if(pthread_join(mThread, NULL) == 0)
    {
        return OK;
    }
    else
    {
        return ERROR;
    }
}

void* FFStream::handleThreadStart(void* ptr)
{
#ifdef OS_ANDROID
    LOGD("getpriority before:%d", getpriority(PRIO_PROCESS, 0));
    int streamThreadPriority = -6;
    if(setpriority(PRIO_PROCESS, 0, streamThreadPriority) != 0)
    {
        LOGE("set stream thread priority failed");
    }
    LOGD("getpriority after:%d", getpriority(PRIO_PROCESS, 0));
#endif
    
    FFStream* stream = (FFStream*)ptr;
    stream->mRunning = true;    
    stream->run();
    stream->mRunning = false;
    return 0;
}

status_t FFStream::interrupt_l(void* ctx)
{
    //LOGD("Checking interrupt_l");
    FFStream* stream = (FFStream*)ctx;
    if(stream == NULL) return 1;
    if(stream->status() == FFSTREAM_STOPPED)
    {
        //abort av_read_frame.
        LOGI("interrupt_l: FFSTREAM_STOPPED");
        return 1; 
    }
    else if(stream->isSeeking())
    {
        //abort av_read_frame.
        //TODO: av_read_frame does not return AVERROR_EXIT but AVERROR_EOF.
        //need to fix it.
        LOGD("interrupt_l: isSeeking");
        return 1;
    }
    else
    {
        return 0;
    }
}

void FFStream::notifyListener_l(int msg, int ext1, int ext2)
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