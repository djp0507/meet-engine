//
//  ppHttpd.h
//
//  Created by stephenzhang on 13-8-29.
//  Copyright (c) 2013年 Stephen Zhang. All rights reserved.
//

#ifndef socket_httpd_h
#define socket_httpd_h

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define METHOD_UNKNOWN 0
#define METHOD_GET 1
#define METHOD_HEAD 2
#define METHOD_POST 3
#define SERVER_SOFTWARE "ppHttpd"
#define SERVER_URL "http://www.pptv.com"

typedef struct
{
    unsigned short port;
    char* dir;
    char* hostname;
} HostParameter;

void init_http_host(HostParameter *host_parameter);
void close_host();

#endif
