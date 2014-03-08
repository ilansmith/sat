#ifndef _UNIT_TEST_H_
#define _UNIT_TEST_H_

#define ARRAY_SZ(arr) (sizeof(arr) / sizeof(arr[0]))
typedef struct {
	char *description;
	char *known_issue;
	int (*func)(void);
	int disabled;
} test_t;

int p_comment(char *comment, ...);
int s_comment(char *comment, char *fmt, ...);
int unit_test(int argc, char *argv[], int test_number, test_t *unit_tests);

#endif

