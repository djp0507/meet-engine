/*
 * Copyright (C) 2012 Roger Shen  rogershen@pptv.com
 *
 */

#define LOG_TAG "AudioTrackWrapper"

#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include "errors.h"
#include "log.h"
#include "audiotrack.h"

#define SIZE_OF_AUDIOTRACK 256

/* From AudioSystem.h */
#define MUSIC 3

enum pcm_sub_format {
    PCM_SUB_16_BIT          = 0x1, // must be 1 for backward compatibility
    PCM_SUB_8_BIT           = 0x2  // must be 2 for backward compatibility
};

enum audio_format {
    PCM                 = 0x00000000, // must be 0 for backward compatibility
    PCM_16_BIT          = (PCM|PCM_SUB_16_BIT),
    PCM_8_BIT           = (PCM|PCM_SUB_8_BIT)
};

enum audio_channels {
    CHANNEL_OUT_FRONT_LEFT            = 0x4,
    CHANNEL_OUT_FRONT_RIGHT           = 0x8,
    CHANNEL_OUT_FRONT_CENTER          = 0x10,
    CHANNEL_OUT_LOW_FREQUENCY         = 0x20,
    CHANNEL_OUT_BACK_LEFT             = 0x40,
    CHANNEL_OUT_BACK_RIGHT            = 0x80,
    CHANNEL_OUT_FRONT_LEFT_OF_CENTER  = 0x100,
    CHANNEL_OUT_FRONT_RIGHT_OF_CENTER = 0x200,
    CHANNEL_OUT_BACK_CENTER           = 0x400,
    CHANNEL_OUT_MONO = CHANNEL_OUT_FRONT_LEFT,
    CHANNEL_OUT_STEREO = (CHANNEL_OUT_FRONT_LEFT | CHANNEL_OUT_FRONT_RIGHT),
    CHANNEL_OUT_QUAD = (CHANNEL_OUT_FRONT_LEFT | CHANNEL_OUT_FRONT_RIGHT |
            CHANNEL_OUT_BACK_LEFT | CHANNEL_OUT_BACK_RIGHT),
    CHANNEL_OUT_SURROUND = (CHANNEL_OUT_FRONT_LEFT | CHANNEL_OUT_FRONT_RIGHT |
            CHANNEL_OUT_FRONT_CENTER | CHANNEL_OUT_BACK_CENTER),
    CHANNEL_OUT_5POINT1 = (CHANNEL_OUT_FRONT_LEFT | CHANNEL_OUT_FRONT_RIGHT |
            CHANNEL_OUT_FRONT_CENTER | CHANNEL_OUT_LOW_FREQUENCY | CHANNEL_OUT_BACK_LEFT | CHANNEL_OUT_BACK_RIGHT),
    CHANNEL_OUT_7POINT1 = (CHANNEL_OUT_FRONT_LEFT | CHANNEL_OUT_FRONT_RIGHT |
            CHANNEL_OUT_FRONT_CENTER | CHANNEL_OUT_LOW_FREQUENCY | CHANNEL_OUT_BACK_LEFT | CHANNEL_OUT_BACK_RIGHT |
            CHANNEL_OUT_FRONT_LEFT_OF_CENTER | CHANNEL_OUT_FRONT_RIGHT_OF_CENTER),
    CHANNEL_OUT_ALL = (CHANNEL_OUT_FRONT_LEFT | CHANNEL_OUT_FRONT_RIGHT |
            CHANNEL_OUT_FRONT_CENTER | CHANNEL_OUT_LOW_FREQUENCY | CHANNEL_OUT_BACK_LEFT | CHANNEL_OUT_BACK_RIGHT |
            CHANNEL_OUT_FRONT_LEFT_OF_CENTER | CHANNEL_OUT_FRONT_RIGHT_OF_CENTER | CHANNEL_OUT_BACK_CENTER),
};

// _ZN7android11AudioSystem19getOutputFrameCountEPii
typedef int (*AUDIOSYSTEM_getOutputFrameCount)(int *, int);
// _ZN7android11AudioSystem16getOutputLatencyEPji
typedef int (*AUDIOSYSTEM_getOutputLatency)(unsigned int *, int);
// _ZN7android11AudioSystem21getOutputSamplingRateEPii
typedef int (*AUDIOSYSTEM_getOutputSamplingRate)(int *, int);

