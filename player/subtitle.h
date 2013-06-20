#ifndef SUBTITLE_H
#define SUBTITLE_H

typedef enum
{
	TEXT,
	RGB,
	YUV,
}SubtitleFormat;

typedef enum
{
	CHS,
	CHT,
	ENG,
}SubtitleLang;
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



typedef struct
{
    int64_t start_ms;
    int64_t stop_ms;
	SubtitleFormat format;
    void* data;
} Subtitle;

bool set_subtitle_path(const char* path, int lang);
bool decode_subtitle(Subtitle* data);
bool seek(int64_t time);


#endif
