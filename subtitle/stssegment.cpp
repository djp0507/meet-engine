#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <memory.h>
#include "subtitle.h"
#include "stssegment.h"
#include "simpletextsubtitle.h"

extern "C" {
#include "libass/ass.h"
};

int CSTSSegment::getSubtitleText(char* text, int maxLength)
{
    int textLength = 0;
    for (size_t i = 0; i < mSubs.size(); ++i) {
        ASS_Event* event = mSubtitle->getEventAt(mSubs[i]);
        if (!text) {
            if (textLength) {
                textLength += 1; // »»ÐÐ·û
            }
            textLength += strlen(event->Text);
        } else {
            if (textLength && maxLength > 1) {
                *text++ = '\n';
                textLength ++;
                maxLength --;
            }
            const char* p = event->Text;
            for ( ;maxLength > 1 && *p; --maxLength, *text++ = *p++, ++textLength) {
            }
        }
    }
    if (text) {
        *text = '\x0';
    }

    return textLength;
}