// _ZN7android10AudioTrack16getMinFrameCountEPiij
typedef int (*AUDIOTRACK_getMinFrameCount)(int *, int, unsigned int);

// _ZN7android10AudioTrackC1EijiiijPFviPvS1_ES1_ii
typedef void (*AUDIOTRACK_ctor)(void *, int, unsigned int, int, int, int, unsigned int, void (*)(int, void *, void *), void *, int, int);
// _ZN7android10AudioTrackC1EijiiijPFviPvS1_ES1_i
typedef void (*AUDIOTRACK_ctor_legacy)(void *, int, unsigned int, int, int, int, unsigned int, void (*)(int, void *, void *), void *, int);
// _ZN7android10AudioTrackD1Ev
typedef void (*AUDIOTRACK_dtor)(void *);
// _ZNK7android10AudioTrack9initCheckEv
typedef int (*AUDIOTRACK_initCheck)(void *);
// _ZN7android10AudioTrack5startEv
typedef int (*AUDIOTRACK_start)(void *);
// _ZN7android10AudioTrack4stopEv
typedef int (*AUDIOTRACK_stop)(void *);
// _ZN7android10AudioTrack5writeEPKvj
typedef int (*AUDIOTRACK_write)(void *, void  const*, unsigned int);
// _ZN7android10AudioTrack5flushEv
typedef int (*AUDIOTRACK_flush)(void *);
// _ZN7android10AudioTrack5pauseEv
typedef int (*AUDIOTRACK_pause)(void *);
// _ZNK7android10AudioTrack7latencyEv
typedef uint32_t (*AUDIOTRACK_latency)(void *);

static void* libmedia = NULL;
static void* AudioTrack = NULL;

static AUDIOSYSTEM_getOutputFrameCount as_getOutputFrameCount = NULL;
static AUDIOSYSTEM_getOutputLatency as_getOutputLatency = NULL;
static AUDIOSYSTEM_getOutputSamplingRate as_getOutputSamplingRate = NULL;

static AUDIOTRACK_getMinFrameCount at_getMinFrameCount = NULL;
static AUDIOTRACK_ctor at_ctor = NULL;
static AUDIOTRACK_ctor_legacy at_ctor_legacy = NULL;
static AUDIOTRACK_dtor at_dtor = NULL;
static AUDIOTRACK_initCheck at_initCheck = NULL;
static AUDIOTRACK_start at_start = NULL;
static AUDIOTRACK_stop at_stop = NULL;
static AUDIOTRACK_write at_write = NULL;
static AUDIOTRACK_flush at_flush = NULL;
static AUDIOTRACK_pause at_pause = NULL;
static AUDIOTRACK_latency at_latency = NULL;

