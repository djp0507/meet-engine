
extern "C" {
#include "libass/ass.h"
};
#include <vector>

class CSTSSegment;

class CSimpleTextSubtitle
{
public:
    friend class STSSegment;
    CSimpleTextSubtitle(ASS_Library* assLibrary):mAssLibrary(assLibrary),mAssTrack(NULL)
    {
        mNextSegment = 0;
        mFileName = NULL;
    }
    virtual ~CSimpleTextSubtitle();

    bool LoadFile(const char* fileName);

    bool getSubtitleSegment(int64_t time, STSSegment** segment)
    {
        return false;
    }
    bool seekTo(int64_t time);
    bool getNextSubtitleSegment(STSSegment** segment);
    ASS_Event* getEventAt(int pos);

    const char* getFileName()
    {
        return mFileName;
    }
protected:
    std::vector<CSTSSegment*>  mSegments;
    ASS_Library*            mAssLibrary;
    ASS_Track*              mAssTrack;
    size_t                  mNextSegment;
    const char*             mFileName;
};