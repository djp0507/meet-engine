// subtitle.cpp : Defines the entry point for the console application.
//
#include <stdio.h>
#include <string.h>
#include <map>
#include <vector>
#include <list>
#include "subtitle.h"

extern "C"
{
#include "libass/ass.h"
};
// forward declared
class CSubtitleManager;
class CSimpleTextSubtitle;
class CSTSSegment;

#include "simpletextsubtitle.h"

class CSubtitleManager :
    public ISubtitles
{
public:
    CSubtitleManager();
    virtual ~CSubtitleManager();

    // ISubtitles
public:
    virtual void close();
    virtual int  getLanguageCount();
    virtual bool getLanguageName(int language, char* name);
    virtual bool getLanguageCode(int language, char* code);
    virtual int  getLanguageFlags(int language);
    virtual bool getSelectedLanguage(int* selected);
    virtual bool setSelectedLanguage(int selected);
    virtual bool getSubtitleSegment(int64_t time, STSSegment** segment);
    virtual bool seekTo(int64_t time);
    virtual bool getNextSubtitleSegment(STSSegment** segment);
    virtual bool loadSubtitle(const char* fileName, bool isMediaFile);
    virtual int  getSubtitleIndex(const char* fileName);

protected:
    CSimpleTextSubtitle* getSelectedSimpleTextSubtitle()
    {
        if (mSelected >= 0 && mSelected < mSubtitles.size()) {
            return mSubtitles[mSelected];
        }
        return NULL;
    }

    ASS_Library*  mAssLibrary;
    std::vector<CSimpleTextSubtitle*>  mSubtitles;
    int           mSelected;
};

CSubtitleManager::CSubtitleManager()
{
    mAssLibrary = ass_library_init();
    mSelected = 0;
}

CSubtitleManager::~CSubtitleManager()
{
    if (mAssLibrary) {
        ass_library_done(mAssLibrary);
    }
    std::vector<CSimpleTextSubtitle*>::iterator itr = mSubtitles.begin();
    for (; itr != mSubtitles.end(); ++itr) {
        delete *itr;
    }
    mSubtitles.clear();
}

void CSubtitleManager::close() 
{
    delete this;
}

int  CSubtitleManager::getLanguageCount()
{
    return mSubtitles.size();
}

bool CSubtitleManager::getLanguageName(int language, char* name)
{
    return false;
}

bool CSubtitleManager::getLanguageCode(int language, char* code)
{
    return false;
}

int  CSubtitleManager::getLanguageFlags(int language)
{
    return 0;
}

bool CSubtitleManager::getSelectedLanguage(int* selected)
{
    if (!selected) {
        return false;
    }

    if (mSelected >= 0 && mSelected < mSubtitles.size()) {
        *selected = mSelected;
        return true;
    }

    return false;
}

bool CSubtitleManager::setSelectedLanguage(int selected)
{
    if (selected < 0 && selected >= mSubtitles.size()) {
        return false;
    }
    mSelected = selected;
    return true;
}

bool CSubtitleManager::getSubtitleSegment(int64_t time, STSSegment** segment)
{
    return false;
}

bool CSubtitleManager::seekTo(int64_t time)
{
    CSimpleTextSubtitle* subtitle = getSelectedSimpleTextSubtitle();
    if (subtitle) {
        return subtitle->seekTo(time);
    }

    return false;
}

bool CSubtitleManager::getNextSubtitleSegment(STSSegment** segment)
{
    CSimpleTextSubtitle* subtitle = getSelectedSimpleTextSubtitle();
    if (subtitle) {
        return subtitle->getNextSubtitleSegment(segment);
    }

    return false;
}

bool CSubtitleManager::loadSubtitle(const char* fileName, bool isMediaFile)
{
    std::vector<CSimpleTextSubtitle*>::iterator itr = mSubtitles.begin();
    for (; itr != mSubtitles.end(); ++itr) {
        if (strcmp((*itr)->getFileName(), fileName) == 0) {
            return true;
        }
    }

    CSimpleTextSubtitle* subtitle = new CSimpleTextSubtitle(mAssLibrary);
    if (!subtitle->LoadFile(fileName)) {
        delete subtitle;
        return false;
    }

    subtitle->seekTo(0);

    mSubtitles.push_back(subtitle);

    return true;
}

int  CSubtitleManager::getSubtitleIndex(const char* fileName)
{
    std::vector<CSimpleTextSubtitle*>::iterator itr = mSubtitles.begin();
    for (int index = 0; itr != mSubtitles.end(); ++itr) {
        if (strcmp((*itr)->getFileName(), fileName) == 0) {
            return index;
        }
    }

    return -1;
}

bool ISubtitles::create(ISubtitles** subtitle)
{
    if (!subtitle) {
        return false;
    }

    *subtitle = new CSubtitleManager;
    if (*subtitle) {
        return true;
    }

    return false;
}

#ifdef _TEST
int main(int argc, char* argv[])
{
    ISubtitles* subtitle = NULL;
    if (!ISubtitles::create(&subtitle)) {
        return 0;
    }
    subtitle->loadSubtitle("gb18030.ass", false);
    subtitle->seekTo(0);

    STSSegment* segment = NULL;
    while(subtitle->getNextSubtitleSegment(&segment)) {
        int64_t startTime = segment->getStartTime();
        int64_t stopTime = segment->getStopTime();
        printf("%01d:%02d:%02d.%02d  --> %01d:%02d:%02d.%02d  ",
            int(startTime/1000/3600), int(startTime/1000%3600/60), int(startTime/1000%60), int(startTime%1000)/10,
            int(stopTime/1000/3600), int(stopTime/1000%3600/60), int(stopTime/1000%60), int(stopTime%1000)/10);

        char subtitleText[1024];
        segment->getSubtitleText(subtitleText, 1024);
        printf("%s\n", subtitleText);
    }

    subtitle->close();
    return 0;
}
#endif // _TEST