static status_t InitLibrary()
{
    /* DL Open libmedia */
    libmedia = dlopen("libmedia.so", RTLD_NOW|RTLD_LOCAL);
    if (!libmedia)
        return ERROR;

    /* Register symbols */
    as_getOutputFrameCount = (AUDIOSYSTEM_getOutputFrameCount)(dlsym(libmedia, "_ZN7android11AudioSystem19getOutputFrameCountEPii"));
    as_getOutputLatency = (AUDIOSYSTEM_getOutputLatency)(dlsym(libmedia, "_ZN7android11AudioSystem16getOutputLatencyEPji"));
    if(as_getOutputLatency == NULL) {
        /* 4.1 Jellybean prototype */
        as_getOutputLatency = (AUDIOSYSTEM_getOutputLatency)(dlsym(libmedia, "_ZN7android11AudioSystem16getOutputLatencyEPj19audio_stream_type_t"));
    }
    as_getOutputSamplingRate = (AUDIOSYSTEM_getOutputSamplingRate)(dlsym(libmedia, "_ZN7android11AudioSystem21getOutputSamplingRateEPii"));
    at_getMinFrameCount = (AUDIOTRACK_getMinFrameCount)(dlsym(libmedia, "_ZN7android10AudioTrack16getMinFrameCountEPiij"));
    if(at_getMinFrameCount == NULL) {
        /* 4.1 Jellybean prototype */
        at_getMinFrameCount = (AUDIOTRACK_getMinFrameCount)(dlsym(libmedia, "_ZN7android10AudioTrack16getMinFrameCountEPi19audio_stream_type_tj"));
    }
    at_ctor = (AUDIOTRACK_ctor)(dlsym(libmedia, "_ZN7android10AudioTrackC1EijiiijPFviPvS1_ES1_ii"));
    at_ctor_legacy = (AUDIOTRACK_ctor_legacy)(dlsym(libmedia, "_ZN7android10AudioTrackC1EijiiijPFviPvS1_ES1_i"));
    at_dtor = (AUDIOTRACK_dtor)(dlsym(libmedia, "_ZN7android10AudioTrackD1Ev"));
    at_initCheck = (AUDIOTRACK_initCheck)(dlsym(libmedia, "_ZNK7android10AudioTrack9initCheckEv"));
    at_start = (AUDIOTRACK_start)(dlsym(libmedia, "_ZN7android10AudioTrack5startEv"));
    at_stop = (AUDIOTRACK_stop)(dlsym(libmedia, "_ZN7android10AudioTrack4stopEv"));
    at_write = (AUDIOTRACK_write)(dlsym(libmedia, "_ZN7android10AudioTrack5writeEPKvj"));
    at_flush = (AUDIOTRACK_flush)(dlsym(libmedia, "_ZN7android10AudioTrack5flushEv"));
    at_pause = (AUDIOTRACK_pause)(dlsym(libmedia, "_ZN7android10AudioTrack5pauseEv"));
    at_latency = (AUDIOTRACK_latency)(dlsym(libmedia, "_ZNK7android10AudioTrack7latencyEv"));

    /* We need the first 3 or the last 1 */
    if (!((as_getOutputFrameCount && as_getOutputLatency && as_getOutputSamplingRate)
        || at_getMinFrameCount)) {
        dlclose(libmedia);
        return ERROR;
    }

    // We need all the other Symbols
    if (!((at_ctor || at_ctor_legacy) && at_dtor && at_initCheck &&
           at_start && at_stop && at_write && at_flush)) {
        dlclose(libmedia);
        return ERROR;
    }
    return OK;
}

