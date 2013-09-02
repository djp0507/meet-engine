//
//  ppHttpd.c
//
//  Created by stephenzhang on 13-8-29.
//  Copyright (c) 2013年 Stephen Zhang. All rights reserved.
//
#include "ppHttpd.h"

typedef union
{
    struct sockaddr sa;
    struct sockaddr_in sa_in;
}   usockaddr;

static unsigned short port;
static char* dir;
static char* hostname;
static char* charset;
static int listenfd;

/********** Request variables **********/
static int conn_fd;
static usockaddr client_addr;
static char* request;
static size_t request_size, request_len, request_idx;
static int method;
static char* path;
static char* file;
static char* pathinfo;
struct stat sb;
static char* query;
static char* protocol;
static int status;
static off_t bytes;
static char* req_hostname;
static char* authorization;
static size_t content_length;
static char* content_type;
static char* cookie;
static char* host;
static time_t if_modified_since;
static char* referer;
static char* useragent;
static char* remoteuser;

/* Forwards. */
static int hexit( char c );
static int initialize_listen_socket( usockaddr* usaP );
static void handle_request( void );
static void de_dotdot( char* file );
static int get_pathinfo( void );
static void do_file( void );
static void do_dir( void );
static char* file_details( const char* dir, const char* name );
static void strencode( char* to, size_t tosize, const char* from );
static char** make_envp( void );
static void auth_check( char* dirname );
static void send_authenticate( char* realm );
static void send_error( int s, char* title, char* extra_header, char* text );
static void send_error_body( int s, char* title, char* text );
static void send_error_tail( void );
static void add_headers( int s, char* title, char* extra_header, char* me, char* mt, off_t b, time_t mod );
static void start_request( void );
static void add_to_request( char* str, size_t len );
static char* get_request_line( void );
static void start_response( void );
static void add_to_response( char* str, size_t len );
static void send_response( void );
static void send_via_write( int fd, off_t size );
static void add_to_buf( char** bufP, size_t* bufsizeP, size_t* buflenP, char* str, size_t len );
static char* get_method_str( int m );
static void init_mime( void );
static const char* figure_mime( char* name, char* me, size_t me_size );
static char* ntoa( usockaddr* usaP );
static size_t sockaddr_len( usockaddr* usaP );
static void strdecode( char* to, char* from );
static int b64_decode( const char* str, unsigned char* space, int size );
static void set_ndelay( int fd );
static void clear_ndelay( int fd );
static void* e_malloc( size_t size );
static void* e_realloc( void* optr, size_t size );
static char* e_strdup( char* ostr );

void init_http_host(HostParameter *host_parameter)
{
    usockaddr host_addr;
    fd_set lfdset;
    int maxfd;
    usockaddr usa;
    unsigned int sz;

    port = host_parameter -> port;
    dir = host_parameter -> dir;
    hostname = host_parameter -> hostname;
    
    (void) memset( &host_addr, 0, sizeof(host_addr) );
    host_addr.sa.sa_family = AF_INET;
    host_addr.sa_in.sin_addr.s_addr = inet_addr( hostname );
    host_addr.sa_in.sin_port = htons( port );
    listenfd = initialize_listen_socket( &host_addr );
    if ( listenfd == -1 )
	{
        (void) fprintf( stderr, "can't bind to any address\n");
        return;
	}
    if ( chdir( dir ) < 0 )
    {
        (void) fprintf( stderr, "can't chdir\n");
        return;
    }
    init_mime();    
    /* Main loop. */
    for (;;)
	{
        FD_ZERO( &lfdset );
        maxfd = -1;
        if ( listenfd != -1 )
	    {
            FD_SET( listenfd, &lfdset );
            if ( listenfd > maxfd )
                maxfd = listenfd;
	    }
        if ( select( maxfd + 1, &lfdset, (fd_set*) 0, (fd_set*) 0, (struct timeval*) 0 ) < 0 )
	    {
            if ( errno == EINTR || errno == EAGAIN )continue;	/* try again */
            return;
	    }
        /* Accept the new connection. */
        sz = sizeof(usa);
        if ( listenfd != -1 && FD_ISSET( listenfd, &lfdset ) )
            conn_fd = accept( listenfd, &usa.sa, &sz);
        else
	    {
            (void) fprintf( stderr, "can't connection\n");
            return;
	    }
        if ( conn_fd < 0 )
	    {
            if ( errno == EINTR || errno == EAGAIN )continue;	/* try again */
            return;
	    }
            client_addr = usa;
            handle_request();
        (void) close( conn_fd );
	}
}

