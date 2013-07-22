/*
 * Copyright (C) 2012 Roger Shen  rogershen@pptv.com
 *
 */
 
#include <sys/resource.h>
#include <sched.h>
#include <sys/time.h>
#include <unistd.h>
extern "C"
{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"
}

#define LOG_TAG "AudioPlayer"

#include "utils.h"
#include "autolock.h"
#include "errors.h"
#include "player.h"
#include "log.h"
#include "audiotrack.h"
#include "audioplayer.h"


class AudioRender
{
public:
    AudioRender()
    {
        mSamplesSize = AVCODEC_MAX_AUDIO_FRAME_SIZE*2;
        mSamples = NULL;
        mConvertCtx = NULL;
        
        //we assume android/ios devices have two speakers.
        //todo: need to support devices which have more speakers.
        mDeviceChannelLayoutOutput = AV_CH_LAYOUT_STEREO;
        mDeviceChannels = 2;
    }
    
    ~AudioRender()
    {
        if(mSamples != NULL)
        {
            // Free audio samples buffer
            av_free(mSamples);
            mSamples = NULL;
        }
        if(mConvertCtx != NULL)
        {
            swr_free(&mConvertCtx);
        }
    }

    status_t open(int sampleRate, 
            uint64_t channelLayout,
            int channels,
            AVSampleFormat sampleFormat)
    {
        mSampleRate = sampleRate;
        mChannelLayout = channelLayout;
        if(mChannelLayout <= 0)
        {
            mChannelLayout = AV_CH_LAYOUT_MONO;
        }
        mChannels = channels;
        if(mChannels <= 0)
        {
            mChannels = 1;
        }
        mSampleFormat = sampleFormat;

        mSampleFormatOutput = mSampleFormat;
        mSampleRateOutput = mSampleRate;
        mChannelLayoutOutput = mChannelLayout;
        mChannelsOutput = mChannels;
    
        if((mSampleFormatOutput < AV_SAMPLE_FMT_U8 ||
            mSampleFormatOutput > AV_SAMPLE_FMT_S16) ||
            mSampleRateOutput < 4000 ||
            mSampleRateOutput > 48000 ||
            mChannelsOutput > mDeviceChannels ||
            mChannelsOutput < 1)
        {
            if(mSamples == NULL)
            {
                mSamples = (int16_t*)av_malloc(mSamplesSize);
                if(mSamples == NULL)
                {
                    LOGE("No enough memory for audio converstion");
                    return ERROR;
                }
            }

            if(mConvertCtx == NULL)
            {
                switch(mSampleFormat)
                {
                case AV_SAMPLE_FMT_U8:
                case AV_SAMPLE_FMT_U8P:
                    mFormatSize = 1;
                    break;
                case AV_SAMPLE_FMT_S16:
                case AV_SAMPLE_FMT_S16P:
                    mFormatSize = 2;
                    break;
                case AV_SAMPLE_FMT_S32:
                case AV_SAMPLE_FMT_S32P:
                case AV_SAMPLE_FMT_FLT:
                case AV_SAMPLE_FMT_FLTP:
                    mFormatSize = 4;
                    break;
                case AV_SAMPLE_FMT_DBL:
                case AV_SAMPLE_FMT_DBLP:
                    mFormatSize = 8;
                    break;
                default:
                    mFormatSize = 2;
                    LOGI("unsupported sample format %d", mSampleFormat);
                    break;
                }
                
                mFormatSizeOutput = mFormatSize;
                if(mSampleFormatOutput < AV_SAMPLE_FMT_U8)
                {
                    mSampleFormatOutput = AV_SAMPLE_FMT_U8;
                    mFormatSizeOutput = 1;
                }
                else if(mSampleFormatOutput > AV_SAMPLE_FMT_S16)
                {
                    mSampleFormatOutput = AV_SAMPLE_FMT_S16;
                    mFormatSizeOutput = 2;
                }
                LOGD("mSampleFormatOutput:%d", mSampleFormatOutput);
                LOGD("mFormatSizeOutput:%d", mFormatSizeOutput);
                
                if(mSampleRateOutput < 4000)
                {
                    mSampleRateOutput = 4000;
                }
                else if(mSampleRateOutput > 48000)
                {
                    mSampleRateOutput = 48000;
                }
                LOGD("mSampleRateOutput:%d", mSampleRateOutput);

                if(mChannelsOutput > mDeviceChannels)
                {
                    mChannelLayoutOutput = mDeviceChannelLayoutOutput;
                    mChannelsOutput = mDeviceChannels;
                }
                else if(mChannelsOutput < 1)
                {
                    mChannelLayoutOutput = AV_CH_LAYOUT_MONO;
                    mChannelsOutput = 1;
                }
                LOGD("mChannelLayoutOutput:%lld", mChannelLayoutOutput);
                LOGD("mChannelsOutput:%d", mChannelsOutput);
                
                mConvertCtx = swr_alloc_set_opts(mConvertCtx,
                                    mChannelLayoutOutput,
                                    mSampleFormatOutput,
                                    mSampleRateOutput,
                                    mChannelLayout,
                                    mSampleFormat,
                                    mSampleRate,
                                    0, 0);
                if(swr_init(mConvertCtx) < 0 || mConvertCtx == NULL)
                {
                    LOGE("swr_init failed");
                    return ERROR;
                }
            }
        }
        
    	return AudioTrack_open(mSampleRateOutput, mChannelLayoutOutput, mSampleFormatOutput);
    }

