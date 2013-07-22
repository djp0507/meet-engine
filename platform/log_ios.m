#import <Foundation/NSObjCRuntime.h>
#import <Foundation/NSString.h>
#include "log_ios.h"


void __ios_log(const char *fmt, ...)
{
    return;
    va_list argp;
    va_start(argp, fmt);
    
    //NSString *log = [[NSString alloc] initWithCString:(const char*)fmt encoding:NSASCIIStringEncoding];
    //NSLogv(log, argp);
    
    char msg[2048] = {0};
    vsnprintf(msg, sizeof(msg), fmt, argp);
    printf("%s \n", msg);
    
    va_end(argp);
}

void setFFmpegLogCallback(void* avcl, int level, const char* fmt, va_list vl)
{
}

