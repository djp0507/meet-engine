#include "libass/ass.h"
#include <string.h>
#include <stdlib.h>

void ass_msg(ASS_Library *priv, int lvl, char *fmt, ...)
{

}
char parse_bool(char *str)
{
    while (*str == ' ' || *str == '\t')
        str++;
    if (!strncasecmp(str, "yes", 3))
        return 1;
    else if (strtol(str, NULL, 10) > 0)
        return 1;
    return 0;
}
double ass_strtod(const char *string, char **endPtr)
{
    return atof(string);
}
int lookup_style(ASS_Track *track, char *name)
{
    int i;
    if (*name == '*')
        ++name;                 // FIXME: what does '*' really mean ?
    for (i = track->n_styles - 1; i >= 0; --i) {
        if (strcmp(track->styles[i].Name, name) == 0)
            return i;
    }
    i = track->default_style;
    return i;                   // use the first style
}

int mystrtou32(char **p, int base, uint32_t *res)
{
    char *start = *p;
    *res = strtol(*p, p, base);
    if (*p != start)
        return 1;
    else
        return 0;
}

int strtocolor(ASS_Library *library, char **q, uint32_t *res, int hex)
{
    uint32_t color = 0;
    int result;
    char *p = *q;
    int base = hex ? 16 : 10;

    if (*p == '&')
        ++p;
    else {
        // ass_msg(library, MSGL_DBG2, "suspicious color format: \"%s\"\n", p);
    }

    if (*p == 'H' || *p == 'h') {
        ++p;
        result = mystrtou32(&p, 16, &color);
    } else {
        result = mystrtou32(&p, base, &color);
    }

    {
        unsigned char *tmp = (unsigned char *) (&color);
        unsigned char b;
        b = tmp[0];
        tmp[0] = tmp[3];
        tmp[3] = b;
        b = tmp[1];
        tmp[1] = tmp[2];
        tmp[2] = b;
    }
    if (*p == '&')
        ++p;
    *q = p;

    *res = color;
    return result;
}