static int initialize_listen_socket( usockaddr* usaP )
{
    int listen_fd;
    int i = 1;
    listen_fd = socket( usaP->sa.sa_family, SOCK_STREAM, 0 );
    if ( listen_fd < 0 )
	{
        (void) fprintf(stderr, "create socket failed\n");
        return -1;
	}
    
    (void) fcntl( listen_fd, F_SETFD, 1 );
    if ( setsockopt( listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*) &i, sizeof(i) ) < 0 )
	{
        (void) fprintf(stderr, "setsockopt SO_REUSEADDR failed\n");
        return -1;
	}
    
    if ( bind( listen_fd, &usaP->sa, sockaddr_len( usaP ) ) < 0 )
	{
        (void) fprintf(stderr, "bind socket failed\n");
        return -1;
	}
    
    if ( listen( listen_fd, 10 ) < 0 )
	{
        (void) fprintf(stderr, "listen socket failed\n");
        return -1;
	}
    
    return listen_fd;
}

static void handle_sigterm( int sig )
{
    (void) fprintf( stderr, "exiting due to signal %d\n", sig );
    return;
}

static void handle_sighup( int sig )
{
    const int oerrno = errno;
    errno = oerrno;
}

static void handle_sigchld( int sig )
{
    const int oerrno = errno;
    pid_t pid;
    int status;    
    /* Reap defunct children until there aren't any more. */
    for (;;)
	{
        pid = waitpid( (pid_t) -1, &status, WNOHANG );
        if ( (int) pid == 0 )		/* none left */
            break;
        if ( (int) pid < 0 )
	    {
            if ( errno == EINTR || errno == EAGAIN )continue;
            break;
	    }
	}   
    /* Restore previous errno. */
    errno = oerrno;
}

struct mime_entry
{
    const char* ext;
    size_t ext_len;
    const char* val;
    size_t val_len;
};
static struct mime_entry enc_tab[] = {
#include "mime_encodings.h"
};
static const int n_enc_tab = sizeof(enc_tab) / sizeof(*enc_tab);
static struct mime_entry typ_tab[] = {
#include "mime_types.h"
};
static const int n_typ_tab = sizeof(typ_tab) / sizeof(*typ_tab);


int ext_compare( const void *a, const void *b )
{
        return * ( int * ) a - * ( int * ) b;
}

static void init_mime( void )
{
    int i;
    /* Sort the tables so we can do binary search. */
    qsort( enc_tab, n_enc_tab, sizeof(*enc_tab), ext_compare );
    qsort( typ_tab, n_typ_tab, sizeof(*typ_tab), ext_compare );
    
    /* Fill in the lengths. */
    for ( i = 0; i < n_enc_tab; ++i )
	{
        enc_tab[i].ext_len = strlen( enc_tab[i].ext );
        enc_tab[i].val_len = strlen( enc_tab[i].val );
	}
    for ( i = 0; i < n_typ_tab; ++i )
	{
        typ_tab[i].ext_len = strlen( typ_tab[i].ext );
        typ_tab[i].val_len = strlen( typ_tab[i].val );
	}
}

static void send_error( int s, char* title, char* extra_header, char* text )
{
    add_headers(s, title, extra_header, "", "text/html; charset=%s", (off_t) -1, (time_t) -1 );
    
    send_error_tail();
    
    send_response();

}

static void send_error_tail( void )
{
    char buf[500];
    int buflen;
    buflen = snprintf( buf, sizeof(buf), "\
                      <HR>\n\
                      <ADDRESS><A HREF=\"%s\">%s</A></ADDRESS>\n\
                      </BODY>\n\
                      </HTML>\n",
                      SERVER_URL, SERVER_SOFTWARE );
    add_to_response( buf, buflen );
}


