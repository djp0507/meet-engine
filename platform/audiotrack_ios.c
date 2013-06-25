/*
 * Copyright (C) 2012 Roger Shen  rogershen@pptv.com
 *
 */

#import <OpenAL/al.h>
#import <OpenAL/alc.h>
#import <unistd.h>
//#import <Foundation/NSObjCRuntime.h>

#include "errors.h"
#include "log.h"
#include "audiotrack.h"

#define kDefaultDistance 25.0
#define NUM_BUFFERS 3
#define BUFFER_SIZE 10240


struct oalContext
{
	ALCdevice* device;
	ALCcontext* context;
	ALuint buffers[NUM_BUFFERS];
	ALuint source;
	ALenum format;
	ALsizei freq;
/*
	void*					data;
	CGPoint					sourcePos;
	CGPoint					listenerPos;
	CGFloat					listenerRotation;
	ALfloat					sourceVolume;
	BOOL					isPlaying;
	BOOL					wasInterrupted;
	UInt32					iPodIsPlaying;
*/

};

static struct oalContext alContext;


status_t AudioTrack_open(int sampleRate, uint64_t channelLayout, enum AVSampleFormat sampleFormat)
{
	alContext.device = NULL;
	alContext.context = NULL;
	
	ALenum error;
	
	switch(sampleFormat)
	{
	case AV_SAMPLE_FMT_U8:
		if(channelLayout == AV_CH_LAYOUT_MONO)
		{
			alContext.format = AL_FORMAT_MONO8;
		}
		else
		{
			alContext.format = AL_FORMAT_STEREO8;
		}
		break;
	case AV_SAMPLE_FMT_S16:
		if(channelLayout == AV_CH_LAYOUT_MONO)
		{
			alContext.format = AL_FORMAT_MONO16 ;
		}
		else
		{
			alContext.format = AL_FORMAT_STEREO16;
		}
		break;
	default:
		alContext.format = AL_FORMAT_STEREO16;
		break;
	}

	alContext.freq = sampleRate;

	// Create a new OpenAL Device
	// Pass NULL to specify the system default output device
	alContext.device = alcOpenDevice(NULL);
	if (alContext.device != NULL)
	{
		// Create a new OpenAL Context
		// The new context will render to the OpenAL Device just created 
		alContext.context = alcCreateContext(alContext.device, 0);
		if (alContext.context != NULL)
		{
			// Make the new context the Current OpenAL Context
			alcMakeContextCurrent(alContext.context);
			
			// Create some OpenAL Buffer Objects
			alGenBuffers(NUM_BUFFERS, alContext.buffers);
			if((error = alGetError()) != AL_NO_ERROR) {
				LOGE("Error Generating Buffers: %x", error);
				return ERROR;
			}
			
			// Create some OpenAL Source Objects
			alGenSources(1, &alContext.source);
			if(alGetError() != AL_NO_ERROR) 
			{
				LOGE("Error generating sources! %x\n", error);
				return ERROR;
			}
            //alSourcei(alContext.source, AL_LOOPING, AL_FALSE);
            //alSourcef(alContext.source, AL_SOURCE_TYPE, AL_STREAMING);
			
		}
	}
	// clear any errors
	alGetError();

	//alContext.write = (alBufferDataStaticProcPtr) alcGetProcAddress(NULL, (const ALCchar*) "alBufferDataStatic");
    
    // Set Source Position
	//float sourcePosAL[] = {0.0f, kDefaultDistance, 0.0f};
	//alSourcefv(alContext.source, AL_POSITION, sourcePosAL);
    
	// Set Source Reference Distance
	//alSourcef(alContext.source, AL_REFERENCE_DISTANCE, 50.0f);

	// attach OpenAL Buffer to OpenAL Source
	//alSourcei(alContext.source, AL_BUFFER, alContext.buffer);
	//if((error = alGetError()) != AL_NO_ERROR)
	//{
	//	LOGE("Error attaching buffer to source: %x\n", error);
    //    NSLog(@"Error attaching buffer to source: %x\n", error);
	//	return ERROR;
	//}
	
	return OK;
}

