#include "unit_test.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/select.h>
#include <stdarg.h>

#define C_CYAN "\033[01;36m"
#define C_RED "\033[01;31m"
#define C_GREEN "\033[01;32m"
#define C_BLUE "\033[01;34m"
#define C_GREY "\033[00;37m"
#define C_NORMAL "\033[00;00;00m"
#define C_HIGHLIGHT "\033[01m"
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX_APP_NAME_SZ 256

/* io functionality */
typedef int (*vio_t)(const char *format, va_list ap);
int vscanf(const char *format, va_list ap);

static int ask_user, all_tests;

static int vio_colour(vio_t vfunc, char *colour, char *fmt, va_list va)
{
	int ret;

	if (!colour)
		colour = C_NORMAL;

	ret = printf("%s", colour);
	ret += vfunc(fmt, va);
	ret += printf("%s", C_NORMAL);
	fflush(stdout);

	return ret;
}

static int p_colour(char *colour, char *fmt, ...)
{
	int ret;
	va_list va;

	va_start(va, fmt);
	ret = vio_colour(vprintf, colour, fmt, va);
	va_end(va);

	return ret;
}

static int io_init(void)
{
	return p_colour(C_GREY, "> ");
}

int p_comment(char *comment, ...)
{
	int ret;
	va_list va;

	ret = io_init();
	va_start(va, comment);
	ret += vio_colour(vprintf, C_GREY, comment, va);
	ret += p_colour(C_NORMAL, "\n");
	va_end(va);

	return ret;
}

static int to_vscanf(char *fmt, va_list va)
{
	fd_set fdr;
	struct timeval tv;
	int ret = 0, i, timeout = 10;

	for (i = 0; i < timeout; i++) {
		ret += vio_colour(vprintf, C_GREY, ".", NULL);

		tv.tv_sec = 0;
		tv.tv_usec = 500000;
		FD_ZERO(&fdr);
		FD_SET(0, &fdr);
		if (select(1, &fdr, NULL, NULL, &tv) || FD_ISSET(0, &fdr))
			break;
	}

	return (i == timeout) ? 0 : ret + vio_colour(vscanf, C_GREY, fmt, va);
}

int s_comment(char *comment, char *fmt, ...)
{
	int ret, scn;
	va_list va;

	ret = io_init();
	va_start(va, fmt);
	ret += vio_colour(vprintf, C_GREY, comment, NULL);
	ret += (scn = to_vscanf(fmt, va)) ? scn : printf("\n");

	va_end(va);

	return ret;
}

static void p_test_summery(int total, int passed, int failed, int known_issues,
	int disabled)
{
#define SUMMERY_FMT "%-14s%i\n"

	printf("\ntest summery\n------------\n");
	printf("%s"SUMMERY_FMT"%s", C_HIGHLIGHT, "total", total, C_NORMAL);
	printf(SUMMERY_FMT, "passed", passed);
	printf(SUMMERY_FMT, "failed", failed);
	printf(SUMMERY_FMT, "known issues", known_issues);
	printf(SUMMERY_FMT, "disabled", disabled);

#undef SUMMERY_FMT
}

static char *app_name(char *argv0)
{
	char *name, *ptr;
	static char path[MAX_APP_NAME_SZ];

	snprintf(path, MAX_APP_NAME_SZ, "%s", argv0);
	for (name = ptr = path; *ptr; ptr++) {
		if (*ptr != '/')
			continue;

		name = ptr + 1;
	}
	return name;
}

static void test_usage(char *path)
{
	char *app = app_name(path);

	printf("usage:\n"
		"%s               - run all tests\n"
		"  or\n"
		"%s <test>        - run a specific test\n"
		"  or\n"
		"%s <from> <to>   - run a range of tests\n"
		"  or\n"
		"%s list          - list all tests\n",
		app, app, app, app);
}

static int test_getarg(char *arg, int *arg_ival, int min, int max)
{
	char *err;

	*arg_ival = strtol(arg, &err, 10);
	if (*err)
		return -1;
	if (*arg_ival < min || *arg_ival > max) {
		printf("test number out of range: %i\n", *arg_ival);
		return -1;
	}
	return 0;
}

static int test_getargs(int argc, char *argv[], int *from, int *to, int max)
{
	if (argc > 3) {
		test_usage(argv[0]);
		return -1;
	}

	if (argc == 1) {
		*from = 0;
		*to = max;
		ask_user = 1;
		return 0;
	}

	/* 2 <= argc <= 3*/
	if (test_getarg(argv[1], from, 1, max)) {
		test_usage(argv[0]);
		return -1;
	}

	if (argc == 2) {
		*to = *from;
	}
	else { /* argc == 3 */
		if (test_getarg(argv[2], to, *from, max)) {
			test_usage(argv[0]);
			return -1;
		}
	}

	(*from)--; /* map test number to table index */
	return 0;
}

static int is_list_tests(int argc, char *argv[], int test_number,
		test_t *unit_tests)
{
	int i;

	if (argc != 2 || strcmp(argv[1], "list"))
		return 0;

	p_colour(C_HIGHLIGHT, "%s unit tests\n", app_name(argv[0]));
	for (i = 0; i < test_number - 1; i++) {
		test_t *t = &unit_tests[i];

		printf("%i. ", i + 1);
		p_colour(!all_tests && t->disabled ? C_GREY : C_NORMAL, "%s",
			t->description);
		if (!all_tests && t->disabled)
			p_colour(C_CYAN, " (disabled)");
		if (!t->disabled && t->known_issue) {
			p_colour(C_BLUE, " (known issue: ");
			p_colour(C_GREY, t->known_issue);
			p_colour(C_BLUE, ")");
		}
		printf("\n");
	}

	return 1;
}

int unit_test(int argc, char *argv[], int test_number, test_t *unit_tests)
{
	test_t *t;
	int from, to, max = test_number, ret;
	int total = 0, disabled = 0, passed = 0, failed = 0, known_issues = 0;

#if defined(ALL_TESTS)
	all_tests = 1;
#endif

	if (is_list_tests(argc, argv, test_number, unit_tests))
		return 0;

	if (test_getargs(argc, argv, &from, &to, max - 1))
		return -1;

	for (t = &unit_tests[from]; t < unit_tests + MIN(to, max); t++) {
		total++;
		printf("%i. %s:", from + total, t->description);
		if (!all_tests && t->disabled) {
			disabled++;
			p_colour(C_CYAN, " disabled\n");
			continue;
		}
		if (t->known_issue) {
			p_colour(C_BLUE, " known issue: ");
			p_colour(C_NORMAL, "%s\n", t->known_issue);
			known_issues++;
			continue;
		}
		if (!t->func) {
			p_colour(C_CYAN, " function does not exist\n");
			return -1;
		}
		printf("\n");
		fflush(stdout);

		if ((ret = t->func())) {
			p_colour(C_RED, "Failed");
			failed++;
		}
		else {
			p_colour(C_GREEN, "OK");
			passed++;
		}
		printf("\n");
	}

	p_test_summery(total, passed, failed, known_issues, disabled);
	return 0;
}

