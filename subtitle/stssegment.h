
class CSimpleTextSubtitle;

#include <vector>

class CSTSSegment :
    public STSSegment
{
public:
    CSTSSegment(CSimpleTextSubtitle* subtitle, int64_t startTime, int64_t stopTime):
      mSubtitle(subtitle),mStartTime(startTime),mStopTime(stopTime)
      {
      }
      virtual ~CSTSSegment()
      {
      }

      // implement STSSegment
public:
    virtual int64_t getStartTime() { return mStartTime; }
    virtual int64_t getStopTime()  { return mStopTime;  }
    virtual int getEntryCount()
    {
        return mSubs.size();
    }
    virtual STSEntity* getEntry(int index)
    {
        return NULL;
    }
    virtual int getSubtitleText(char* text, int maxLength);
    virtual int getSubtitleImage(int *width, int *height, void** bitmapData)
    {
        return 0;
    }

public:
    CSimpleTextSubtitle*  mSubtitle;
    int64_t           mStartTime;
    int64_t           mStopTime;
    std::vector<int>  mSubs;
};