status_t AudioTrack_start()
{
	ALenum error;
	
	// Begin playing our source file
	alSourcePlay(alContext.source);
	if((error = alGetError()) != AL_NO_ERROR) {
		LOGE("error starting source: %x\n", error);
        //NSLog(@"error starting source: %x\n", error);
	}

    return OK;
}

status_t AudioTrack_resume()
{
    return AudioTrack_start();
}

status_t AudioTrack_flush()
{
    return OK;
}

status_t AudioTrack_stop()
{
	ALenum error;
	
	// Stop playing our source file
	alSourceStop(alContext.source);
	if((error = alGetError()) != AL_NO_ERROR) {
		LOGE("error stopping source: %x\n", error);
		//NSLog(@"error stopping source: %x\n", error);
	} 
    return OK;
}

status_t AudioTrack_pause()
{
	ALenum error;
	
	// Pause playing our source file
	alSourcePause(alContext.source);
	if((error = alGetError()) != AL_NO_ERROR) {
		LOGE("error pausing source: %x\n", error);
	} 
    return OK;
}

int32_t AudioTrack_write(void *buffer, uint32_t buffer_size)
{
	ALenum error;
    
    ALint state;
    alGetSourcei(alContext.source, AL_SOURCE_STATE, &state);
    if (state == AL_INITIAL)
    {
        ALint queued;
        alGetSourcei(alContext.source,
                     AL_BUFFERS_QUEUED,
                     &queued);
        if (queued == NUM_BUFFERS)
        {
            alSourcePlay(alContext.source);
            error = alGetError();
            if (error != AL_NO_ERROR)
            {
                //NSLog(@"error playing data: %x\n", error);
                return 0;
            }
            return buffer_size;
        }
        else if (queued < NUM_BUFFERS && queued >= 0)
        {
            alBufferData(alContext.buffers[queued],
                         alContext.format,
                         buffer,
                         buffer_size,
                         alContext.freq);
            error = alGetError();
            if (error != AL_NO_ERROR)
            {
                //NSLog(@"error loading buffer data: %x\n", error);
                return 0;
            }
            
            alSourceQueueBuffers(alContext.source, 1, &(alContext.buffers[queued]));
            error = alGetError();
            if (error != AL_NO_ERROR)
            {
                //NSLog(@"error queue buffer data: %x\n", error);
                return 0;
            }
            return buffer_size;
        }
        else
        {
            //NSLog(@"invalid buffer index: %d\n", queued);
            return 0;
        }

    }
    else if (state != AL_PAUSED)
    {
        while (state != AL_PAUSED)
        {
            ALint processed;
            alGetSourcei(alContext.source,
                         AL_BUFFERS_PROCESSED,
                         &processed);
            if (processed > 0)
            {
                ALuint bufferUnqueued;
                alSourceUnqueueBuffers(alContext.source,
                                       1,
                                       &bufferUnqueued);
                
                error = alGetError();
                if (error != AL_NO_ERROR)
                {
                    //NSLog(@"error unqueue buffer data: %x\n", error);
                    return 0;
                }
                
                //TODO:check buffer_size range
                alBufferData(bufferUnqueued,
                             alContext.format,
                             buffer,
                             buffer_size,
                             alContext.freq);
                
                error = alGetError();
                if (error != AL_NO_ERROR)
                {
                    //NSLog(@"error queue buffer data: %x\n", error);
                    return 0;
                }
                
                alSourceQueueBuffers(alContext.source, 1, &bufferUnqueued);
                
                alGetSourcei(alContext.source, AL_SOURCE_STATE, &state);
                if(state != AL_PLAYING)
                {
                    alSourcePlay(alContext.source);
                }
                return buffer_size;
            }
            usleep(1000ll);//1ms
        }
    }
    
    return 0;
}

uint32_t AudioTrack_getLatency()
{
    return 0;
}

status_t AudioTrack_close()
{
	// Delete the Sources
    alDeleteSources(1, &alContext.source);
	// Delete the Buffers
    alDeleteBuffers(NUM_BUFFERS, alContext.buffers);

	if(alContext.context != NULL)
	{
	    //Release context
        alcMakeContextCurrent(NULL);
	    alcDestroyContext(alContext.context);
	}
	if(alContext.device != NULL)
	{
	    //Close device
	    alcCloseDevice(alContext.device);
	}
    return OK;
}

