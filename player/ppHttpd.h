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
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <limits.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <time.h>
#include <pwd.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <ctype.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <dirent.h>
#include <sys/uio.h>

#ifndef ERR_DIR
#define ERR_DIR "errors"
#endif
#ifndef AUTH_FILE
#define AUTH_FILE ".htpasswd"
#endif
#ifndef READ_TIMEOUT
#define READ_TIMEOUT 60
#endif
#ifndef WRITE_TIMEOUT
#define WRITE_TIMEOUT 300
#endif

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif
#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

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

#endif