    status_t render(int16_t* buffer, uint32_t buffer_size)
    {
        if(mConvertCtx != NULL && mSamples != NULL)
        {
	        int64_t begin_decode = getNowMs();
            uint32_t sampleCount = buffer_size / mChannels / mFormatSize;
            LOGD("sampleCount:%d", sampleCount);
            int sampleCountOutput = swr_convert(mConvertCtx,
                    (uint8_t**)(&mSamples), (int)sampleCount,
                    (const uint8_t**)(&buffer), (int)sampleCount);
            if(sampleCountOutput > 0)
            {
                buffer = mSamples;
                buffer_size = sampleCountOutput*mChannelsOutput*mFormatSizeOutput;
                LOGD("sampleCountOutput:%d", sampleCountOutput);
                LOGD("buffer_size:%d", buffer_size);
            }
            else
            {
                LOGE("Audio convert %d -> 1 failed", mSampleFormat);
            }
            int64_t end_decode = getNowMs();
        	LOGD("convert audio cost %lld[ms]", (end_decode-begin_decode));
        }
    	int32_t size = AudioTrack_write(buffer, buffer_size);
        if(size <= 0) {
            LOGE("Failed to write audio sample");
            return ERROR;
        }
    	else
    	{
    		LOGD("Write audio sample size:%d", size);
            return OK;
    	}
    }

private:
    struct SwrContext * mConvertCtx;
    int mSampleRate;
    int mSampleRateOutput;
    uint64_t mChannelLayout;
    uint64_t mChannelLayoutOutput;
    int mChannels;
    int mChannelsOutput;
    int mDeviceChannels;
    int mDeviceChannelLayoutOutput;
    AVSampleFormat mSampleFormat;
    AVSampleFormat mSampleFormatOutput;
    uint32_t mFormatSize;
    uint32_t mFormatSizeOutput;

    int16_t* mSamples;
    uint32_t mSamplesSize;
};

AudioPlayer::AudioPlayer()
{
    mDataStream = NULL;
    mDurationMs = 0;
    mOutputBufferingStartMs = 0;
    mAvePacketDurationMs = 0;
    mLastPacketMs = 0;
    mAudioContext = NULL;
    mAudioStreamIndex = -1;
    mSamplesSize = 0;
    mSamples = NULL;
    mListener = NULL;
    mReachEndStream = false;

    mPlayerStatus = MEDIA_PLAYER_INITIALIZED;
    mRunning = false;

    mSeeking = false;
}

AudioPlayer::AudioPlayer(FFStream* dataStream, AVStream* context, int32_t streamIndex)
{
    mDataStream = dataStream;
    if(dataStream)
    {
        mDurationMs = dataStream->getDurationMs();
    }
    else
    {
        mDurationMs = 0;
    }
    mOutputBufferingStartMs = 0;
    mAvePacketDurationMs = 0;
    mLastPacketMs = 0;
    mAudioContext = context;
    mAudioStreamIndex = streamIndex;
    mSamplesSize = AVCODEC_MAX_AUDIO_FRAME_SIZE*2;
    mSamples = (int16_t*)av_malloc(mSamplesSize);
    mListener = NULL;
    mRender = NULL;
    mReachEndStream = false;
    
    pthread_mutex_init(&mLock, NULL);
    pthread_cond_init(&mCondition, NULL);

    mPlayerStatus = MEDIA_PLAYER_INITIALIZED;
    mRunning = false;
    mSeeking = false;
}

