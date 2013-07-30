#include "config.h"
#include "libass/ass.h"
#include "ass_library.h"
#include <string.h>
#include <stdlib.h>

#ifdef CONFIG_ENCA
#include <enca.h>
#endif

#define MSGL_FATAL 0
#define MSGL_ERR 1
#define MSGL_WARN 2
#define MSGL_INFO 4
#define MSGL_V 6
#define MSGL_DBG2 7

void ass_msg(ASS_Library *priv, int lvl, char *fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    priv->msg_callback(lvl, fmt, va, priv->msg_callback_data);
    va_end(va);
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

#ifdef CONFIG_ENCA
void *ass_guess_buffer_cp(ASS_Library *library, unsigned char *buffer,
                          int buflen, char *preferred_language,
                          char *fallback)
{
    const char **languages;
    size_t langcnt;
    EncaAnalyser analyser;
    EncaEncoding encoding;
    char *detected_sub_cp = NULL;
    int i;

    languages = enca_get_languages(&langcnt);
    ass_msg(library, MSGL_V, "ENCA supported languages");
    for (i = 0; i < langcnt; i++) {
        ass_msg(library, MSGL_V, "lang %s", languages[i]);
    }

    for (i = 0; i < langcnt; i++) {
        const char *tmp;

        if (strcasecmp(languages[i], preferred_language) != 0)
            continue;
        analyser = enca_analyser_alloc(languages[i]);
        encoding = enca_analyse_const(analyser, buffer, buflen);
        tmp = enca_charset_name(encoding.charset, ENCA_NAME_STYLE_ICONV);
        if (tmp && encoding.charset != ENCA_CS_UNKNOWN) {
            detected_sub_cp = strdup(tmp);
            ass_msg(library, MSGL_INFO, "ENCA detected charset: %s", tmp);
        }
        enca_analyser_free(analyser);
    }

    free(languages);

    if (!detected_sub_cp) {
        detected_sub_cp = strdup(fallback);
        ass_msg(library, MSGL_INFO,
            "ENCA detection failed: fallback to %s", fallback);
    }

    return detected_sub_cp;
}
#endif
