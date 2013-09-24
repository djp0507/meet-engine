#ifndef IPLAYER_H
#define IPLAYER_H

#include "errors.h"

#define CHANNELS_MAX 100
#define LANGCODE_LEN 4
#define LANGTITLE_LEN 20

//视频媒体信息
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
	
	//we do not use dynamic mem alloc, for easy mem management.
	//use the ISO 639 language code (3 letters)
	//for detail, refer to http://en.wikipedia.org/wiki/List_of_ISO_639-1_codes
	//chinese language needs to map this logic:
	//	chi:chinese
	//	zho:chinese
	//	chs:simplified chinese
	//	cht:tranditional chinese
	char audio_languages[CHANNELS_MAX][LANGCODE_LEN];
	char audio_titles[CHANNELS_MAX][LANGTITLE_LEN];

	//all channels count, include audio / video /subtitle
	int32_t channels;

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
        video_channels(0),
        channels(0)
        {}
    
} MediaInfo;

//播放器回调状态分类
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

//播放器回调信息详细分类
enum media_info_type {
    MEDIA_INFO_UNKNOWN = 1,
    MEDIA_INFO_VIDEO_TRACK_LAGGING = 700,
    MEDIA_INFO_BUFFERING_START = 701,
    MEDIA_INFO_BUFFERING_END = 702,
    MEDIA_INFO_NETWORK_BANDWIDTH = 703,
    
    MEDIA_INFO_BAD_INTERLEAVING = 800,
    MEDIA_INFO_NOT_SEEKABLE = 801,
    MEDIA_INFO_METADATA_UPDATE = 802,
};

//播放器错误分类
enum media_error_type {
    MEDIA_ERROR_UNKNOWN = 1,
    MEDIA_ERROR_SERVER_DIED = 100,
    MEDIA_ERROR_NOT_VALID_FOR_PROGRESSIVE_PLAYBACK = 200,
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

class MediaPlayerListener
{
public:
    virtual void notify(int msg, int ext1, int ext2){}
	virtual ~MediaPlayerListener(){}
};

class IPlayer
{
public:
	//播放器初始化接口
	virtual status_t setDataSource(const char *url) = 0;
	virtual status_t setVideoSurface(void* surface) = 0;
	virtual status_t prepare() = 0;
	virtual status_t prepareAsync() = 0;

	//播放器控制接口
	virtual status_t start() = 0;
	virtual status_t stop() = 0;
	virtual status_t pause() = 0;
	virtual status_t seekTo(int msec) = 0;
	virtual status_t selectAudioChannel(int32_t index){return false;} 

	//播放器状态回调接口
	virtual status_t setListener(MediaPlayerListener* listener) = 0;

	//播放器状态获取接口
	virtual status_t getVideoWidth(int *w) = 0;
	virtual status_t getVideoHeight(int *h) = 0;
	virtual status_t getCurrentPosition(int *msec) = 0;
	virtual status_t getDuration(int *msec) = 0;
	virtual bool getMediaInfo(const char* url, MediaInfo* info){return false;} 
	virtual bool getMediaDetailInfo(const char* url, MediaInfo* info){return false;}
	virtual bool getThumbnail(const char* url, MediaInfo* info){return false;}
	virtual int flags() = 0;
	
	virtual ~IPlayer() {}

	//下面是不常用的接口
	
#ifdef OS_ANDROID
	virtual status_t startCompatibilityTest() = 0;
	virtual void stopCompatibilityTest() = 0;
#endif
	virtual status_t startP2PEngine() = 0;
	virtual void stopP2PEngine() = 0;
	virtual status_t reset() = 0;
	virtual status_t setAudioStreamType(int type) = 0;
	virtual status_t setLooping(int loop) = 0;
	virtual bool isLooping() = 0;
	virtual status_t setVolume(float leftVolume, float rightVolume) = 0;
	virtual bool isPlaying() = 0;
	virtual status_t setDataSource(int fd, int64_t offset, int64_t length) = 0;
	virtual void notify(int msg, int ext1, int ext2) = 0;
	virtual void disconnect() = 0;
	virtual status_t suspend() = 0;
	virtual status_t resume() = 0;
};

extern "C" IPlayer* getPlayer(void* context);

#endif