AudioPlayer::~AudioPlayer()
{
    stop_l();

    if(mAudioContext != NULL)
    {
        if(mSamples != NULL)
        {
            // Free audio samples buffer
            av_free(mSamples);
            mSamples = NULL;
        }
        
        pthread_mutex_destroy(&mLock);
        pthread_cond_destroy(&mCondition);
    }
    LOGD("AudioPlayer destructor");
}

status_t AudioPlayer::prepare()
{
    if(mPlayerStatus == MEDIA_PLAYER_PREPARED)
        return OK;
    if(mPlayerStatus != MEDIA_PLAYER_INITIALIZED)
        return INVALID_OPERATION;
    
    if(mAudioContext != NULL)
    {
        LOGD("sample rate:%d", mAudioContext->codec->sample_rate);
        LOGD("channel layout:%lld", mAudioContext->codec->channel_layout);
        LOGD("channels:%d", mAudioContext->codec->channels);
        LOGD("sample format:%d", mAudioContext->codec->sample_fmt);
        mRender = new AudioRender();
        uint64_t channelLayout = AV_CH_LAYOUT_MONO;
        switch(mAudioContext->codec->channels)
        {
        case 1:
            channelLayout = AV_CH_LAYOUT_MONO;
            break;
        case 2:
            channelLayout = AV_CH_LAYOUT_STEREO;
            break;
        case 3:
            channelLayout = AV_CH_LAYOUT_2POINT1;
            break;
        case 4:
            channelLayout = AV_CH_LAYOUT_3POINT1;
            break;
        case 5:
            channelLayout = AV_CH_LAYOUT_4POINT1;
            break;
        case 6:
            channelLayout = AV_CH_LAYOUT_5POINT1;
            break;
        case 7:
            channelLayout = AV_CH_LAYOUT_6POINT1;
            break;
        case 8:
            channelLayout = AV_CH_LAYOUT_7POINT1;
            break;
        default:
            channelLayout = AV_CH_LAYOUT_MONO;
            break;
        }
        mRender->open(mAudioContext->codec->sample_rate,
                        channelLayout,//mAudioContext->codec->channel_layout is not accurate for some videos.
                        mAudioContext->codec->channels,
                        mAudioContext->codec->sample_fmt);
        
    	mNumFramesPlayed = 0;
        mPositionTimeMediaMs = 0;
        mAvePacketDurationMs = 0;
        
    	mLatencyMs = AudioTrack_getLatency();
        LOGD("mLatencyMs:%d", mLatencyMs);
    }

    mPlayerStatus = MEDIA_PLAYER_PREPARED;
    return OK;
}

status_t AudioPlayer::start()
{
    if(mPlayerStatus == MEDIA_PLAYER_STARTED)
        return OK;
    if(mPlayerStatus != MEDIA_PLAYER_PREPARED &&
        mPlayerStatus != MEDIA_PLAYER_PAUSED)
        return INVALID_OPERATION;
    
    return start_l();
}

status_t AudioPlayer::stop()
{
    if(mPlayerStatus == MEDIA_PLAYER_STOPPED)
        return OK;
    if(mPlayerStatus != MEDIA_PLAYER_STARTED &&
        mPlayerStatus != MEDIA_PLAYER_PAUSED)
        return INVALID_OPERATION;
    
    return stop_l();
}

status_t AudioPlayer::pause()
{
    if(mPlayerStatus == MEDIA_PLAYER_PAUSED)
        return OK;
    if(mPlayerStatus != MEDIA_PLAYER_STARTED)
        return INVALID_OPERATION;
    
    return pause_l();
}

status_t AudioPlayer::flush()
{
    return flush_l();
}

status_t AudioPlayer::setMediaTimeMs(int64_t timeMs)
{
    mPositionTimeMediaMs = timeMs;
    return OK;
}

