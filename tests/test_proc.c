#include "unity.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

void setUp(void) {}
void tearDown(void) {}

static void test_die(void)
{
	pid_t pid = fork();
	int st = 0;

	TEST_ASSERT_TRUE(pid >= 0);
	if (pid == 0) {
		freopen("/dev/null", "w", stderr);
		die("boom");
	}
	TEST_ASSERT_TRUE(waitpid(pid, &st, 0) > 0);
	TEST_ASSERT_TRUE(WIFEXITED(st));
	TEST_ASSERT_EQUAL_INT(1, WEXITSTATUS(st));
}

static void test_spawn(void)
{
	char *ok[] = {"true", NULL};
	char *bad[] = {"false", NULL};
	char *missing[] = {"/bin/no-such-portfolio-cmd", NULL};
	char *echo[] = {"echo", "hello", NULL};
	char *dd[] = {"dd", "if=/dev/zero", "bs=20000", "count=1", "status=none", NULL};
	char *out = NULL;
	size_t n = 0;
	strvec *v;

	TEST_ASSERT_EQUAL_INT(-1, spawn_wait(NULL));
	TEST_ASSERT_EQUAL_INT(0, spawn_wait(ok));
	TEST_ASSERT_EQUAL_INT(-1, spawn_wait(bad));
	TEST_ASSERT_EQUAL_INT(-1, spawn_wait(missing));
	TEST_ASSERT_EQUAL_INT(-1, spawn_capture(NULL, &out, NULL));
	TEST_ASSERT_EQUAL_INT(0, spawn_capture(echo, &out, &n));
	TEST_ASSERT_EQUAL_UINT(6, n);
	TEST_ASSERT_EQUAL_STRING("hello\n", out);
	free(out);
	out = NULL;
	TEST_ASSERT_EQUAL_INT(-1, spawn_capture(bad, &out, NULL));
	free(out);
	out = NULL;
	n = 0;
	TEST_ASSERT_EQUAL_INT(0, spawn_capture(dd, &out, &n));
	TEST_ASSERT_EQUAL_UINT(20000, n);
	free(out);

	v = strvec_new();
	TEST_ASSERT_EQUAL_UINT(0, strvec_len(v));
	strvec_sort(v);
	strvec_push(v, "true");
	strvec_sort(v);
	TEST_ASSERT_EQUAL_INT(0, strvec_spawn(v));
	strvec_free(v);
	v = strvec_new();
	strvec_push(v, "echo");
	strvec_push(v, "z");
	strvec_sort(v);
	out = NULL;
	TEST_ASSERT_EQUAL_INT(0, strvec_capture(v, &out, NULL));
	TEST_ASSERT_EQUAL_STRING("z\n", out);
	free(out);
	strvec_free(v);

	{
		char *none[] = {NULL};
		char *sig[] = {"sh", "-c", "kill -9 $$", NULL};

		out = NULL;
		TEST_ASSERT_EQUAL_INT(-1, spawn_wait(none));
		TEST_ASSERT_EQUAL_INT(-1, spawn_capture(none, &out, NULL));
		TEST_ASSERT_EQUAL_INT(-1, spawn_wait(sig));
		out = NULL;
		TEST_ASSERT_EQUAL_INT(-1, spawn_capture(sig, &out, NULL));
		free(out);
	}
}

static void test_run(void)
{
	char *out = NULL;
	size_t n = 0;

	TEST_ASSERT_EQUAL_INT(-1, run_cmd(NULL, NULL, &out, NULL));
	TEST_ASSERT_EQUAL_INT(-1, run_cmd("   ", NULL, &out, NULL));
	TEST_ASSERT_EQUAL_INT(0, run_cmd("echo hello", NULL, &out, &n));
	TEST_ASSERT_EQUAL_STRING("hello\n", out);
	TEST_ASSERT_EQUAL_UINT(6, n);
	free(out);
	TEST_ASSERT_EQUAL_INT(0, run_cmd("echo hello", "there", &out, NULL));
	TEST_ASSERT_EQUAL_STRING("hello there\n", out);
	free(out);
	out = NULL;
	TEST_ASSERT_EQUAL_INT(-1, run_cmd("false", NULL, &out, NULL));
	free(out);
	TEST_ASSERT_EQUAL_INT(0, run_cmd("true", NULL, NULL, NULL));
	out = NULL;
	TEST_ASSERT_EQUAL_INT(0, run_cmd("echo a b c d e f g", NULL, &out, NULL));
	TEST_ASSERT_EQUAL_STRING("a b c d e f g\n", out);
	free(out);
}

static void test_tags_blank(void)
{
	strvec *tags = NULL;

	TEST_ASSERT_EQUAL_INT(0, tags_collect("\t\n", &tags));
	TEST_ASSERT_EQUAL_UINT(0, strvec_len(tags));
	strvec_free(tags);
	TEST_ASSERT_EQUAL_INT(0, tags_collect("a\t\tb\n", &tags));
	TEST_ASSERT_EQUAL_UINT(0, strvec_len(tags));
	strvec_free(tags);
}

int main(void)
{
	UNITY_BEGIN();
	RUN_TEST(test_die);
	RUN_TEST(test_spawn);
	RUN_TEST(test_run);
	RUN_TEST(test_tags_blank);
	return UNITY_END();
}