static void add_headers( int s, char* title, char* extra_header, char* me, char* mt, off_t b, time_t mod )
{
    time_t now;
    char timebuf[100];
    char buf[10000];
    int buflen;
    int s100;
    const char* rfc1123_fmt = "%a, %d %b %Y %H:%M:%S GMT";
    
    status = s;
    bytes = b;
    start_response();
    buflen = snprintf( buf, sizeof(buf), "%s %d %s\015\012", protocol, status, title );
    add_to_response( buf, buflen );
    buflen = snprintf( buf, sizeof(buf), "Server: %s\015\012", SERVER_SOFTWARE );
    add_to_response( buf, buflen );
    now = time( (time_t*) 0 );
    (void) strftime( timebuf, sizeof(timebuf), rfc1123_fmt, gmtime( &now ) );
    buflen = snprintf( buf, sizeof(buf), "Date: %s\015\012", timebuf );
    add_to_response( buf, buflen );
    s100 = status / 100;
    if ( s100 != 2 && s100 != 3 )
	{
        buflen = snprintf( buf, sizeof(buf), "Cache-Control: no-cache,no-store\015\012" );
        add_to_response( buf, buflen );
	}
    if ( extra_header != (char*) 0 && extra_header[0] != '\0' )
	{
        buflen = snprintf( buf, sizeof(buf), "%s\015\012", extra_header );
        add_to_response( buf, buflen );
	}
    if ( me != (char*) 0 && me[0] != '\0' )
	{
        buflen = snprintf( buf, sizeof(buf), "Content-Encoding: %s\015\012", me );
        add_to_response( buf, buflen );
	}
    if ( mt != (char*) 0 && mt[0] != '\0' )
	{
        buflen = snprintf( buf, sizeof(buf), "Content-Type: %s\015\012", mt );
        add_to_response( buf, buflen );
	}
    if ( bytes >= 0 )
	{
        buflen = snprintf(buf, sizeof(buf), "Content-Length: %lld\015\012", (int64_t) bytes );
        add_to_response( buf, buflen );
	}
    if ( mod != (time_t) -1 )
	{
        (void) strftime(timebuf, sizeof(timebuf), rfc1123_fmt, gmtime( &mod ) );
        buflen = snprintf( buf, sizeof(buf), "Last-Modified: %s\015\012", timebuf );
        add_to_response( buf, buflen );
	}
    buflen = snprintf( buf, sizeof(buf), "Connection: close\015\012\015\012" );
    add_to_response( buf, buflen );
}