int64_t AudioPlayer::getMediaTimeMs()
{
    if(mPlayerStatus != MEDIA_PLAYER_STARTED &&
        mPlayerStatus != MEDIA_PLAYER_PAUSED &&
        mPlayerStatus != MEDIA_PLAYER_PLAYBACK_COMPLETE)
        return 0;

    int64_t audioNowMs = 0;
    if(mAudioContext != NULL)
    {
        int64_t playedDiffMs = 0;
        if(mOutputBufferingStartMs == 0)
        {
            playedDiffMs = 0;
        }
        else
        {
    	    int64_t outputBufferingNowMs = getNowMs();
            playedDiffMs = outputBufferingNowMs - mOutputBufferingStartMs;
            if(playedDiffMs < 0)
            {
                playedDiffMs = 0;
            }
            else if(playedDiffMs > mAvePacketDurationMs)
            {
                playedDiffMs = mAvePacketDurationMs;
            }
        }
        audioNowMs = mPositionTimeMediaMs + playedDiffMs - mLatencyMs;
        //LOGD("diffMs:%lld", diffMs);
        //LOGD("audio time now:%lld", audioNowMs);
    }
    else
    {
        if(mPlayerStatus == MEDIA_PLAYER_STARTED)
            audioNowMs = mPositionTimeMediaMs + (getNowMs() - mOutputBufferingStartMs);
        else
            audioNowMs = mPositionTimeMediaMs;
    }
    return audioNowMs;
}

status_t AudioPlayer::seekTo(int64_t msec)
{
    if(mAudioContext != NULL)
    {
        mPositionTimeMediaMs = msec;
    	mSeeking = true;
    }
    else
    {
        mPositionTimeMediaMs = msec;
        mOutputBufferingStartMs = getNowMs();
    }
    return OK;
}

status_t AudioPlayer::start_l()
{
    if(mAudioContext != NULL)
    {
        if(mPlayerStatus == MEDIA_PLAYER_PREPARED)
        {
            pthread_create(&mThread, NULL, handleThreadStart, this);
        }
        else if(mPlayerStatus == MEDIA_PLAYER_PAUSED)
        {
        	if (AudioTrack_resume() != OK)
            {
                LOGE("AudioTrack_resume failed");
        		return ERROR;
        	}
        }
    }
    else
    {
        mOutputBufferingStartMs = getNowMs();
    }
    
    mPlayerStatus = MEDIA_PLAYER_STARTED;
    return OK;
}

status_t AudioPlayer::stop_l()
{
    if(mAudioContext != NULL)
    {
        mRunning = false;
        pthread_cond_signal(&mCondition);
        
        flush_l();
        if(join_l() != OK)
            return ERROR;

        AudioTrack_close();
        
        if(mRender != NULL)
        {
            delete mRender;
            mRender = NULL;
        }
    }
    mPlayerStatus = MEDIA_PLAYER_STOPPED;
    return OK;
}

status_t AudioPlayer::pause_l()
{
    if(mAudioContext != NULL)
    {
    	if(AudioTrack_pause() != OK)
            return ERROR;
        mPlayerStatus = MEDIA_PLAYER_PAUSED;
        pthread_cond_signal(&mCondition);
    }
    else
    {
        mPositionTimeMediaMs = mPositionTimeMediaMs + (getNowMs() - mOutputBufferingStartMs);
        mPlayerStatus = MEDIA_PLAYER_PAUSED;
    }
    return OK;
}

status_t AudioPlayer::flush_l()
{
    if(mAudioContext != NULL)
    {
        AudioTrack_flush();
    }
    return OK;
}

int32_t AudioPlayer::decode_l(AVPacket *packet)
{
    int32_t size = mSamplesSize;
	int64_t begin_decode = getNowMs();
    //TODO: avcodec_decode_audio4
    int32_t len = avcodec_decode_audio3(mAudioContext->codec, mSamples, &size, packet);
	int64_t end_decode = getNowMs();
        
    if(len >= 0)
    {
	    LOGV("decode audio cost %lld[ms], decoded size:%d", (end_decode-begin_decode), size);
    }
    else
    {
        LOGV("decode audio failed,size:%d", size);
        return 0;
    }

    return size;
}

