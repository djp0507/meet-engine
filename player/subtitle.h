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
//chs ��������
//cht ��������
//dan ������
//deu ����
//eng ����Ӣ��
//enu Ӣ��
//esp ��������
//fin ������
//fra ���ģ���׼��
//frc ���ô���
//ita �������
//jpn ����
//kor ����
//nld ������
//nor Ų����
//plk ������
//ptb ������������
//ptg ��������
//rus ����
//sve �����
//tha ̩��



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