static void handle_request( void )
{
    char* method_str;
    char* line;
    char* cp;
    int r, file_len, i;
    const char* index_names[] = {"index.html", "index.htm", "index.xhtml", "index.xht", "Default.htm","index.cgi"};    
    remoteuser = (char*) 0;
    method = METHOD_UNKNOWN;
    path = (char*) 0;
    file = (char*) 0;
    pathinfo = (char*) 0;
    query = "";
    protocol = (char*) 0;
    status = 0;
    bytes = -1;
    req_hostname = (char*) 0;    
    authorization = (char*) 0;
    content_type = (char*) 0;
    content_length = -1;
    cookie = (char*) 0;
    host = (char*) 0;
    if_modified_since = (time_t) -1;
    referer = "";
    useragent = "";
    
#ifdef TCP_NOPUSH
    r = 1;
    (void) setsockopt(conn_fd, IPPROTO_TCP, TCP_NOPUSH, (void*) &r, sizeof(r) );
#endif
    /* Read in the request. */
    start_request();
    for (;;)
	{
        char buf[10000];
        int r = read( conn_fd, buf, sizeof(buf) );
        if ( r < 0 && ( errno == EINTR || errno == EAGAIN ) )continue;
        if ( r <= 0 )break;
        add_to_request( buf, r );
        if ( strstr( request, "\015\012\015\012" ) != (char*) 0 || strstr( request, "\012\012" ) != (char*) 0 )break;
	}
    
    /* Parse the first line of the request. */
    method_str = get_request_line();
    (void) fprintf(stderr, "%s\n",method_str);
    if ( method_str == (char*) 0 )
    {
        send_error( 400, "Bad Request", "", "Can't parse request." );
        return;
    }
    path = strpbrk( method_str, " \t\012\015" );
    if ( path == (char*) 0 )
    {
        send_error( 400, "Bad Request", "", "Can't parse request." );
        return;
    }
    *path++ = '\0';
    path += strspn( path, " \t\012\015" );
    protocol = strpbrk( path, " \t\012\015" );
    if ( protocol == (char*) 0 )
    {
        send_error( 400, "Bad Request", "", "Can't parse request." );
        return;
    }
    *protocol++ = '\0';
    protocol += strspn( protocol, " \t\012\015" );
    query = strchr( path, '?' );
    if ( query == (char*) 0 )query = "";
    else *query++ = '\0';
    
    /* Parse the rest of the request headers. */
    while ( ( line = get_request_line() ) != (char*) 0 )
	{
        if ( line[0] == '\0' )
            break;
        else if ( strncasecmp( line, "Authorization:", 14 ) == 0 )
	    {
            cp = &line[14];
            cp += strspn( cp, " \t" );
            authorization = cp;
            (void) fprintf(stderr, "authorization:%s\n",authorization);
	    }
        else if ( strncasecmp( line, "Content-Length:", 15 ) == 0 )
	    {
            cp = &line[15];
            cp += strspn( cp, " \t" );
            content_length = atol( cp );
            (void) fprintf(stderr, "content_length:%ld\n",content_length);
	    }
        else if ( strncasecmp( line, "Content-Type:", 13 ) == 0 )
	    {
            cp = &line[13];
            cp += strspn( cp, " \t" );
            content_type = cp;
            (void) fprintf(stderr, "content_type:%s\n",content_type);
	    }
        else if ( strncasecmp( line, "Cookie:", 7 ) == 0 )
	    {
            cp = &line[7];
            cp += strspn( cp, " \t" );
            cookie = cp;
            (void) fprintf(stderr, "cookie:%s\n",cookie);
	    }
        else if ( strncasecmp( line, "Host:", 5 ) == 0 )
	    {
            cp = &line[5];
            cp += strspn( cp, " \t" );
            host = cp;
            (void) fprintf(stderr, "host:%s\n",host);
            if ( strchr( host, '/' ) != (char*) 0 || host[0] == '.' )
            {
                send_error( 400, "Bad Request", "", "Can't parse request." );
                return;
            }
	    }
        else if ( strncasecmp( line, "Referer:", 8 ) == 0 )
	    {
            cp = &line[8];
            cp += strspn( cp, " \t" );
            referer = cp;
            (void) fprintf(stderr, "referer:%s\n",referer);
	    }
        else if ( strncasecmp( line, "User-Agent:", 11 ) == 0 )
	    {
            cp = &line[11];
            cp += strspn( cp, " \t" );
            useragent = cp;
            (void) fprintf(stderr, "useragent:%s\n",useragent);
	    }
	}
    
    if ( strcasecmp( method_str, get_method_str( METHOD_GET ) ) == 0 )
        method = METHOD_GET;
    else if ( strcasecmp( method_str, get_method_str( METHOD_HEAD ) ) == 0 )
        method = METHOD_HEAD;
    else if ( strcasecmp( method_str, get_method_str( METHOD_POST ) ) == 0 )
        method = METHOD_POST;
    else
    {
        send_error( 501, "Not Implemented", "", "That method is not implemented." );
        return;
    }
    strdecode( path, path );
    if ( path[0] != '/' )
    {
        send_error( 400, "Bad Request", "", "Bad filename." );
        return;
    }
    file = &(path[1]);
    de_dotdot( file );
    if ( file[0] == '\0' )
        file = "./";
    if ( file[0] == '/' ||( file[0] == '.' && file[1] == '.' &&( file[2] == '\0' || file[2] == '/' ) ) )
    {
        send_error( 400, "Bad Request", "", "Illegal filename." );
        return;
    }
    r = stat( file, &sb );
    if ( r < 0 )
        r = get_pathinfo();
    if ( r < 0 )
    {
        send_error( 404, "Not Found", "", "File not found." );
        return;
    }
    file_len = strlen( file );
    if ( ! S_ISDIR( sb.st_mode ) )
	{
        /* Not a directory. */
        while ( file[file_len - 1] == '/' )
	    {
            file[file_len - 1] = '\0';
            --file_len;
	    }
        do_file();
	}
    else
	{
        char idx[10000];
        
        /* The filename is a directory.  Is it missing the trailing slash? */
        if ( file[file_len - 1] != '/' && pathinfo == (char*) 0 )
	    {
            char location[10000];
            if ( query[0] != '\0' )
                (void) snprintf(location, sizeof(location), "Location: %s/?%s", path,query );
            else
                (void) snprintf(location, sizeof(location), "Location: %s/", path );
            send_error( 302, "Found", location, "Directories must end with a slash." );
	    }
        
        /* Check for an index file. */
        for ( i = 0; i < sizeof(index_names) / sizeof(char*); ++i )
	    {
            (void) snprintf( idx, sizeof(idx), "%s%s", file, index_names[i] );
            if ( stat( idx, &sb ) >= 0 )
            {
                file = idx;
                do_file();
                goto got_one;
            }
	    }
        
        /* Nope, no index file, so it's an actual directory request. */
        do_dir();
        
	got_one: ;
	}
}

static size_t sockaddr_len( usockaddr* usaP )
{
    switch ( usaP->sa.sa_family )
    {
        case AF_INET:
            return sizeof(struct sockaddr_in);
        default:
            return 0;	/* shouldn't happen */
    }
}

static void start_request( void )
{
    request_size = 0;
    request_idx = 0;
}

static void add_to_request( char* str, size_t len )
{
    add_to_buf( &request, &request_size, &request_len, str, len );
}

static char* get_request_line( void )
{
    int i;
    char c;
    for ( i = request_idx; request_idx < request_len; ++request_idx )
	{
        c = request[request_idx];
        if ( c == '\012' || c == '\015' )
	    {
            request[request_idx] = '\0';
            ++request_idx;
            if ( c == '\015' && request_idx < request_len && request[request_idx] == '\012' )
            {
                request[request_idx] = '\0';
                ++request_idx;
            }
            return &(request[i]);
	    }
	}
    return (char*) 0;
}