status_t AudioTrack_open(int sampleRate, uint64_t channelLayout, AVSampleFormat sampleFormat)
{
    if(InitLibrary() != OK)
    {
        LOGE("InitLibrary failed");
        return ERROR;
    }

    int32_t rate = sampleRate;

    int32_t channel = CHANNEL_OUT_STEREO;
    switch(channelLayout)
    {
    case AV_CH_LAYOUT_MONO:
        channel = CHANNEL_OUT_MONO;
        break;
    case AV_CH_LAYOUT_STEREO:
        channel = CHANNEL_OUT_STEREO;
        break;
    case AV_CH_LAYOUT_2POINT1:
    case AV_CH_LAYOUT_2_1:
    case AV_CH_LAYOUT_SURROUND:
    case AV_CH_LAYOUT_3POINT1:
    case AV_CH_LAYOUT_4POINT0:
    case AV_CH_LAYOUT_2_2:
    case AV_CH_LAYOUT_QUAD:
        channel = CHANNEL_OUT_QUAD;
        break;
    case AV_CH_LAYOUT_4POINT1:
    case AV_CH_LAYOUT_5POINT0:
    case AV_CH_LAYOUT_5POINT1:
    case AV_CH_LAYOUT_5POINT0_BACK:
    case AV_CH_LAYOUT_5POINT1_BACK:
    case AV_CH_LAYOUT_6POINT0:
        channel = CHANNEL_OUT_5POINT1;
        break;
    case AV_CH_LAYOUT_6POINT0_FRONT:
    case AV_CH_LAYOUT_HEXAGONAL:
    case AV_CH_LAYOUT_6POINT1:
    case AV_CH_LAYOUT_6POINT1_BACK:
    case AV_CH_LAYOUT_6POINT1_FRONT:
    case AV_CH_LAYOUT_7POINT0:
    case AV_CH_LAYOUT_7POINT0_FRONT:
    case AV_CH_LAYOUT_7POINT1:
        channel = CHANNEL_OUT_7POINT1;
        break;
    case AV_CH_LAYOUT_7POINT1_WIDE:
    case AV_CH_LAYOUT_7POINT1_WIDE_BACK:
    case AV_CH_LAYOUT_OCTAGONAL:
    case AV_CH_LAYOUT_STEREO_DOWNMIX:
        channel = CHANNEL_OUT_ALL;
        break;
    default:
        channel = CHANNEL_OUT_STEREO;
        break;
    }

    LOGD("audio output channel: %d", channel);

    int32_t format = 0;
    switch(sampleFormat)
    {
    case AV_SAMPLE_FMT_U8:
        format = PCM_8_BIT;
        break;
    case AV_SAMPLE_FMT_S16:
        format = PCM_16_BIT;
        break;
    default:
        format = PCM_16_BIT;
        break;
    }
    LOGD("audio output format: %d", format);
    
    int32_t stream_type = MUSIC;

    /* Get the minimum buffer value */
    int32_t status, size;
    int32_t afSampleRate, afFrameCount, afLatency, minBufCount, minFrameCount;
    if (!at_getMinFrameCount) {
        status = as_getOutputSamplingRate(&afSampleRate, stream_type);
        status ^= as_getOutputFrameCount(&afFrameCount, stream_type);
        status ^= as_getOutputLatency((uint32_t*)(&afLatency), stream_type);
        if (status != 0) {
            LOGE("Could not query the AudioStream parameters");
            return ERROR;
        }
        minBufCount = afLatency / ((1000 * afFrameCount) / afSampleRate);
        if (minBufCount < 2)
            minBufCount = 2;
        minFrameCount = (afFrameCount * rate * minBufCount) / afSampleRate;
    }
    else {
        status = at_getMinFrameCount(&minFrameCount, stream_type, rate);
        if (status != 0) {
            LOGE("Could not query the AudioTrack parameters");
            return ERROR;
        }
        LOGD("minFrameCount: %d", minFrameCount);
    }

    size = minFrameCount * (channel >= CHANNEL_OUT_STEREO ? 2 : 1);// * 4;

    /* Sizeof(AudioTrack) == 0x58 (not sure) on 2.2.1, this should be enough */
    AudioTrack = malloc(SIZE_OF_AUDIOTRACK);
    if (!AudioTrack)
        return ERROR;

    *((uint32_t *) ((uint32_t)AudioTrack + SIZE_OF_AUDIOTRACK - 4)) = 0xbaadbaad;
    // Higher than android 2.2
    if (at_ctor)
        at_ctor(AudioTrack, stream_type, rate, format, channel, size, 0, NULL, NULL, 0, 0);
    // Higher than android 1.6
    else if (at_ctor_legacy)
        at_ctor_legacy(AudioTrack, stream_type, rate, format, channel, size, 0, NULL, NULL, 0);

    if( (*((uint32_t *) ((uint32_t)AudioTrack + SIZE_OF_AUDIOTRACK - 4)) != 0xbaadbaad) )
    {
        LOGE("AudioTrack ctor failed.");
        free(AudioTrack);
        return ERROR;
    }
    
    /* And Init */
    status = at_initCheck(AudioTrack);

    /* android 1.6 uses channel count instead of stream_type */
    if (status != 0) {
        channel = (channel == CHANNEL_OUT_STEREO) ? 2 : 1;
        at_ctor_legacy(AudioTrack, stream_type, rate, format, channel, size, 0, NULL, NULL, 0);
        status = at_initCheck(AudioTrack);
    }
    if (status != 0) {
        LOGE("Cannot create AudioTrack!");
        free(AudioTrack);
        return ERROR;
    }
	return OK;
}

status_t AudioTrack_registerCallback(ReadCallback callback)
{
    return OK;
}

status_t AudioTrack_start()
{
    at_start(AudioTrack);
    return OK;
}

status_t AudioTrack_resume()
{
    at_start(AudioTrack);
    return OK;
}

status_t AudioTrack_flush()
{
    at_flush(AudioTrack);
    return OK;
}

status_t AudioTrack_stop()
{
    at_stop(AudioTrack);
    return OK;
}

status_t AudioTrack_pause()
{
    at_pause(AudioTrack);
    return OK;
}

int32_t AudioTrack_write(void *buffer, uint32_t buffer_size)
{
    size_t length = 0;
    while (length < buffer_size) {
        length += at_write(AudioTrack, (char*)(buffer) + length, buffer_size - length);
    }
    return length;
}

uint32_t AudioTrack_getFrameSize()
{
    return 0;
}

uint32_t AudioTrack_getLatency()
{
    if(at_latency)
    {
        return at_latency(AudioTrack);
    }
    else
    {
        LOGE("failed to get latency");
    }
    return 0;
}

status_t AudioTrack_close()
{
    at_flush(AudioTrack);
    at_dtor(AudioTrack);
    free(AudioTrack);
    dlclose(libmedia);
    return OK;
}

