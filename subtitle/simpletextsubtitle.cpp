#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <memory.h>
#include "subtitle.h"
#include "stssegment.h"
#include "simpletextsubtitle.h"

#ifdef _TEST
#include <atlbase.h>
#include <atlconv.h>
#endif

#include <set>

CSimpleTextSubtitle::~CSimpleTextSubtitle()
{
    if (mAssTrack) {
        ass_free_track(mAssTrack);
        mAssTrack = NULL;
    }
    std::vector<CSTSSegment*>::iterator itr = mSegments.begin();
    for (; itr != mSegments.end(); ++itr) {
        delete *itr;
    }
    mSegments.clear();
    if (mFileName) {
        free((void*)mFileName);
        mFileName = NULL;
    }
}

bool CSimpleTextSubtitle::LoadFile(const char* fileName)
{
    if (!mAssLibrary) {
        return false;
    }
    mAssTrack = ass_read_file(mAssLibrary, const_cast<char*>(fileName), "enca:zh:utf-8");
    if (!mAssTrack) {
        return false;
    }

    std::set<int64_t> breakpoints;
    for (int i = 0; i < mAssTrack->n_events; ++i) {
        ASS_Event* event = &mAssTrack->events[i];
        int64_t startTime = event->Start;
        int64_t stopTime  = event->Start + event->Duration;

        breakpoints.insert(startTime);
        breakpoints.insert(stopTime);
    }

    std::set<int64_t>::iterator itr = breakpoints.begin();
    int64_t prev = 0;
    if (itr != breakpoints.end()) {
        prev = *itr;
        ++itr;
    }
    for (; itr != breakpoints.end(); ++itr) {
        CSTSSegment* segment = new CSTSSegment(this, prev, *itr);
        mSegments.push_back(segment);
        prev = *itr;
    }

    for (int i = 0; i < mAssTrack->n_events; ++i) {
        ASS_Event* event = &mAssTrack->events[i];
        int64_t startTime = event->Start;
        int64_t stopTime  = event->Start + event->Duration;

        size_t j = 0;
        for (j = 0; j < mSegments.size() && mSegments[j]->mStartTime < startTime; ++j) {
        }
        for (; j < mSegments.size() && mSegments[j]->mStopTime <= stopTime; ++j) {
            CSTSSegment* s = mSegments[j];
            for (int l = 0, m = s->mSubs.size(); l <= m; l++) {
                if (l == m || event->ReadOrder < mAssTrack->events[s->mSubs[l]].ReadOrder) {
                    s->mSubs.insert(s->mSubs.begin() + l, i);
                    break;
                }
            }
        }
    }

    // 删除空segment
    for (int i = mSegments.size() - 1; i >= 0; --i) {
        if (mSegments[i]->mSubs.size() <= 0) {
            CSTSSegment* p = mSegments[i];
            mSegments.erase(mSegments.begin() + i);
            delete p;
        }
    }

    mFileName = strdup(fileName);

    return true;
}

bool CSimpleTextSubtitle::seekTo(int64_t time)
{
    size_t nextPos = 0;
    for (size_t i = 0; i < mSegments.size(); ++i, ++nextPos) {
        CSTSSegment* segment = mSegments.at(i);
        if (segment->mStopTime >= time) {
            break;
        }
    }
    mNextSegment = nextPos;
    return true;
}

bool CSimpleTextSubtitle::getNextSubtitleSegment(STSSegment** segment)
{
    if (!segment) {
        return false;
    }

    if (mNextSegment < mSegments.size()) {
        *segment = mSegments[mNextSegment];
        mNextSegment++;
        return true;
    }
    return false;
}

ASS_Event* CSimpleTextSubtitle::getEventAt(int pos)
{
    if (pos >= 0 && pos < mAssTrack->n_events) {
        return &mAssTrack->events[pos];
    }
    return NULL;
}