static char* response;
static size_t response_size, response_len;

static void start_response( void )
{
    response_size = 0;
}

static void add_to_response( char* str, size_t len )
{
    add_to_buf( &response, &response_size, &response_len, str, len );
}

static void add_to_buf( char** bufP, size_t* bufsizeP, size_t* buflenP, char* str, size_t len )
{
    if ( *bufsizeP == 0 )
	{
        *bufsizeP = len + 500;
        *buflenP = 0;
        *bufP = (char*) e_malloc( *bufsizeP );
	}
    else if ( *buflenP + len >= *bufsizeP )
	{
        *bufsizeP = *buflenP + len + 500;
        *bufP = (char*) e_realloc( (void*) *bufP, *bufsizeP );
	}
    (void) memmove( &((*bufP)[*buflenP]), str, len );
    *buflenP += len;
    (*bufP)[*buflenP] = '\0';
}

/* Clear NDELAY mode on a socket. */
static void clear_ndelay( int fd )
{
    int flags, newflags;
    
    flags = fcntl( fd, F_GETFL, 0 );
    if ( flags != -1 )
	{
        newflags = flags & ~ (int) O_NDELAY;
        if ( newflags != flags )
            (void) fcntl( fd, F_SETFL, newflags );
	}
}


static void* e_malloc( size_t size )
{
    void* ptr;
    ptr = malloc( size );
    if ( ptr == (void*) 0 )
	{
        (void) fprintf( stderr, "out of memory\n");
	}
    return ptr;
}

static void* e_realloc( void* optr, size_t size )
{
    void* ptr;
    
    ptr = realloc( optr, size );
    if ( ptr == (void*) 0 )
	{
        (void) fprintf( stderr, "out of memory\n");
	}
    return ptr;
}


static char* e_strdup( char* ostr )
{
    char* str;
    
    str = strdup( ostr );
    if ( str == (char*) 0 )
	{
        (void) fprintf( stderr, "out of memory copying a string\n");
	}
    return str;
}

static void send_response( void )
{
    write( conn_fd, response, response_len );
}

static char* get_method_str( int m )
{
    switch ( m )
	{
        case METHOD_GET: return "GET";
        case METHOD_HEAD: return "HEAD";
        case METHOD_POST: return "POST";
        default: return "UNKNOWN";
	}
}

static void strdecode( char* to, char* from )
{
    for ( ; *from != '\0'; ++to, ++from )
	{
        if ( from[0] == '%' && isxdigit( from[1] ) && isxdigit( from[2] ) )
	    {
            *to = hexit( from[1] ) * 16 + hexit( from[2] );
            from += 2;
	    }
        else
            *to = *from;
	}
    *to = '\0';
}

static void de_dotdot( char* file )
{
    char* cp;
    char* cp2;
    int l;
    
    /* Collapse any multiple / sequences. */
    while ( ( cp = strstr( file, "//") ) != (char*) 0 )
	{
        for ( cp2 = cp + 2; *cp2 == '/'; ++cp2 )
            continue;
        (void) strcpy( cp + 1, cp2 );
	}
    
    /* Remove leading ./ and any /./ sequences. */
    while ( strncmp( file, "./", 2 ) == 0 )
        (void) strcpy( file, file + 2 );
    while ( ( cp = strstr( file, "/./") ) != (char*) 0 )
        (void) strcpy( cp, cp + 2 );
    
    /* Alternate between removing leading ../ and removing xxx/../ */
    for (;;)
	{
        while ( strncmp( file, "../", 3 ) == 0 )
            (void) strcpy( file, file + 3 );
        cp = strstr( file, "/../" );
        if ( cp == (char*) 0 )
            break;
        for ( cp2 = cp - 1; cp2 >= file && *cp2 != '/'; --cp2 )
            continue;
        (void) strcpy( cp2 + 1, cp + 4 );
	}
    
    /* Also elide any xxx/.. at the end. */
    while ( ( l = strlen( file ) ) > 3 &&
           strcmp( ( cp = file + l - 3 ), "/.." ) == 0 )
	{
        for ( cp2 = cp - 1; cp2 >= file && *cp2 != '/'; --cp2 )
            continue;
        if ( cp2 < file )
            break;
        *cp2 = '\0';
	}
}

static int get_pathinfo( void )
{
    int r;    
    pathinfo = &file[strlen(file)];
    for (;;)
	{
        do
	    {
            --pathinfo;
            if ( pathinfo <= file )
            {
                pathinfo = (char*) 0;
                return -1;
            }
	    }
        while ( *pathinfo != '/' );
        *pathinfo = '\0';
        r = stat( file, &sb );
        if ( r >= 0 )
	    {
            ++pathinfo;
            return r;
	    }
        else
            *pathinfo = '/';
	}
}

