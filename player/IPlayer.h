#ifndef ANDROID_IPLAYER_H
#define ANDROID_IPLAYER_H

namespace android {

typedef struct MediaInfo {
	int32_t duration_ms; //in millisecond 
	long long size_byte; //in byte
	int32_t width;
	int32_t height;

	const char* format_name;
	const char* audio_name;
	const char* video_name;
	int32_t thumbnail_width;
	int32_t thumbnail_height;
	int32_t* thumbnail;
	int32_t audio_channels;
	int32_t video_channels;

    MediaInfo() :
        duration_ms(0),
        size_byte(0),
        width(0),
        height(0),
        format_name(NULL),
        audio_name(NULL),
        video_name(NULL),
        thumbnail_width(0),
        thumbnail_height(0),
        thumbnail(NULL),
        audio_channels(0),
        video_channels(0)
        {}
    
} MediaInfo;


enum media_event_type {
    MEDIA_NOP               = 0, // interface test message
    MEDIA_PREPARED          = 1,
    MEDIA_PLAYBACK_COMPLETE = 2,
    MEDIA_BUFFERING_UPDATE  = 3,
    MEDIA_SEEK_COMPLETE     = 4,
    MEDIA_SET_VIDEO_SIZE    = 5,
    MEDIA_ERROR             = 100,
    MEDIA_INFO              = 200,
    MEDIA_COMPATIBILITY_TEST_COMPLETE              = 300,
};

// Generic error codes for the media player framework.  Errors are fatal, the
// playback must abort.
//
// Errors are communicated back to the client using the
// MediaPlayerListener::notify method defined below.
// In this situation, 'notify' is invoked with the following:
//   'msg' is set to MEDIA_ERROR.
//   'ext1' should be a value from the enum media_error_type.
//   'ext2' contains an implementation dependant error code to provide
//          more details. Should default to 0 when not used.
//
// The codes are distributed as follow:
//   0xx: Reserved
//   1xx: Android Player errors. Something went wrong inside the MediaPlayer.
//   2xx: Media errors (e.g Codec not supported). There is a problem with the
//        media itself.
//   3xx: Runtime errors. Some extraordinary condition arose making the playback
//        impossible.
//
enum media_error_type {
    // 0xx
    MEDIA_ERROR_UNKNOWN = 1,
    // 1xx
    MEDIA_ERROR_SERVER_DIED = 100,
    // 2xx
    MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK = 200,
    // 3xx
};


// Info and warning codes for the media player framework.  These are non fatal,
// the playback is going on but there might be some user visible issues.
//
// Info and warning messages are communicated back to the client using the
// MediaPlayerListener::notify method defined below.  In this situation,
// 'notify' is invoked with the following:
//   'msg' is set to MEDIA_INFO.
//   'ext1' should be a value from the enum media_info_type.
//   'ext2' contains an implementation dependant info code to provide
//          more details. Should default to 0 when not used.
//
// The codes are distributed as follow:
//   0xx: Reserved
//   7xx: Android Player info/warning (e.g player lagging behind.)
//   8xx: Media info/warning (e.g media badly interleaved.)
//
enum media_info_type {
    // 0xx
    MEDIA_INFO_UNKNOWN = 1,
    // 7xx
    // The video is too complex for the decoder: it can't decode frames fast
    // enough. Possibly only the audio plays fine at this stage.
    MEDIA_INFO_VIDEO_TRACK_LAGGING = 700,
    // MediaPlayer is temporarily pausing playback internally in order to
    // buffer more data.
    MEDIA_INFO_BUFFERING_START = 701,
    // MediaPlayer is resuming playback after filling buffers.
    MEDIA_INFO_BUFFERING_END = 702,
    // Bandwidth in recent past
    MEDIA_INFO_NETWORK_BANDWIDTH = 703,
    
    // 8xx
    // Bad interleaving means that a media has been improperly interleaved or not
    // interleaved at all, e.g has all the video samples first then all the audio
    // ones. Video is playing but a lot of disk seek may be happening.
    MEDIA_INFO_BAD_INTERLEAVING = 800,
    // The media is not seekable (e.g live stream).
    MEDIA_INFO_NOT_SEEKABLE = 801,
    // New media metadata is available.
    MEDIA_INFO_METADATA_UPDATE = 802,
};



enum media_player_states {
    MEDIA_PLAYER_STATE_ERROR        = 0,
    MEDIA_PLAYER_IDLE               = 1 << 0,
    MEDIA_PLAYER_INITIALIZED        = 1 << 1,
    MEDIA_PLAYER_PREPARING          = 1 << 2,
    MEDIA_PLAYER_PREPARED           = 1 << 3,
    MEDIA_PLAYER_STARTED            = 1 << 4,
    MEDIA_PLAYER_PAUSED             = 1 << 5,
    MEDIA_PLAYER_STOPPED            = 1 << 6,
    MEDIA_PLAYER_PLAYBACK_COMPLETE  = 1 << 7
};

// ----------------------------------------------------------------------------
// ref-counted object for callbacks
class MediaPlayerListener
{
public:
    virtual void notify(int msg, int ext1, int ext2){}
	virtual ~MediaPlayerListener(){}
};

class IPlayer
{
public:
	virtual void notify(int msg, int ext1, int ext2) = 0;
	virtual void disconnect() = 0;
	virtual status_t suspend() = 0;
	virtual status_t resume() = 0;
	virtual status_t setDataSource(const char *url) = 0;
	virtual status_t setDataSource(int fd, int64_t offset, int64_t length) = 0;
	virtual status_t setVideoSurface(void* surface) = 0;
	virtual status_t prepare() = 0;
	virtual status_t prepareAsync() = 0;
	virtual status_t start() = 0;
	virtual status_t stop() = 0;
	virtual status_t pause() = 0;
	virtual bool isPlaying() = 0;
	virtual status_t seekTo(int msec) = 0;
	virtual	status_t getVideoWidth(int *w) = 0;
	virtual status_t getVideoHeight(int *h) = 0;
	virtual status_t getCurrentPosition(int *msec) = 0;
	virtual status_t getDuration(int *msec) = 0;
	virtual status_t reset() = 0;
	virtual status_t setAudioStreamType(int type) = 0;
	virtual status_t setLooping(int loop) = 0;
	virtual bool isLooping() = 0;
	virtual status_t setVolume(float leftVolume, float rightVolume) = 0;
	//virtual status_t invoke(const Parcel& request, Parcel *reply) {}
	//virtual status_t setMetadataFilter(const Parcel& filter) {}
	//virtual status_t getMetadata(bool update_only, bool apply_filter, Parcel *metadata) {}
	virtual status_t setListener(MediaPlayerListener* listener) = 0;
	virtual int flags() = 0;
	virtual status_t startCompatibilityTest() = 0;
	virtual void stopCompatibilityTest() = 0;
	
	virtual status_t startP2PEngine() = 0;
	virtual void stopP2PEngine() = 0;
	virtual ~IPlayer() {}
	virtual bool getMediaInfo(const char* url, MediaInfo* info){return false;} 
	virtual bool getMediaDetailInfo(const char* url, MediaInfo* info){return false;}
	virtual bool getThumbnail(const char* url, MediaInfo* info){return false;}
};

} // namesapce android

#endif // ifndef ANDROID_IPLAYER_H
