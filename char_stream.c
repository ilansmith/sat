#include "char_stream.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/* memory management */
static cs_t *cs_context_alloc(cs_type_t type, void *stream,
	int(*stream_getc)(cs_t *cs), int(*stream_ungetc)(cs_t *cs),
	long(*stream_getpos)(cs_t *cs), int(*stream_setpos)(cs_t *cs, int n),
	void(*stream_rewind)(cs_t *cs))
{
	cs_t *cs;

	if (!(cs = calloc(1, sizeof(cs_t))))
		return NULL;

	cs->type = type;
	cs->stream_getc = stream_getc;
	cs->stream_ungetc = stream_ungetc;
	cs->stream_getpos = stream_getpos;
	cs->stream_setpos = stream_setpos;
	cs->stream_rewind = stream_rewind;
	cs->stream = stream;
	cs->c = EOF;
	return cs;
}

static void cs_context_free(cs_t *cs)
{
	free(cs);
}

/* string2char */
static int getc_string2char(cs_t *cs)
{
	if (!(cs->c = *cs->pos.spos))
		return cs->c = EOF;

	cs->pos.spos++;
	return cs->c;
}

static int ungetc_string2char(cs_t *cs)
{
	return cs->c = cs->c == EOF ? EOF : *--cs->pos.spos;
}

static long getpos_string2char(cs_t *cs)
{
	return (long)(cs->pos.spos - (char *)cs->stream);
}

static int setpos_string2char(cs_t *cs, int n)
{
	if (cs->pos.spos - n < (char *)(cs->stream) ||
		cs->pos.spos + n >= (char *)(cs->stream) +
		strlen((char *)(cs->stream))) {
		return -1;
	}
	cs->pos.spos += n;
	return 0;
}

static void rewind_string2char(cs_t *cs)
{
	cs->pos.spos = (char *)cs->stream;
	cs->c = EOF;
}

static cs_t *open_string2char(char *s)
{
	cs_t *cs;
	char * str;

	if (!(str = calloc(1, strlen(s) + sizeof(char)))) {
		fprintf(stderr, "could not allocate memory for: %s\n", s);
		return NULL;
	}

	strcpy(str, s);
	if (!(cs = cs_context_alloc(CS_STRING2CHAR, str, getc_string2char,
		ungetc_string2char, getpos_string2char, setpos_string2char,
		rewind_string2char))) {
		free(str);
		return NULL;
	}

	cs->pos.spos = str;
	return cs;
}

static void close_string2char(cs_t *cs)
{
	free(cs->stream);
	cs_context_free(cs);
}

/* file2char */
static int getc_file2char(cs_t *cs)
{
	if ((cs->c = fgetc((FILE *)cs->stream)) != EOF)
		cs->pos.fpos++;

	return cs->c;
}

static int ungetc_file2char(cs_t* cs)
{
	if (!cs->pos.fpos)
		return EOF;

	cs->pos.fpos--;
	return cs->c = ungetc(cs->c, (FILE *)cs->stream);
}

static long getpos_file2char(cs_t *cs)
{
	return cs->pos.fpos;
}

static int setpos_file2char(cs_t *cs, int n)
{
	if (fseek((FILE *)cs->stream, n, SEEK_CUR))
		return -1;
	cs->pos.fpos += n;
	return 0;
}

static void rewind_file2char(cs_t *cs)
{
	rewind((FILE *)cs->stream);
	cs->pos.fpos = 0;
	cs->c = EOF;
}

static cs_t *open_file2char(char *file)
{
	cs_t *cs;
	FILE *f;

	if (!(f = fopen(file, "r"))) {
		fprintf(stderr, "could not open file: %s\n", file);
		return NULL;
	}

	if (!(cs = cs_context_alloc(CS_FILE2CHAR, f, getc_file2char,
		ungetc_file2char, getpos_file2char, setpos_file2char,
		rewind_file2char))) {
		fclose(f);
		return NULL;
	}

	cs->pos.fpos = 0;
	return cs;
}

static void close_file2char(cs_t *cs)
{
	fclose((FILE *)cs->stream);
	cs_context_free(cs);
}

/* stream functions */
int cs_getc(cs_t *cs)
{
	return cs->stream_getc(cs);
}

int cs_ungetc(cs_t *cs)
{
	return cs->stream_ungetc(cs);
}

long cs_getpos(cs_t *cs)
{
	return cs->stream_getpos(cs);
}

int cs_setpos(cs_t *cs, int n)
{
	return cs->stream_setpos(cs, n);
}

void cs_rewind(cs_t *cs)
{
	cs->stream_rewind(cs);
}

cs_t *cs_open(cs_type_t type, char *data)
{
	cs_t *cs;

	switch (type)
	{
	case CS_STRING2CHAR:
		cs = open_string2char((char *)data);
		break;
	case CS_FILE2CHAR:
		cs = open_file2char((char *)data);
		break;
	default:
		cs = NULL;
		break;
	}

	return cs;
}

void cs_close(cs_t *cs)
{
	switch (cs->type)
	{
	case CS_STRING2CHAR:
		close_string2char(cs);
		break;
	case CS_FILE2CHAR:
		close_file2char(cs);
		break;
	default:
		break;
	}
}

