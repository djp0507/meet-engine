#ifndef SUBTITLE_H
#define SUBTITLE_H

#ifdef _MSC_VER
    typedef __int64          int64_t;
    typedef unsigned __int64 uint64_t;
#else
    #include <stdint.h>
#endif

typedef enum
{
    TEXT, // UTF-8
    RGB,
    YUV,
}SubtitleFormat;

//chs 简体中文
//cht 繁体中文
//dan 丹麦文
//deu 德文
//eng 国际英文
//enu 英文
//esp 西班牙文
//fin 芬兰文
//fra 法文（标准）
//frc 加拿大法文
//ita 意大利文
//jpn 日文
//kor 韩文
//nld 荷兰文
//nor 挪威文
//plk 波兰文
//ptb 巴西葡萄牙文
//ptg 葡萄牙文
//rus 俄文
//sve 瑞典文
//tha 泰文

/**
 * 对应一个字幕，独立渲染单元
 */
struct STSEntity
{
public:
    int64_t start_ms;      // 字幕开始时间
    int64_t stop_ms;       // 字幕结束时间
    SubtitleFormat format; // 字幕格式
    union {
        const char* text;   // UTF-8
        void* data;
    };
};

/**
 * 相同渲染时间的entity组合成一个segment
 */
class STSSegment
{
public:
    /**
     * 获取字幕段的开始时间
     * @return  单位：毫秒
     */
    virtual int64_t getStartTime() = 0;

    /**
     * 获取字幕段的结束时间
     * @return  单位：毫秒
     */
    virtual int64_t getStopTime() = 0;

    /*
     * 获取字幕的数量
     */
    virtual int getEntryCount() = 0;

    /**
     * 获取字幕entity
     * @param[in] index   从0开始的索引
     */
    virtual STSEntity* getEntry(int index) = 0;

    /*
     * 获取segment的字幕文本
     * @param[out] text       接收字幕文本缓冲区. 如果缓冲区比较小，不会修改缓冲区内容
     * @param[in]  maxLength  text缓冲区长度，包括结尾的null
     * @return:  拷贝到text中的字节数, 0 表示缓冲区太小
     * @remark 为了简化字幕渲染，不对字幕进行分区域渲染，所以提供了该接口
     *         把所有entity的text用换行符组合在一起,便于前端展示
     *         text 可以为NULL, 直接返回缓冲区的的大小
     */
    virtual int getSubtitleText(char* text, int maxLength) = 0;
    /*
     * 获取字幕图片
     *     尽快使用图片内存,ISubtitles会复用*bitmapData指向的内存
     */
    virtual int getSubtitleImage(int *width, int *height, void** bitmapData);
};

class ISubtitles
{
public:
    static bool create(ISubtitles** subtitle);
public:
    /*
     * 关闭字幕组件
     */
    virtual void close() = 0;
    /**
     * 获取当前字幕的个数
     */
    virtual int getLanguageCount() = 0;
    /**
     * 获取字幕语言的名称
     * @param[in]   language 语言索引,从0开始
     * @param[out]  name     字幕语言 utf-8编码,512字节
    */
    virtual bool getLanguageName(int language, char* name) = 0;
    /**
     * 获取字幕语言的代码,例如：chs,cht etc.
     * @param[in]   language 语言索引,从0开始
     * @param[out]  name     字幕语言 utf-8编码,512字节
    */
    virtual bool getLanguageCode(int language, char* code) = 0;
    /**
     * 获取字幕属性
     * @param[in]   language 语言索引,从0开始
     * @return:     位标记属性
     *              0x0001  媒体文件内嵌字幕
     *              0x0002  外挂字幕
     */
    virtual int getLanguageFlags(int language) = 0;
    /**
     * 获取当前选择字幕语言
     * @param[out]  selected
    */
    virtual bool getSelectedLanguage(int* selected) = 0;

    /**
     * 设置字幕语言
     * @param[in]  selected
    */
    virtual bool setSelectedLanguage(int selected) = 0;

    /**
     * 获取时间点对应的字幕段
     * @param[in]  time     时间点,单位: 毫秒
     * @param[out] segment  时间点对应的字幕
     */
    virtual bool getSubtitleSegment(int64_t time, STSSegment** segment) = 0;

    /**
     * 与 getNextSubtitleSegment 配合使用，设置时间点到 time
     * @param[in]  time     时间点，单位：毫秒
     */
    virtual bool seekTo(int64_t time) = 0;
    /**
     * 获取下一个字幕段
     */
    virtual bool getNextSubtitleSegment(STSSegment** segment) = 0;
    /**
     * 加载字幕，添加到字幕列表中
     * @param[in] fileName     字幕文件路径
     * @param[in] isMediaFile  文件是否媒体文件,如果为true,使用ffmpeg解出内嵌字幕,
     *                         并且尝试加载同目录下的同名的字幕文件
    */
    virtual bool loadSubtitle(const char* fileName, bool isMediaFile) = 0;

    /**
     * 根据文件名获取字幕语言索引
     * @param[in]  fileName  字幕文件路径
     * @return:  从0开始的语言索引  -1 表示不存在该字幕
     */
    virtual int getSubtitleIndex(const char* fileName) = 0;
};

#endif
