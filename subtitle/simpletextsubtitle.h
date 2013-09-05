
extern "C" {
#include "libass/ass.h"
};
#include <vector>
#include "tinyxml2/tinyxml2.h"

class CSTSSegment;

class CSimpleTextSubtitle
{
public:
    friend class STSSegment;
    CSimpleTextSubtitle(ASS_Library* assLibrary):mAssLibrary(assLibrary),mAssTrack(NULL)
    {
        mNextSegment = 0;
        mFileName = NULL;
        mLanguageName = NULL;
    }
    virtual ~CSimpleTextSubtitle();

    bool LoadFile(const char* fileName);
    bool ParseXMLNode(const char* fileName, tinyxml2::XMLElement* element);
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
    const char* getLanguageName()
    {
        return mLanguageName;
    }
protected:
    bool ArrangeTrack(ASS_Track* track);

    std::vector<CSTSSegment*>  mSegments;
    ASS_Library*            mAssLibrary;
    ASS_Track*              mAssTrack;
    size_t                  mNextSegment;
    const char*             mFileName;
    const char*             mLanguageName;
};