static void do_file( void )
{
    char buf[10000];
    char mime_encodings[500];
    const char* mime_type;
    char fixed_mime_type[500];
    char* cp;
    int fd;    
    /* Check authorization for this directory. */
    (void) strncpy( buf, file, sizeof(buf) );
    cp = strrchr( buf, '/' );
    if ( cp == (char*) 0 )
        (void) strcpy( buf, "." );
    else
        *cp = '\0';
    auth_check( buf );
    
    /* Check if the filename is the AUTH_FILE itself - that's verboten. */
    if ( strcmp( file, AUTH_FILE ) == 0 ||
        ( strcmp( &(file[strlen(file) - sizeof(AUTH_FILE) + 1]), AUTH_FILE ) == 0 &&
         file[strlen(file) - sizeof(AUTH_FILE)] == '/' ) )
	{
        send_error( 403, "Forbidden", "", "File is protected." );
        return;
	}
    
    if ( pathinfo != (char*) 0 )
    {
        send_error( 404, "Not Found", "", "File not found." );
        return;
    }
    
    fd = open( file, O_RDONLY );
    if ( fd < 0 )
	{
        send_error( 403, "Forbidden", "", "File is protected." );
	}
    mime_type = figure_mime( file, mime_encodings, sizeof(mime_encodings) );
    (void) snprintf(fixed_mime_type, sizeof(fixed_mime_type), mime_type, charset );
    if ( if_modified_since != (time_t) -1 && if_modified_since >= sb.st_mtime )
	{
        add_headers(304, "Not Modified", "", mime_encodings, fixed_mime_type,(off_t) -1, sb.st_mtime );
        send_response();
        return;
	}
    add_headers(200, "Ok", "", mime_encodings, fixed_mime_type, sb.st_size,sb.st_mtime );
    send_response();
    if ( method == METHOD_HEAD )return;
    
    if ( sb.st_size > 0 )	/* ignore zero-length files */
	{
        fprintf(stderr, "size:%lld\n",sb.st_size);
        send_via_write( fd, sb.st_size );
	}
    (void) close( fd );
}

static void auth_check( char* dirname )
{
    char authpath[10000];
    struct stat sb;
    char authinfo[500];
    char* authpass;
    char* colon;
    static char line[10000];
    int l;
    FILE* fp;
    char* cryp;
    
    /* Construct auth filename. */
    if ( dirname[strlen(dirname) - 1] == '/' )
        (void) snprintf( authpath, sizeof(authpath), "%s%s", dirname, AUTH_FILE );
    else
        (void) snprintf( authpath, sizeof(authpath), "%s/%s", dirname, AUTH_FILE );
    
    /* Does this directory have an auth file? */
    if ( stat( authpath, &sb ) < 0 )
	/* Nope, let the request go through. */
        return;
    
    /* Does this request contain authorization info? */
    if ( authorization == (char*) 0 )
	/* Nope, return a 401 Unauthorized. */
        send_authenticate( dirname );
    
    /* Basic authorization info? */
    if ( strncmp( authorization, "Basic ", 6 ) != 0 )
        send_authenticate( dirname );
    
    /* Decode it. */
    l = b64_decode(&(authorization[6]), (unsigned char*) authinfo, sizeof(authinfo) - 1 );
    authinfo[l] = '\0';
    /* Split into user and password. */
    authpass = strchr( authinfo, ':' );
    if ( authpass == (char*) 0 )
	/* No colon?  Bogus auth info. */
        send_authenticate( dirname );
    *authpass++ = '\0';
    /* If there are more fields, cut them off. */
    colon = strchr( authpass, ':' );
    if ( colon != (char*) 0 )
        *colon = '\0';
    
    /* Open the password file. */
    fp = fopen( authpath, "r" );
    if ( fp == (FILE*) 0 )
	{
        send_error( 403, "Forbidden", "", "File is protected." );
	}
    
    /* Read it. */
    while ( fgets( line, sizeof(line), fp ) != (char*) 0 )
	{
        /* Nuke newline. */
        l = strlen( line );
        if ( line[l - 1] == '\n' )
            line[l - 1] = '\0';
        /* Split into user and encrypted password. */
        cryp = strchr( line, ':' );
        if ( cryp == (char*) 0 )
            continue;
        *cryp++ = '\0';
        /* Is this the right user? */
        if ( strcmp( line, authinfo ) == 0 )
	    {
            /* Yes. */
            (void) fclose( fp );
            /* So is the password right? */
            if ( strcmp( crypt( authpass, cryp ), cryp ) == 0 )
            {
                /* Ok! */
                remoteuser = line;
                return;
            }
            else
            /* No. */
                send_authenticate( dirname );
	    }
	}
    
    /* Didn't find that user.  Access denied. */
    (void) fclose( fp );
    send_authenticate( dirname );
}

