/*
 * Copyright (C) 2012 Roger Shen  rogershen@pptv.com
 *
 */
 
#include <sys/time.h>
#include "utils.h"


int64_t getNowMs()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return (int64_t)tv.tv_usec /1000ll + tv.tv_sec * 1000ll;
}

int64_t getNowUs()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    return (int64_t)tv.tv_sec * 1000000ll + tv.tv_usec;
}