void AudioPlayer::run()
{
    while(mRunning)
    {
        if(mPlayerStatus == MEDIA_PLAYER_PAUSED)
        {
            int64_t waitUs = 10000ll;//10ms
            struct timespec ts;
            ts.tv_sec = 0;
            ts.tv_nsec = waitUs*1000ll;
            AutoLock autoLock(&mLock);
            int32_t err = pthread_cond_timedwait_relative_np(&mCondition, &mLock, &ts);
            continue;
        }
        else
        {
            AutoLock autoLock(&mLock);
            AVPacket* pPacket = NULL;
            status_t ret = mDataStream->getPacket(mAudioStreamIndex, &pPacket);
            if(ret == FFSTREAM_OK)
            {
                if(!mSeeking)
                {
                    //decoding
                    //TODO: do we need to stop playing if decode failed?
        		    LOGD("decode before");
        	        int32_t size = decode_l(pPacket);
        		    LOGD("decode after");

                    if(size > 0)
                    {
                	    //rendering
            		    LOGD("render before");
                	    render_l(mSamples, size);
                        //update audio output buffering start time
        	            mOutputBufferingStartMs = getNowMs();
            		    LOGD("render after");
                        
                        mAvePacketDurationMs = (mAvePacketDurationMs*4+(pPacket->duration*1000*av_q2d(mAudioContext->time_base)))/5;
                       	mPositionTimeMediaMs = (int64_t)(pPacket->pts*1000*av_q2d(mAudioContext->time_base));
                        LOGD("audio frame pts:%lld", mPositionTimeMediaMs);
                    }
                }
            
    	        av_free_packet(pPacket);
                av_free(pPacket);
                pPacket = NULL;
                continue;
            }
            else if(ret == FFSTREAM_ERROR_FLUSHING)
            {
                mSeeking = false;
                mOutputBufferingStartMs = 0;
                avcodec_flush_buffers(mAudioContext->codec);
                av_free(pPacket);
                pPacket = NULL;
                continue;
            }
    		else if(ret == FFSTREAM_ERROR_BUFFERING)
    		{
		        LOGD("audio queue no data");
                int64_t waitUs = 10000ll;//10ms
                //usleep(waitUs);
                struct timespec ts;
                ts.tv_sec = 0;
                ts.tv_nsec = waitUs*1000ll;
                int32_t err = pthread_cond_timedwait_relative_np(&mCondition, &mLock, &ts);
                continue;
    		}
    		else if(ret == FFSTREAM_ERROR_EOF)
    		{
    		    LOGD("reach audio stream end");
                mReachEndStream = true;
                mPlayerStatus = MEDIA_PLAYER_PLAYBACK_COMPLETE;
                break;
            }
            else
            {
                LOGE("Read audio packet error:%d", ret);
                break;
            }
        }
    }

    LOGD("exit audio thread");
}

status_t AudioPlayer::join_l()
{
    if(mAudioContext != NULL)
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
    return OK;
}

void AudioPlayer::render_l(int16_t* buffer, uint32_t buffer_size)
{
    if(mAudioContext != NULL)
    {
        if(mRender != NULL)
        {
            if(mRender->render(buffer, buffer_size) != OK)
            {
                LOGE("Audio render failed");
            }
        }
        else
        {
            LOGE("Audio render is unavailable");
        }
    }
}

void* AudioPlayer::handleThreadStart(void* ptr)
{    
	if (AudioTrack_start() != OK)
    {
        LOGE("AudioTrack_start failed");
		return 0;
	}
    
#ifdef OS_ANDROID
    LOGD("getpriority before:%d", getpriority(PRIO_PROCESS, 0));
    int audioThreadPriority = -6;
    if(setpriority(PRIO_PROCESS, 0, audioThreadPriority) != 0)
    {
        LOGE("set audio thread priority failed");
    }
    LOGD("getpriority after:%d", getpriority(PRIO_PROCESS, 0));
#endif
    
    AudioPlayer* audioPlayer = (AudioPlayer *) ptr;
    audioPlayer->mRunning = true;
    audioPlayer->run();
    audioPlayer->mRunning = false;
    
    AudioTrack_stop();
    setpriority(PRIO_PROCESS, 0, 0);

    return 0;
}

status_t AudioPlayer::setListener(MediaPlayerListener* listener)
{
    mListener = listener;
    return OK;
}

void AudioPlayer::notifyListener_l(int msg, int ext1, int ext2)
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