static void send_authenticate( char* realm )
{
    char header[10000];    
    (void) snprintf(header, sizeof(header), "WWW-Authenticate: Basic realm=\"%s\"", realm );
    send_error( 401, "Unauthorized", header, "Authorization required." );
}

static int hexit( char c )
{
    if ( c >= '0' && c <= '9' )
        return c - '0';
    if ( c >= 'a' && c <= 'f' )
        return c - 'a' + 10;
    if ( c >= 'A' && c <= 'F' )
        return c - 'A' + 10;
    return 0;           /* shouldn't happen, we're guarded by isxdigit() */
}

static int b64_decode_table[256] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 00-0F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 10-1F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,  /* 20-2F */
    52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,  /* 30-3F */
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,  /* 40-4F */
    15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,  /* 50-5F */
    -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,  /* 60-6F */
    41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,  /* 70-7F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 80-8F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 90-9F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* A0-AF */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* B0-BF */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* C0-CF */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* D0-DF */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* E0-EF */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1   /* F0-FF */
};

static int b64_decode( const char* str, unsigned char* space, int size )
{
    const char* cp;
    int space_idx, phase;
    int d, prev_d = 0;
    unsigned char c;
    
    space_idx = 0;
    phase = 0;
    for ( cp = str; *cp != '\0'; ++cp )
	{
        d = b64_decode_table[(int) *cp];
        if ( d != -1 )
	    {
            switch ( phase )
            {
                case 0:
                    ++phase;
                    break;
                case 1:
                    c = ( ( prev_d << 2 ) | ( ( d & 0x30 ) >> 4 ) );
                    if ( space_idx < size )
                        space[space_idx++] = c;
                    ++phase;
                    break;
                case 2:
                    c = ( ( ( prev_d & 0xf ) << 4 ) | ( ( d & 0x3c ) >> 2 ) );
                    if ( space_idx < size )
                        space[space_idx++] = c;
                    ++phase;
                    break;
                case 3:
                    c = ( ( ( prev_d & 0x03 ) << 6 ) | d );
                    if ( space_idx < size )
                        space[space_idx++] = c;
                    phase = 0;
                    break;
            }
            prev_d = d;
	    }
	}
    return space_idx;
}

static char* ntoa( usockaddr* usaP )
{    
    return inet_ntoa( usaP->sa_in.sin_addr );
}

static const char* figure_mime( char* name, char* me, size_t me_size )
{
    char* prev_dot;
    char* dot;
    char* ext;
    int me_indexes[100], n_me_indexes;
    size_t ext_len, me_len;
    int i, top, bot, mid;
    int r;
    const char* default_type = "text/plain; charset=%s";
    const char* type;
    
    /* Peel off encoding extensions until there aren't any more. */
    n_me_indexes = 0;
    for ( prev_dot = &name[strlen(name)]; ; prev_dot = dot )
	{
        for ( dot = prev_dot - 1; dot >= name && *dot != '.'; --dot );
        if ( dot < name )
	    {
            /* No dot found.  No more encoding extensions, and no type
             ** extension either.
             */
            type = default_type;
            goto done;
	    }
        ext = dot + 1;
        ext_len = prev_dot - ext;
        /* Search the encodings table.  Linear search is fine here, there
         ** are only a few entries.
         */
        for ( i = 0; i < n_enc_tab; ++i )
	    {
            if ( ext_len == enc_tab[i].ext_len && strncasecmp( ext, enc_tab[i].ext, ext_len ) == 0 )
            {
                if ( n_me_indexes < sizeof(me_indexes)/sizeof(*me_indexes) )
                {
                    me_indexes[n_me_indexes] = i;
                    ++n_me_indexes;
                }
                goto next;
            }
	    }
        /* No encoding extension found.  Break and look for a type extension. */
        break;
        
	next: ;
	}
    
    /* Binary search for a matching type extension. */
    top = n_typ_tab - 1;
    bot = 0;
    while ( top >= bot )
	{
        mid = ( top + bot ) / 2;
        r = strncasecmp( ext, typ_tab[mid].ext, ext_len );
        if ( r < 0 )
            top = mid - 1;
        else if ( r > 0 )
            bot = mid + 1;
        else
            if ( ext_len < typ_tab[mid].ext_len )
                top = mid - 1;
            else if ( ext_len > typ_tab[mid].ext_len )
                bot = mid + 1;
            else
            {
                type = typ_tab[mid].val;
                goto done;
            }
	}
    type = default_type;
    
done:
    
    /* The last thing we do is actually generate the mime-encoding header. */
    me[0] = '\0';
    me_len = 0;
    for ( i = n_me_indexes - 1; i >= 0; --i )
	{
        if ( me_len + enc_tab[me_indexes[i]].val_len + 1 < me_size )
	    {
            if ( me[0] != '\0' )
            {
                (void) strcpy( &me[me_len], "," );
                ++me_len;
            }
            (void) strcpy( &me[me_len], enc_tab[me_indexes[i]].val );
            me_len += enc_tab[me_indexes[i]].val_len;
	    }
	}
    
    return type;
}

