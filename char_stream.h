#ifndef _CHAR_STREAM_H_
#define _CHAR_STREAM_H_

typedef enum cs_type_t {
    CS_STRING2CHAR = 0,
    CS_FILE2CHAR = 1,
} cs_type_t;

typedef struct cs_t {
    cs_type_t type;
    int (*stream_getc)(struct cs_t *cs);
    int (*stream_ungetc)(struct cs_t *cs);
    long (*stream_getpos)(struct cs_t *cs);
    int (*stream_setpos)(struct cs_t *cs, int n);
    void (*stream_rewind)(struct cs_t *cs);
    void *stream;
    int c;
    union {
	char *spos;
	long fpos;
    } pos;
} cs_t;

cs_t *cs_open(cs_type_t type, char *data);
void cs_close(cs_t *cs);
int cs_getc(cs_t *cs);
int cs_ungetc(cs_t *cs);
long cs_getpos(cs_t *cs);
int cs_setpos(cs_t *cs, int n);
void cs_rewind(cs_t *cs);

#endif
