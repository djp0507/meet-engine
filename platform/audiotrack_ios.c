/*
 * Copyright (C) 2012 Roger Shen  rogershen@pptv.com
 *
 */

#import <OpenAL/al.h>
#import <OpenAL/alc.h>

#include "errors.h"
#include "log.h"
#include "audiotrack.h"

#define kDefaultDistance 25.0
typedef ALvoid AL_APIENTRY(*alBufferDataStaticProcPtr) (const ALint bid, ALenum format, ALvoid* data, ALsizei size, ALsizei freq);


struct oalContext
{
	ALCdevice* device;
	ALCcontext* context;
	ALuint buffer;
	ALuint source;
	ALenum format;
	ALsizei freq;
	alBufferDataStaticProcPtr write;
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


status_t AudioTrack_open(int sampleRate, uint64_t channelLayout, AVSampleFormat sampleFormat)
{
	alContext.device = NULL;
	alContext.context = NULL;
	alContext.write = NULL;
	
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
			alGenBuffers(1, &alContext.buffer);
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
			
		}
	}
	// clear any errors
	alGetError();

	alContext.write = (alBufferDataStaticProcPtr) alcGetProcAddress(NULL, (const ALCchar*) "alBufferDataStatic");

	// attach OpenAL Buffer to OpenAL Source
	alSourcei(alContext.source, AL_BUFFER, alContext.buffer);
	if((error = alGetError()) != AL_NO_ERROR)
	{
		LOGE("Error attaching buffer to source: %x\n", error);
		return ERROR;
	}
	
	return OK;
}

status_t AudioTrack_start()
{
	ALenum error;
	
	// Begin playing our source file
	alSourcePlay(alContext.source);
	if((error = alGetError()) != AL_NO_ERROR) {
		LOGE("error starting source: %x\n", error);
	}
    return OK;
}

status_t AudioTrack_resume()
{
    return OK;
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
	if(alContext.write != NULL)
	{
		alContext.write(alContext.buffer,
			alContext.format,
			(ALvoid*)buffer,
			(ALsizei)buffer_size,
			alContext.freq);

		error = alGetError();
		if(error == AL_OUT_OF_MEMORY)
		{
			LOGE("There is not enough memory available to create this buffer: %x\n", error);
		}
		else if(error == AL_INVALID_VALUE)
		{
			LOGE("The size parameter is not valid for the format specified, the buffer is in use, or the data is a NULL pointer: %x\n", error);
		} 
		else if(error == AL_INVALID_ENUM)
		{
			LOGE("The specified format does not exist: %x\n", error);
		} 
		else
		{
			return buffer_size;
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
    alDeleteBuffers(1, &alContext.buffer);

	if(alContext.context != NULL)
	{
	    //Release context
	    alcDestroyContext(alContext.context);
	}
	if(alContext.device != NULL)
	{
	    //Close device
	    alcCloseDevice(alContext.device);
	}
    return OK;
}