static void send_via_write( int fd, off_t size )
{
    if ( size <= SIZE_T_MAX )
	{
        size_t size_size = (size_t) size;
        void* ptr = mmap( 0, size_size, PROT_READ, MAP_PRIVATE, fd, 0 );
         fprintf(stderr, "errno = %d\n",errno);
        if ( ptr != (void*) -1 )
	    {
            write( conn_fd, ptr, size_size );
            (void) munmap( ptr, size_size );
	    }
        (void) madvise( ptr, size_size, MADV_SEQUENTIAL );
	}
    else
	{
        /* mmap can't deal with files larger than 2GB. */
        char buf[30000];
        ssize_t r, r2;
        
        for (;;)
	    {
            r = read( fd, buf, sizeof(buf) );
            if ( r < 0 && ( errno == EINTR || errno == EAGAIN ) )
            {
                sleep( 1 );
                continue;
            }
            if ( r <= 0 )
                return;
            for (;;)
            {
                r2 = write( conn_fd, buf, r);
                if ( r2 < 0 && ( errno == EINTR || errno == EAGAIN ) )
                {
                    sleep( 1 );
                    continue;
                }
                if ( r2 != r )
                    return;
                break;
            }
	    }
	}
}

static void do_dir( void )
{
    char buf[10000];
    size_t buflen;
    char* contents;
    size_t contents_size, contents_len;

    char command[10000];
    FILE* fp;
    
    if ( pathinfo != (char*) 0 )
        send_error( 404, "Not Found", "", "File not found." );
    
    /* Check authorization for this directory. */
    auth_check( file );
    
    contents_size = 0;
    buflen = snprintf( buf, sizeof(buf), "\
                      <HTML>\n\
                      <HEAD><TITLE>Index of %s</TITLE></HEAD>\n\
                      <BODY BGCOLOR=\"#99cc99\" TEXT=\"#000000\" LINK=\"#2020ff\" VLINK=\"#4040cc\">\n\
                      <H4>Index of %s</H4>\n\
                      <PRE>\n",
                      file, file );
    add_to_buf( &contents, &contents_size, &contents_len, buf, buflen );
    
    if ( strchr( file, '\'' ) == (char*) 0 )
	{
        (void) snprintf(
                        command, sizeof(command),
                        "ls -lgF '%s' | tail +2 | sed -e 's/^\\([^ ][^ ]*\\)\\(  *[^ ][^ ]*  *[^ ][^ ]*  *[^ ][^ ]*\\)\\(  *[^ ][^ ]*\\)  *\\([^ ][^ ]*  *[^ ][^ ]*  *[^ ][^ ]*\\)  *\\(.*\\)$/\\1 \\3  \\4  |\\5/' -e '/ -> /!s,|\\([^*]*\\)$,|<A HREF=\"\\1\">\\1</A>,' -e '/ -> /!s,|\\(.*\\)\\([*]\\)$,|<A HREF=\"\\1\">\\1</A>\\2,' -e '/ -> /s,|\\([^@]*\\)\\(@* -> \\),|<A HREF=\"\\1\">\\1</A>\\2,' -e 's/|//'",
                        file );
        fp = popen( command, "r" );
        for (;;)
	    {
            size_t r;
            r = fread( buf, 1, sizeof(buf), fp );
            if ( r == 0 )
                break;
            add_to_buf( &contents, &contents_size, &contents_len, buf, r );
	    }
        (void) pclose( fp );
	}
    
    buflen = snprintf( buf, sizeof(buf), "\
                      </PRE>\n\
                      <HR>\n\
                      <ADDRESS><A HREF=\"%s\">%s</A></ADDRESS>\n\
                      </BODY>\n\
                      </HTML>\n",
                      SERVER_URL, SERVER_SOFTWARE );
    add_to_buf( &contents, &contents_size, &contents_len, buf, buflen );
    
    add_headers( 200, "Ok", "", "", "text/html; charset=%s", contents_len, sb.st_mtime );
    if ( method != METHOD_HEAD )
        add_to_response( contents, contents_len );
    send_response();
}
